#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Searches for words that must never be published. The list is private, so
# it lives outside the repository, in the maintainer's own file: one
# extended regular expression per line, case is ignored. Where the file
# does not exist, the search says so and passes.
#
#   tools/check-private-words.sh             the repository (files and every
#                                            commit message) and site/dist
#   tools/check-private-words.sh --staged    what is about to be committed
#                                            (tools/git-hooks/pre-commit)
#   tools/check-private-words.sh --message <file>
#                                            a commit message
#                                            (tools/git-hooks/commit-msg)
#
# Binary files (pictures, video) are searched as text, so their metadata is
# searched too.
# Exit status: 0 nothing found (or no list), 1 found, 2 the list is invalid.

set -u

# Prints an error to stderr.
err() {
  echo "check-private-words: $*" >&2
}

#######################################
# Runs grep with the list; a grep error (status 2) is an error here too.
# Arguments:
#   The list, then grep's other arguments.
# Outputs:
#   The matches.
# Returns:
#   0 found, 1 not found, 2 error.
#######################################
search() {
  local list="$1"
  shift
  grep -a -i -E -f "${list}" "$@"
}

#######################################
# Searches the staged version of every staged file.
#######################################
search_staged() {
  local list="$1" path status=1 found
  while IFS= read -r -d '' path; do
    found="$(git show ":${path}" | search "${list}" -n)"
    case "$?" in
      0) echo "${path}: ${found}" && status=0 ;;
      2) return 2 ;;
    esac
  done < <(git diff --cached --name-only -z --diff-filter=ACMR)
  return "${status}"
}

#######################################
# Searches every project file, every commit message and the built website.
#######################################
search_all() {
  local list="$1" status=1 code
  local -a files
  mapfile -t files < <(git ls-files --cached --others --exclude-standard)
  if ((${#files[@]} > 0)); then
    search "${list}" -n -- "${files[@]}"
    code=$?
    ((code == 2)) && return 2
    ((code == 0)) && status=0
  fi
  local messages line
  messages="$(git log --format='%H %B' | search "${list}")"
  code=$?
  ((code == 2)) && return 2
  if ((code == 0)); then
    while IFS= read -r line; do
      echo "commit message: ${line}"
    done <<<"${messages}"
    status=0
  fi
  if [[ -d site/dist ]]; then
    search "${list}" -r -l site/dist
    code=$?
    ((code == 2)) && return 2
    ((code == 0)) && status=0
  fi
  return "${status}"
}

main() {
  local list="${HOME}/.config/timelapse-switch/private-words"
  list="${TSW_PRIVATE_WORDS:-${list}}"
  if [[ ! -f "${list}" ]]; then
    echo "no private word list at ${list}: skipped"
    return 0
  fi
  # An invalid expression must stop everything, never pass silently.
  grep -E -f "${list}" /dev/null
  if (($? == 2)); then
    err "the list ${list} has an invalid expression"
    return 2
  fi
  cd "$(git rev-parse --show-toplevel)" || return 2
  local status
  case "${1:-}" in
    --staged) search_staged "${list}" ;;
    --message) search "${list}" -n -- "$2" ;;
    '') search_all "${list}" ;;
    *) err "unknown option: $1" && return 2 ;;
  esac
  status=$?
  case "${status}" in
    0) err 'private words found (above): remove them before publishing' \
      && return 1 ;;
    1) return 0 ;;
    *) err 'the search failed' && return 2 ;;
  esac
}

main "$@"
