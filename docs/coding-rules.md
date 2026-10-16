# Coding rules

These are the rules every line of Timelapse Switch follows. They exist for one reason: **a plugin runs inside OBS, so a bug in our code can freeze OBS or lose the recording a creator just spent two hours on.** The rules make that kind of bug hard to write and easy to catch.

Think of them like a seat belt: a little uncomfortable at first, then you stop noticing them.

**How to read this document**

- **Part A** adapts "The Power of Ten" by Gerard J. Holzmann (NASA/JPL Laboratory for Reliable Software), ten rules written for safety critical C code. Each rule shows the original idea, our version, why it matters here, how it is checked, and an example.
- **Part B** covers what P10 does not: threads, OBS object lifetimes, errors, readability, naming, C++ and Qt.
- **Part C** is the review checklist and the list of approved exceptions.

Where a rule says "checked by `tools/check.sh`", a machine verifies it on every run. Where it says "review", a person does. Holzmann's own point is that rules nobody can check end up ignored, so we prefer machine checks.

**Where the rules apply**

| Code | Location | How strictly |
|---|---|---|
| Core logic, in C17 | `src/core/`, `src/plugin-*.c` | All rules, fully |
| Dock and dialogs, in C++17 with Qt | `src/ui/` | All rules, with the C++ adaptations marked "C++" |
| Tests | `tests/` | Readability, error checking and zero warnings; the rest is relaxed |
| Shell scripts | `tools/` | [Google Shell Style Guide](https://google.github.io/styleguide/shellguide.html), see B7 |

---

## Part A. The Power of Ten, adapted

### P10-1. Simple control flow

> **Holzmann:** "Restrict all code to very simple control flow constructs: do not use goto statements, setjmp or longjmp constructs, and direct or indirect recursion."

**Our rule:** no `goto`, no `setjmp`/`longjmp` and no recursion, direct or indirect. Early `return` on errors is allowed and encouraged; Holzmann allows it too. **C++:** we never throw exceptions, and nothing may escape into OBS.

**Why here:** OBS's own code uses `goto fail` for cleanup. We don't, because a function you can read top to bottom, without jumps, is a function a reviewer can check. With no recursion, the call graph has no cycles, so every call chain ends.

**Instead of `goto` cleanup:** write a `destroy` function that accepts a half-built object (B2). On failure, call it and return.

**Checked by:** `tools/check.sh` (text check for `goto`, `setjmp` and `longjmp`) and clang-tidy (`cppcoreguidelines-avoid-goto`, `misc-no-recursion`).

```c
/* Yes: early return, cleanup in one place. */
struct tsw_valve *valve = tsw_valve_create(&config);
if (!valve)
	return TSW_E_START_FAILED;
```

### P10-2. Every loop has a fixed upper bound

> **Holzmann:** "All loops must have a fixed upper-bound. It must be trivially possible for a checking tool to prove statically that a preset upper-bound on the number of iterations of a loop cannot be exceeded."

**Our rule:** every loop states its limit with a named constant (`TSW_MAX_...`) or the size of a fixed array. A loop whose length depends on data, such as draining a queue, also carries a maximum. If it reaches that maximum, it reports a failed check (P10-5) and returns an error. Loops that must never end belong to OBS's threads, not to ours. We don't create threads unless an ADR in `docs/decisions.md` justifies one.

**Why here:** a loop that never ends inside a video callback freezes OBS's video for good. A bounded loop can only be slow, never stuck.

**Checked by:** review.

```c
/* The queue can never hold more than TSW_MAX_PENDING_CHUNKS, so neither can this loop. */
for (size_t i = 0; i < TSW_MAX_PENDING_CHUNKS && tsw_ring_has_data(ring); i++) {
	...
}
```

### P10-3. No memory allocation on the real-time path

> **Holzmann:** "Do not use dynamic memory allocation after initialization."

**Our rule:** we keep Holzmann's rule where it matters most, the real-time path. For us, "initialization" means **starting a recording**:
- Everything the video and audio callbacks need (frame buffers, ring buffers, counters) is allocated in `tsw_recording_start()` and freed in `tsw_recording_stop()`.
- While recording, those callbacks never allocate or free memory.
- Real-time code lives in files named `*-rt.c`, so the rule is easy to check.
- Outside the real-time path (settings, dock, start, stop), allocation is allowed through OBS's allocator (`bmalloc`, `bzalloc`, `bfree`). OBS counts its allocations and reports leaks when it closes.

**Why here:** `malloc` can take an unpredictable amount of time, and a video callback has about 33 ms per frame at 30 fps, shared with OBS. Memory sized at start also means a recording can't fail halfway because memory ran out.

**Checked by:** review. A text check that forbids allocation calls in `*-rt.c` files arrives in `tools/check.sh` with the first real-time file.

### P10-4. Functions fit on one page

> **Holzmann:** "No function should be longer than what can be printed on a single sheet of paper in a standard reference format with one line per statement and one line per declaration. Typically, this means no more than about 60 lines of code per function."

**Our rule:** at most 60 lines per function; we aim for 40. A function does one thing, and its name says what that thing is.

**Why here:** a function you can see whole on screen is a function you can understand whole, which is one of this project's promises: anyone can follow the flow step by step.

**Checked by:** clang-tidy (`readability-function-size`, `LineThreshold: 60`).

### P10-5. Check what must be true

> **Holzmann:** "The assertion density of the code should average to a minimum of two assertions per function. [...] Assertions must always be side-effect free and should be defined as Boolean tests. When an assertion fails, an explicit recovery action must be taken, e.g., by returning an error condition to the caller."

**Our rule:**
- Functions in `src/core/` average at least **two checks** of the form `TSW_CHECK(condition)`: parameters on entry, results, loop limits, invariants.
- A check never changes anything; it only tests.
- When a check fails, the code **recovers**. It returns an error and the recording stops cleanly and is saved. **It never aborts OBS**, in debug or release builds.
- `TSW_CHECK` records the failure (file, line and the text of the condition) in a small preallocated fault log that is safe to use from real-time threads. The UI thread then writes that log to the OBS log and shows the error code TSW-E901 in the dock.
- A check that can never fail, such as `TSW_CHECK(true)`, is noise and is not allowed.

**Why here:** Holzmann's numbers say unit tests find about one defect per 10 to 100 lines. Checks catch the ones tests miss, at the exact place they happen, and the user gets a saved file instead of a crash.

**Checked by:** review for now. `TSW_CHECK` and a density count in `tools/check.sh` (failing below 2.0 per function in `src/core/`) arrive with the first core module.

```c
enum tsw_result tsw_valve_set_speed(struct tsw_valve *valve, uint32_t speed)
{
	if (!TSW_CHECK(valve != NULL))
		return TSW_E_INTERNAL;
	if (!TSW_CHECK(speed >= 1 && speed <= TSW_MAX_SPEED))
		return TSW_E_INTERNAL;

	atomic_store(&valve->speed, speed);
	return TSW_OK;
}
```

### P10-6. Smallest possible scope

> **Holzmann:** "Data objects must be declared at the smallest possible level of scope."

**Our rule:**
- Declare a variable where it is first needed, inside the innermost block, and make it `const` when it does not change.
- Never reuse a variable for a second purpose.
- Functions used by only one file are `static`.
- There is exactly one global object, the plugin context, and it is documented where it is defined.

**Why here:** when a value is wrong, fewer places could have written it, so the bug is found sooner.

**Checked by:** the compiler (`-Wshadow`) and review.

### P10-7. Check every result, validate every parameter

> **Holzmann:** "The return value of non-void functions must be checked by each calling function, and the validity of parameters must be checked inside each function."

**Our rule:**
- Every result is checked. That includes OBS functions (`obs_output_start`, `video_output_open`, ...), the C library and our own functions.
- Our functions that return a result are marked `TSW_NODISCARD`, so the compiler complains when a caller ignores it.
- If ignoring a result is really correct, cast it to `(void)` and add a comment that says why.
- Parameters are validated on entry with `TSW_CHECK`, which counts toward P10-5.

**Why here:** most "it just stopped recording" bugs start with a call that failed silently.

**Checked by:** the compiler (`warn_unused_result` through `TSW_NODISCARD`) and clang-tidy (`bugprone-unused-return-value`, `cert-err33-c`).

```c
(void)os_sleep_ms(10); /* Waking up early is harmless here: the loop re-checks the time. */
```

### P10-8. Keep the preprocessor simple

> **Holzmann:** "The use of the preprocessor must be limited to the inclusion of header files and simple macro definitions. Token pasting, variable argument lists (ellipses), and recursive macro calls are not allowed. All macros must expand into complete syntactic units."

**Our rule:**
- `#include`, `#pragma once`, and a few macros that each expand into one complete expression or attribute. Use `enum` or `static const` for constants instead of `#define`.
- No token pasting (`##`), no variadic macros (`__VA_ARGS__`), no recursive macros.
- **Conditional compilation lives in one file only: `src/tsw-compiler.h`**, with a justification for each use. The single exception is the standard `#ifdef __cplusplus` / `extern "C"` boilerplate in headers.
- Differences between operating systems are handled by **CMake choosing a different source file** (`src/platform/linux.c`, `windows.c`, `macos.c`), never with `#ifdef` inside a function.

**Why here:** Holzmann notes that ten `#ifdef`s can create up to 1024 different programs, each of which would need testing. We build for three systems, so we keep it to exactly three.

**Checked by:** `tools/check.sh` (text checks for `##`, `__VA_ARGS__` and conditionals outside `tsw-compiler.h`) and clang-tidy (`cppcoreguidelines-macro-usage`: only `TSW_` and `PLUGIN_` macros are allowed).

### P10-9. Restrict pointers

> **Holzmann:** "No more than one level of dereferencing is allowed. Pointer dereference operations may not be hidden in macro definitions or inside typedef declarations. Function pointers are not permitted."

**Our rule:**
- **One dereference per expression.** Instead of `a->b->c`, write `const struct tsw_b *b = a->b;` and then `b->c`. This also gives you a named place to check for `NULL`.
- No data structures built from pointers to pointers. When OBS asks for one (`video_output_open(video_t **out, ...)`), pass the address of a local variable.
- No pointer hidden in a `typedef` or a macro.
- **Function pointers only at the boundary with OBS and Qt**, where their APIs require callbacks: output and encoder registration, raw video and audio callbacks, hotkeys, frontend events and Qt `connect`. All of them are registered in one file per side (`src/plugin-main.c` and `src/ui/dock.cpp`), and the table in `docs/architecture.md` lists every callback, who calls it and on which thread. That table is the "alternate guarantee" Holzmann asks for when function pointers can't be avoided: it lets a reader follow the flow of control.

**Why here:** callbacks are how OBS talks to plugins, so we can't avoid them. We can keep them few, in one place and documented.

**Checked by:** review (the callback table must match the code).

### P10-10. Zero warnings, two analyzers, from day one

> **Holzmann:** "All code must be compiled, from the first day of development, with all compiler warnings enabled at the compiler's most pedantic setting. All code must compile with these setting without any warnings. All code must be checked daily with at least one, but preferably more than one, state-of-the-art static source code analyzer and should pass the analyses with zero warnings."

**Our rule:**
- **Warnings.** `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wformat=2 -Wcast-qual -Wnull-dereference -Wimplicit-fallthrough -Wundef` and more, with `-Werror`; `/W4 /WX` on MSVC. They apply to our code only; OBS and Qt headers are system includes.
- **Two static analyzers:** clang-tidy (`.clang-tidy`) and the GCC analyzer (`gcc -fanalyzer`).
- **Zero warnings.** If a tool is confused by correct code, we **rewrite the code** until it is obviously correct, as Holzmann recommends. Disabling a check is a last resort, needs a written justification in `.clang-tidy`, and is listed in Part C.
- **Always, not daily.** Everything runs in `tools/check.sh` before every commit and in CI on every push.

**Checked by:** `tools/check.sh` steps 2, 3 and 4, and CI.

---

## Part B. Rules P10 does not cover

### B1. Threads: who may do what

OBS calls our code from several threads. Each has a job and a budget.

```
   UI thread (Qt)            OBS video thread            OBS audio thread
   dock, settings,           raw video callback:         raw audio callback and our
   start, stop, logs         "does this frame pass?"     audio input: "how much passes?"
        |                          |                            |
        |   reads counters   <-----+---- atomics and ring ------+
        |   and faults             |     buffers (no locks)
        v                          v
   writes the OBS log        never: allocate, log, lock a busy mutex, touch disk or UI
```

| Thread | Our code there | May | Must not |
|---|---|---|---|
| UI thread (Qt main) | dock, dialogs, start and stop, frontend events | allocate, log, update widgets | block for more than about 50 ms |
| OBS video thread | raw video callback (`*-rt.c`) | copy into preallocated buffers, use atomics | allocate, log, wait on a lock, do I/O, call Qt |
| OBS audio thread | raw audio callback, our audio input (`*-rt.c`) | same as video | same as video |
| Encoder and output threads | none (OBS's code; we only configure it) | | |

The real-time threads talk to the UI only through atomics and single-producer, single-consumer ring buffers. The UI reads them with a timer, a few times per second.

### B2. Memory and OBS object lifetimes

- Every `obs_*_create`, `obs_*_get_ref` and `bmalloc` has its matching `release` or `bfree` **in the same module**, released in reverse order of creation, like taking off clothes in the reverse order you put them on.
- Each module has `create` and `destroy`. `destroy` accepts a half-built object, so a failed `create` just calls `destroy` and returns. This is what replaces `goto` cleanup (P10-1).
- A comment at the declaration says who owns each pointer: `/* Owned: released in tsw_recorder_destroy(). */` or `/* Borrowed from OBS: do not release. */`

### B3. Errors, logs and what the user sees

Every failure produces three things:

1. **A result code** that the code returns (`enum tsw_result`). Each value maps to a public code `TSW-Exxx` from the catalogue in `docs/design/ux-spec.md`, section 4.3.
2. **A log line in English**, written from the UI thread, in this format:
   ```
   [timelapse-switch] TSW-E101 error | cause="no space left on device" free_mb=0 real=00:42:10.120 saved=true | action="free space or choose another folder"
   ```
   The log never gets one line per frame. During a recording, statistics are written as a summary once a minute.
3. **A message in the dock**, translated (English and Spanish), saying what happened, what is safe, why, and what to do, plus the code. Templates and examples are in `docs/design/ux-spec.md`, section 4.

The user is never left guessing: no silent failures, no error without a code, no code without a page in `docs/errors/`.

### B4. Readability and documentation inside the code

The goal is that someone who is not a C expert can follow what the code does, step by step.

- **Every file starts with a short block:** what this file does, and how it fits in the whole. Add an analogy when it helps, like the "tape recorder with a fast-forward pedal".
- **Every function declaration has a compact contract:** what it does, its parameters (with units), what it returns, which thread may call it, and whether it allocates. See `src/plugin-support.h` for the format.
- **Functions longer than about 15 lines are written as a recipe:** numbered comments (`/* 1. ... */`, `/* 2. ... */`) so the steps can be read before the code.
- **Comments explain why, not what.** The code already says what.
- **Names are full words, with units:** `duration_ns`, `buffer_size_bytes`, `frames_to_skip`. Booleans read as questions: `is_recording`, `has_audio`.
- **No magic numbers.** Every constant has a name and a one-line reason.
- **DRY without guessing:** when the same logic appears a second time, extract it. Don't build abstractions for needs that don't exist yet (parsimony).
- **No em dashes or en dashes**, in code or docs. Use a period, comma, colon or parentheses. `tools/check.sh` checks this.

### B5. Naming and formatting

| Thing | Convention | Example |
|---|---|---|
| Files | kebab-case | `src/core/valve-rt.c` |
| C functions | `tsw_<module>_<verb>` | `tsw_valve_set_speed` |
| C types | `struct tsw_<name>`, `enum tsw_<name>` | `struct tsw_valve` |
| Enum values and macros | `TSW_` in capitals | `TSW_E_DISK_FULL`, `TSW_CHECK` |
| C++ classes, methods | Qt style: `PascalCase` classes, `camelCase` methods | `TimelapseDock::showError` |
| Locale keys | `PascalCase.Dotted` | `Dock.Button.Record` |

Formatting is never discussed in review: `clang-format` 19.1.1 (the OBS style: tabs, 120 columns) and `gersemi` 0.21.0 for CMake decide, and `tools/check.sh` verifies.

### B6. C++ and Qt

- Qt objects are owned by their parent widget; never write `new` without a parent or a smart pointer.
- No exceptions are thrown, and no exception may reach OBS.
- Widgets are updated only on the UI thread. From other threads, use `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
- Every visible text comes from `obs_module_text("...")` and the files in `data/locale/`; nothing visible is hardcoded.
- Every interactive widget has an accessible name, for screen readers.

### B7. Shell scripts

Our scripts (`tools/*.sh`, `tools/git-hooks/*`) follow the [Google Shell Style Guide](https://google.github.io/styleguide/shellguide.html). In short:

- **Bash only:** first line `#!/bin/bash`, options with `set` (`set -u`, or `set -euo pipefail` for scripts that must stop at the first error).
- **Layout:** 2 spaces, no tabs; lines of 80 characters at most; `; then` and `; do` on the same line; a long pipeline is split with `|` at the start of each continuation line.
- **Comments:** a header saying what the script does and how to run it; a header block for every function that is not both short and obvious (description, globals, arguments, outputs, returns).
- **Names:** functions and variables in `lower_case`; constants in `UPPER_CASE`, declared at the top with `readonly`; variables inside functions declared `local`, on a separate line when the value comes from a command (`local version` then `version="$(...)"`), so the command's exit status is not lost.
- **Structure:** a script with functions puts them after the constants and ends with `main "$@"`.
- **Quoting and expansions:** always `"${var}"` (quoted, with braces), `"$@"` for arguments, arrays for lists, `$(...)` never backticks, `[[ ... ]]` for tests and `(( ... ))` for arithmetic.
- **Errors:** messages for the user go to stderr (`err "..."`); every command's result is checked.

**Checked by:** `tools/check.sh` step 5: ShellCheck (zero warnings), shfmt with the Google layout (`-i 2 -ci -bn`) and a check for lines over 80 characters. To fix the layout automatically:

```bash
.venv/bin/shfmt -w -i 2 -ci -bn tools/*.sh tools/git-hooks/*
```

Two kinds of script are out of scope:
- The scripts that come with the OBS template (zsh and PowerShell, in `.github/scripts/` and `build-aux/`) keep the template's own style, so they can be updated from upstream.
- The scripts keep their `.sh` extension, which the guide allows for executables, so their names stay recognizable.

---

## Part C. Review checklist and approved exceptions

### Review checklist

Copy this into each pull request:

- [ ] `tools/check.sh` passes, all green.
- [ ] Every loop has a visible bound (P10-2).
- [ ] No allocation in `*-rt.c` files (P10-3).
- [ ] Functions in `src/core/` check their parameters and results with `TSW_CHECK` (P10-5, P10-7).
- [ ] New callbacks are added to the table in `docs/architecture.md` (P10-9).
- [ ] Every new failure has a `TSW-Exxx` code, a log line and a dock message (B3).
- [ ] New files have the header block; new functions have a contract (B4).
- [ ] Requirement IDs are referenced in code, tests and the commit (`docs/quality.md`).

### Approved exceptions

A rule can only be broken with a reason, written in the code (`/* P10-x exception: reason */`) and listed here.

| Rule | Exception | Reason |
|---|---|---|
| P10-3 | Allocation outside the real-time path | Qt and the OBS settings API allocate by design. Holzmann's goal (no unpredictable allocation while running) is kept where it matters. |
| P10-8 | `#pragma once` and the `#ifdef __cplusplus` header boilerplate | Standard on every compiler we support, and needed so C++ can include our C headers. |
| P10-9 | Function pointers for OBS and Qt callbacks | Required by the OBS and Qt APIs; kept in one file per side and documented in a table. |
| P10-10 | clang-tidy check `DeprecatedOrUnsafeBufferHandling` disabled | It requires C11 Annex K functions that don't exist on Linux or macOS. We use the bounded standard functions and check their results. |

---

*Source: G. J. Holzmann, "The Power of Ten: Rules for Developing Safety Critical Code", NASA/JPL Laboratory for Reliable Software, https://spinroot.com/gerard/pdf/P10.pdf. Quotes are from that paper; the adaptations are ours.*
