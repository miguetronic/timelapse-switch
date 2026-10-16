#!/usr/bin/env bash
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# check.sh: run every automatic check of the project with one command (Linux).
#
#   tools/check.sh
#
# It runs all steps even if one fails, so you see every problem at once, and ends with a summary.
# Exit code 0 means everything is green. The rules behind each step are in docs/coding-rules.md.
#
#   1. Format          clang-format 19.1.1 and gersemi 0.21.0, the same versions as the OBS CI
#   2. Build           compiler with pedantic warnings, any warning is an error   (rule P10-10)
#   3. clang-tidy      static analyzer #1                                          (rule P10-10)
#   4. GCC analyzer    static analyzer #2 (gcc -fanalyzer)                         (rule P10-10)
#   5. Project rules   simple text checks for rules no tool covers   (P10-1, P10-8, style, commits, SemVer)

set -u
cd "$(git rev-parse --show-toplevel)" || exit 2

RED=$'\e[31m' GREEN=$'\e[32m' BOLD=$'\e[1m' RESET=$'\e[0m'
declare -a SUMMARY=()
FAILED=0

# record <step name> <exit code>: remember the result of one step for the final summary.
record() {
	if [[ $2 -eq 0 ]]; then
		SUMMARY+=("${GREEN}PASS${RESET}  $1")
	else
		SUMMARY+=("${RED}FAIL${RESET}  $1")
		FAILED=1
	fi
}

step() { printf '\n%s== %s%s\n' "$BOLD" "$1" "$RESET"; }

# ---------------------------------------------------------------------------------------------
step "1. Format (clang-format 19.1.1, gersemi 0.21.0)"
if [[ -x .venv/bin/clang-format && -x .venv/bin/gersemi ]]; then
	PATH="$PWD/.venv/bin:$PATH" ./build-aux/run-clang-format --check
	record "format: C and C++ (clang-format)" $?
	PATH="$PWD/.venv/bin:$PATH" ./build-aux/run-gersemi --check
	record "format: CMake (gersemi)" $?
else
	echo "Formatting tools missing. Install them once with:"
	echo "  python3 -m venv .venv && .venv/bin/pip install -r tools/requirements-dev.txt"
	record "format (tools not installed)" 1
fi

# ---------------------------------------------------------------------------------------------
step "2. Build with strict warnings"
if [[ ! -f build_x86_64/build.ninja ]]; then
	cmake --preset ubuntu-x86_64 >/dev/null
fi
cmake --build build_x86_64
record "build (zero warnings)" $?

# ---------------------------------------------------------------------------------------------
step "3. Static analysis: clang-tidy"
# The OBS build adds a few GCC-only flags that clang-tidy (Clang) does not know; remove or silence them.
mapfile -t SOURCES < <(git ls-files 'src/*.c' 'src/*.cpp')
SOURCES+=(build_x86_64/plugin-support.c)
clang-tidy -p build_x86_64 --quiet --removed-arg=-mno-direct-extern-access \
	--extra-arg=-Wno-unknown-warning-option "${SOURCES[@]}" 2>&1 | grep -vE "warnings? generated"
record "clang-tidy (zero warnings)" "${PIPESTATUS[0]}"

# ---------------------------------------------------------------------------------------------
step "4. Static analysis: GCC analyzer"
# A separate build directory, so the normal build stays fast. Analyzer findings are errors (-Werror).
if [[ ! -f build_analyzer/build.ninja ]]; then
	cmake --preset ubuntu-x86_64 -B build_analyzer -DCMAKE_C_FLAGS=-fanalyzer >/dev/null
fi
cmake --build build_analyzer
record "gcc -fanalyzer (zero warnings)" $?

# ---------------------------------------------------------------------------------------------
step "5. Project rules"

# rule_absent <description> <extended regex> <files...>: fail if the pattern appears in any file.
rule_absent() {
	local description=$1 pattern=$2
	shift 2
	if grep -nE "$pattern" "$@" 2>/dev/null; then
		record "$description" 1
	else
		record "$description" 0
	fi
}

mapfile -t CODE < <(git ls-files 'src/*.c' 'src/*.cpp' 'src/*.h' 'src/*.hpp' 'src/*.c.in')
mapfile -t TEXT < <(git ls-files | grep -vE '^LICENSE$|\.(png|jpg|gif|ico|icns|webp)$')

rule_absent "P10-1: no goto, setjmp or longjmp" '\bgoto\b|\bsetjmp\b|\blongjmp\b' "${CODE[@]}"
rule_absent "P10-8: no token pasting or variadic macros" '##|__VA_ARGS__' "${CODE[@]}"
# Conditional compilation lives only in src/tsw-compiler.h. The one exception is the standard
# header boilerplate "#ifdef __cplusplus / extern "C"", which lets C++ code include our C headers.
mapfile -t NOT_COMPILER_H < <(printf '%s\n' "${CODE[@]}" | grep -v '^src/tsw-compiler.h$')
if grep -nE '^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef|elif)\b' "${NOT_COMPILER_H[@]}" | grep -v '__cplusplus'; then
	record "P10-8: conditional compilation only in tsw-compiler.h" 1
else
	record "P10-8: conditional compilation only in tsw-compiler.h" 0
fi
rule_absent "Style: no em dashes or en dashes in any file" $'\u2014|\u2013' "${TEXT[@]}"

# Conventional Commits (CONTRIBUTING.md): every commit in the history, checked by the same hook Git runs.
BAD_COMMITS=0
while IFS= read -r sha; do
	if ! git log -1 --format=%B "$sha" | tools/git-hooks/commit-msg /dev/stdin >/dev/null; then
		echo "  $(git log -1 --format='%h %s' "$sha")"
		BAD_COMMITS=1
	fi
done < <(git rev-list HEAD)
record "Commits: Conventional Commits format" "$BAD_COMMITS"

# SemVer (docs/releasing.md): buildspec.json holds numbers only; a pre-release suffix lives in the Git tag.
VERSION=$(python3 -c 'import json; print(json.load(open("buildspec.json"))["version"])')
if [[ $VERSION =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
	record "Version: buildspec.json is X.Y.Z ($VERSION)" 0
else
	echo "buildspec.json version \"$VERSION\" must be X.Y.Z, numbers only (see docs/releasing.md)"
	record "Version: buildspec.json is X.Y.Z ($VERSION)" 1
fi

# ---------------------------------------------------------------------------------------------
printf '\n%sSummary%s\n' "$BOLD" "$RESET"
printf '  %s\n' "${SUMMARY[@]}"
exit "$FAILED"
