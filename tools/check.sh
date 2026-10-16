#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Runs every automatic check of the project with one command (Linux).
#
#   tools/check.sh
#
# All steps run even if one fails, so every problem shows up at once, and a
# summary comes last. Exit status 0 means everything passed. The rules
# behind each step are in docs/coding-rules.md.
#
#   1. Format          clang-format and gersemi, same versions as the OBS CI
#   2. Build           pedantic warnings, any warning is an error   (P10-10)
#   3. Unit tests      CTest, the core logic tests                  (D1)
#   4. clang-tidy      static analyzer #1                           (P10-10)
#   5. GCC analyzer    static analyzer #2, gcc -fanalyzer           (P10-10)
#   6. Shell scripts   ShellCheck, shfmt and line length (Google style)
#   7. Project rules   checks no tool covers: P10-1, P10-8, no dashes,
#                      commit messages, version format

set -u

readonly RED=$'\e[31m'
readonly GREEN=$'\e[32m'
readonly BOLD=$'\e[1m'
readonly RESET=$'\e[0m'
readonly PRESET='ubuntu-x86_64'
readonly BUILD_DIR='build_x86_64'
readonly ANALYZER_DIR='build_analyzer'
readonly VENV_BIN='.venv/bin'
# Google Shell Style Guide layout for shfmt: indent with 2 spaces, indent
# case branches, and let "|" and "&&" start a continuation line.
readonly -a SHFMT_STYLE=(-i 2 -ci -bn)
readonly MAX_SHELL_LINE=80
readonly SEMVER='^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$'

summary=()
failed=0

#######################################
# Remembers the result of one check for the final summary.
# Globals:
#   summary, failed
# Arguments:
#   Name of the check.
#   Exit status of the check: 0 means it passed.
#######################################
record() {
  if (($2 == 0)); then
    summary+=("${GREEN}PASS${RESET}  $1")
  else
    summary+=("${RED}FAIL${RESET}  $1")
    failed=1
  fi
}

# Prints the title of a step.
step() {
  printf '\n%s== %s%s\n' "${BOLD}" "$1" "${RESET}"
}

# Prints a message to stderr.
err() {
  echo "$*" >&2
}

#######################################
# Fails a check if a pattern appears in any of the given files.
# Arguments:
#   Name of the check.
#   Extended regular expression.
#   Files to search.
# Outputs:
#   The matching lines, if any.
#######################################
rule_absent() {
  local name="$1"
  local pattern="$2"
  shift 2
  if grep -nE "${pattern}" "$@" 2>/dev/null; then
    record "${name}" 1
  else
    record "${name}" 0
  fi
}

#######################################
# Step 1: source formatting, with the pinned tool versions in .venv.
# Outputs:
#   Formatter messages; an install hint on stderr if tools are missing.
#######################################
check_format() {
  step '1. Format (clang-format 19.1.1, gersemi 0.21.0)'
  if [[ ! -x "${VENV_BIN}/clang-format" || ! -x "${VENV_BIN}/gersemi" ]]; then
    err 'Formatting tools missing. Install them once with:'
    err '  python3 -m venv .venv'
    err '  .venv/bin/pip install -r tools/requirements-dev.txt'
    record 'format (tools not installed)' 1
    return
  fi
  # The template's script checks src/ by default; tests/ follow the same style.
  local -a c_files
  mapfile -t c_files < <(git ls-files 'src/*.[ch]' 'src/*.[ch]pp' \
    'tests/*.[ch]' 'tests/*.[ch]pp')
  # --fail-error: without it the template's script always exits with 0.
  PATH="${PWD}/${VENV_BIN}:${PATH}" ./build-aux/run-clang-format \
    --fail-error --check "${c_files[@]}"
  record 'format: C and C++ (clang-format)' "$?"
  PATH="${PWD}/${VENV_BIN}:${PATH}" ./build-aux/run-gersemi \
    --fail-error --check
  record 'format: CMake (gersemi)' "$?"
}

# Step 2: build with the strict warnings set in CMakeLists.txt.
check_build() {
  step '2. Build with strict warnings'
  if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
    cmake --preset "${PRESET}" >/dev/null
  fi
  cmake --build "${BUILD_DIR}"
  record 'build (zero warnings)' "$?"
}

# Step 3: the unit tests of the core logic (docs/coding-rules.md, D1).
check_unit_tests() {
  step '3. Unit tests'
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
  record 'unit tests (ctest)' "$?"
}

#######################################
# Step 4: clang-tidy on our C and C++ sources. The OBS build adds a few
# GCC-only flags that clang-tidy (Clang) does not know; they are removed
# or silenced here.
#######################################
check_clang_tidy() {
  step '4. Static analysis: clang-tidy'
  local -a sources
  mapfile -t sources < <(git ls-files 'src/*.c' 'src/*.cpp')
  sources+=("${BUILD_DIR}/plugin-support.c")
  clang-tidy -p "${BUILD_DIR}" --quiet \
    --removed-arg=-mno-direct-extern-access \
    --extra-arg=-Wno-unknown-warning-option "${sources[@]}" 2>&1 \
    | grep -vE 'warnings? generated'
  record 'clang-tidy (zero warnings)' "${PIPESTATUS[0]}"
}

#######################################
# Step 5: GCC's static analyzer, in its own build directory so the normal
# build stays fast. Its findings are errors (-Werror).
#######################################
check_analyzer() {
  step '5. Static analysis: GCC analyzer'
  if [[ ! -f "${ANALYZER_DIR}/build.ninja" ]]; then
    cmake --preset "${PRESET}" -B "${ANALYZER_DIR}" \
      -DCMAKE_C_FLAGS=-fanalyzer >/dev/null
  fi
  cmake --build "${ANALYZER_DIR}"
  record 'gcc -fanalyzer (zero warnings)' "$?"
}

#######################################
# Step 6: our shell scripts follow the Google Shell Style Guide.
# ShellCheck finds bugs, shfmt checks the layout, awk checks line length.
# Outputs:
#   Problems found, with file and line.
#######################################
check_shell() {
  step '6. Shell scripts (Google Shell Style Guide)'
  if [[ ! -x "${VENV_BIN}/shellcheck" || ! -x "${VENV_BIN}/shfmt" ]]; then
    err 'ShellCheck or shfmt missing. Install them once with:'
    err '  .venv/bin/pip install -r tools/requirements-dev.txt'
    record 'shell scripts (tools not installed)' 1
    return
  fi
  local -a scripts
  mapfile -t scripts < <(git ls-files 'tools/*.sh' 'tools/git-hooks/*')
  "${VENV_BIN}/shellcheck" --shell=bash "${scripts[@]}"
  record 'shell: ShellCheck (zero warnings)' "$?"
  "${VENV_BIN}/shfmt" --diff "${SHFMT_STYLE[@]}" "${scripts[@]}"
  record 'shell: layout (shfmt)' "$?"
  local long_lines
  long_lines="$(awk -v max="${MAX_SHELL_LINE}" \
    'length > max { print FILENAME ":" FNR ": " length " characters" }' \
    "${scripts[@]}")"
  if [[ -n "${long_lines}" ]]; then
    echo "${long_lines}"
    record "shell: lines of ${MAX_SHELL_LINE} characters or less" 1
  else
    record "shell: lines of ${MAX_SHELL_LINE} characters or less" 0
  fi
}

# Step 7a: rules from docs/coding-rules.md that no tool checks.
check_code_rules() {
  local -a code
  mapfile -t code < <(git ls-files 'src/*.c' 'src/*.cpp' 'src/*.h' \
    'src/*.hpp' 'src/*.c.in')
  rule_absent 'P10-1: no goto, setjmp or longjmp' \
    '\bgoto\b|\bsetjmp\b|\blongjmp\b' "${code[@]}"
  rule_absent 'P10-8: no token pasting or variadic macros' \
    '##|__VA_ARGS__' "${code[@]}"
  # Conditional compilation lives only in src/tsw-compiler.h. The exception
  # is the "#ifdef __cplusplus" boilerplate that lets C++ include C headers.
  local -a others
  mapfile -t others < <(printf '%s\n' "${code[@]}" \
    | grep -v '^src/tsw-compiler.h$')
  if grep -nE '^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef|elif)\b' \
    "${others[@]}" | grep -v '__cplusplus'; then
    record 'P10-8: conditional compilation only in tsw-compiler.h' 1
  else
    record 'P10-8: conditional compilation only in tsw-compiler.h' 0
  fi
}

# Step 7b: no em dashes or en dashes in any tracked text file.
check_text_rules() {
  local -a text
  mapfile -t text < <(git ls-files \
    | grep -vE '^LICENSE$|\.(png|jpg|gif|ico|icns|webp)$')
  rule_absent 'Style: no em dashes or en dashes in any file' \
    $'\u2014|\u2013' "${text[@]}"
}

#######################################
# Step 7c: every commit in the history follows Conventional Commits,
# checked by the same hook Git runs before each commit.
# Outputs:
#   The commits that do not comply.
#######################################
check_commits() {
  local bad=0
  local sha
  while IFS= read -r sha; do
    if ! git log -1 --format=%B "${sha}" \
      | tools/git-hooks/commit-msg /dev/stdin >/dev/null 2>&1; then
      git log -1 --format='  %h %s' "${sha}"
      bad=1
    fi
  done < <(git rev-list HEAD)
  record 'Commits: Conventional Commits format' "${bad}"
}

#######################################
# Step 7d: buildspec.json holds numbers only (docs/releasing.md); a
# pre-release suffix lives in the Git tag.
# Outputs:
#   An explanation on stderr if the version is not valid.
#######################################
check_version() {
  local version
  version="$(jq -r .version buildspec.json)"
  if [[ "${version}" =~ ${SEMVER} ]]; then
    record "Version: buildspec.json is X.Y.Z (${version})" 0
  else
    err "buildspec.json version \"${version}\" must be X.Y.Z, numbers only"
    record "Version: buildspec.json is X.Y.Z (${version})" 1
  fi
}

main() {
  cd "$(git rev-parse --show-toplevel)" || exit 2
  check_format
  check_build
  check_unit_tests
  check_clang_tidy
  check_analyzer
  check_shell
  step '7. Project rules'
  check_code_rules
  check_text_rules
  check_commits
  check_version
  printf '\n%sSummary%s\n' "${BOLD}" "${RESET}"
  printf '  %s\n' "${summary[@]}"
  exit "${failed}"
}

main "$@"
