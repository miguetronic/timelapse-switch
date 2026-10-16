#!/usr/bin/env bash
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# make-sbom.sh: write the Software Bill of Materials (SBOM) of a release, in CycloneDX 1.6 JSON.
#
#   tools/make-sbom.sh [output file]        (default: timelapse-switch-<version>.cdx.json)
#
# An SBOM is the ingredient list of a piece of software. Ours is short on purpose: the plugin ships
# only its own code; OBS and Qt are provided by the user's OBS installation. So the SBOM lists the
# plugin and what it was built against, taken from buildspec.json (the single source of versions).
#
# Reproducible: the same commit always produces the same file. The serial number is derived from
# the plugin's identity (UUID v5) and the timestamp is the commit date.
#
# Needs: jq (also used by the OBS template's packaging scripts) and uuidgen (util-linux).

set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

# 1. Read the identity of the plugin and of its dependencies from buildspec.json.
spec=buildspec.json
name=$(jq -r .name "$spec")
version=$(jq -r .version "$spec")
output=${1:-$name-$version.cdx.json}

plugin_ref="pkg:github/miguetronic/timelapse-switch@$version"
serial="urn:uuid:$(uuidgen --sha1 --namespace @url --name "$plugin_ref")"
timestamp=$(git log -1 --format=%cI)
runtime_note="Provided at run time by the user's OBS installation; not shipped with the plugin."
build_note="Build-time dependencies for Windows and macOS builds; not shipped with the plugin. Linux builds use the distribution's packages instead."

# 2. Build the document with jq, which takes care of JSON quoting and escaping.
#    Component scope: "required" = needed at run time, "excluded" = used only to build.
jq -n \
	--slurpfile spec "$spec" \
	--arg plugin_ref "$plugin_ref" --arg serial "$serial" --arg timestamp "$timestamp" \
	--arg runtime "$runtime_note" --arg build "$build_note" '
	$spec[0] as $s
	| $s.dependencies as $d
	| [
		{type: "library", "bom-ref": "pkg:github/obsproject/obs-studio@\($d["obs-studio"].version)",
		 name: "obs-studio (libobs, obs-frontend-api)", version: $d["obs-studio"].version,
		 purl: "pkg:github/obsproject/obs-studio@\($d["obs-studio"].version)", scope: "required",
		 description: $runtime, licenses: [{expression: "GPL-2.0-or-later"}]},
		{type: "library", "bom-ref": "pkg:github/obsproject/obs-deps@\($d.qt6.version)#qt6",
		 name: "Qt 6 (obs-deps build)", version: $d.qt6.version,
		 purl: "pkg:github/obsproject/obs-deps@\($d.qt6.version)#qt6", scope: "required",
		 description: $runtime},
		{type: "library", "bom-ref": "pkg:github/obsproject/obs-deps@\($d.prebuilt.version)",
		 name: "obs-deps (prebuilt)", version: $d.prebuilt.version,
		 purl: "pkg:github/obsproject/obs-deps@\($d.prebuilt.version)", scope: "excluded",
		 description: $build}
	] as $components
	| {
		bomFormat: "CycloneDX",
		specVersion: "1.6",
		serialNumber: $serial,
		version: 1,
		metadata: {
			timestamp: $timestamp,
			tools: {components: [{type: "application", name: "tools/make-sbom.sh"}]},
			component: {
				type: "library", "bom-ref": $plugin_ref, name: $s.name, version: $s.version, purl: $plugin_ref,
				description: "Timelapse Switch, a plugin for OBS Studio",
				licenses: [{expression: "GPL-2.0-or-later"}],
				supplier: {name: $s.author, url: [$s.website]},
				externalReferences: [{type: "vcs", url: "https://github.com/miguetronic/timelapse-switch"}]
			}
		},
		components: $components,
		dependencies: [{ref: $plugin_ref, dependsOn: [$components[]."bom-ref"]}]
	}' >"$output"

echo "SBOM written: $output"
