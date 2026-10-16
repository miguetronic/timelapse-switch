#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Writes the Software Bill of Materials (SBOM) of a release, in CycloneDX
# 1.6 JSON.
#
#   tools/make-sbom.sh [output file]
#
# The default output is timelapse-switch-<version>.cdx.json.
#
# An SBOM is the ingredient list of a piece of software. Ours is short on
# purpose: the plugin ships only its own code, and OBS and Qt come from the
# user's OBS installation. So it lists the plugin and what it was built
# against, read from buildspec.json, the single source of versions.
#
# Reproducible: the same commit always gives the same file. The serial
# number is derived from the plugin's identity (UUID v5) and the timestamp
# is the commit date.
#
# Needs jq (also used by the OBS template's packaging scripts) and uuidgen
# (util-linux).

set -euo pipefail

readonly REPOSITORY='https://github.com/miguetronic/timelapse-switch'
readonly RUNTIME_NOTE="Provided at run time by the user's OBS installation; \
not shipped with the plugin."
readonly BUILD_NOTE="Build-time dependencies for Windows and macOS builds; \
not shipped with the plugin. Linux builds use the distribution's packages \
instead."

root="$(git rev-parse --show-toplevel)"
readonly SPEC="${root}/buildspec.json"

# 1. Identity of the plugin, serial number and timestamp.
name="$(jq -r .name "${SPEC}")"
version="$(jq -r .version "${SPEC}")"
output="${1:-${name}-${version}.cdx.json}"
plugin_ref="pkg:github/miguetronic/timelapse-switch@${version}"
serial="urn:uuid:$(uuidgen --sha1 --namespace @url --name "${plugin_ref}")"
timestamp="$(git log -1 --format=%cI)"

# 2. Build the document with jq, which handles JSON quoting and escaping.
#    Component scope "required": needed at run time; "excluded": only to
#    build.
jq -n \
  --slurpfile spec "${SPEC}" \
  --arg plugin_ref "${plugin_ref}" \
  --arg serial "${serial}" \
  --arg timestamp "${timestamp}" \
  --arg repository "${REPOSITORY}" \
  --arg runtime "${RUNTIME_NOTE}" \
  --arg build "${BUILD_NOTE}" '
  def dep($name; $version; $purl; $scope; $description):
    {type: "library", "bom-ref": $purl, name: $name, version: $version,
     purl: $purl, scope: $scope, description: $description};

  $spec[0] as $s
  | $s.dependencies["obs-studio"].version as $obs
  | $s.dependencies.qt6.version as $qt
  | $s.dependencies.prebuilt.version as $deps
  | [
      dep("obs-studio (libobs, obs-frontend-api)"; $obs;
          "pkg:github/obsproject/obs-studio@\($obs)"; "required"; $runtime)
        + {licenses: [{expression: "GPL-2.0-or-later"}]},
      dep("Qt 6 (obs-deps build)"; $qt;
          "pkg:github/obsproject/obs-deps@\($qt)#qt6"; "required"; $runtime),
      dep("obs-deps (prebuilt)"; $deps;
          "pkg:github/obsproject/obs-deps@\($deps)"; "excluded"; $build)
    ] as $components
  | {
      bomFormat: "CycloneDX",
      specVersion: "1.6",
      serialNumber: $serial,
      version: 1,
      metadata: {
        timestamp: $timestamp,
        tools: {components: [{type: "application",
                              name: "tools/make-sbom.sh"}]},
        component: {
          type: "library", "bom-ref": $plugin_ref, name: $s.name,
          version: $s.version, purl: $plugin_ref,
          description: "Timelapse Switch, a plugin for OBS Studio",
          licenses: [{expression: "GPL-2.0-or-later"}],
          supplier: {name: $s.author, url: [$s.website]},
          externalReferences: [{type: "vcs", url: $repository}]
        }
      },
      components: $components,
      dependencies: [{ref: $plugin_ref,
                      dependsOn: [$components[]."bom-ref"]}]
    }' >"${output}"

echo "SBOM written: ${output}"
