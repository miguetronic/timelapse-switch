#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Builds the website (site/, Astro + Starlight, ADR-010) and checks it:
#
#   1. it builds, and every link between its pages works (Starlight's links
#      validator fails the build otherwise);
#   2. every error code the plugin uses (src/) or the UX spec lists has its
#      text in site/src/data/errors.json and its page, at the address the
#      dock opens: /errors/tsw-exxx/ (src/ui/dock.cpp, ERROR_PAGE).
#
#   tools/check-site.sh
#
# Needs Node.js 22 and npm; installs the site's packages from its lock
# file the first time.

set -euo pipefail

readonly SITE='site'

# Prints an error to stderr.
err() {
  echo "check-site: $*" >&2
}

#######################################
# Every TSW-Exxx code in the plugin's sources and in the UX spec's table.
# Outputs:
#   One code per line, sorted, unique.
#######################################
error_codes() {
  {
    grep -rhoE 'TSW-E[0-9]{3}' src
    grep -hoE '^\| TSW-E[0-9]{3}' docs/design/ux-spec.md | tr -d '| '
  } | sort -u
}

main() {
  cd "$(git rev-parse --show-toplevel)"
  if [[ ! -d "${SITE}/node_modules" ]]; then
    (cd "${SITE}" && npm ci --no-audit --no-fund)
  fi
  (cd "${SITE}" && npm run build)

  local code page missing=0
  while read -r code; do
    page="${SITE}/dist/errors/${code,,}/index.html"
    if ! grep -q "\"${code}\"" "${SITE}/src/data/errors.json"; then
      err "${code} has no text in ${SITE}/src/data/errors.json"
      missing=1
    elif [[ ! -f "${page}" ]]; then
      err "${code} has no page at ${page}"
      missing=1
    fi
  done < <(error_codes)
  if ((missing == 1)); then
    return 1
  fi
  echo "check-site: built; $(error_codes | wc -l) error pages present"
}

main "$@"
