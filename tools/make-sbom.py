#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""make-sbom.py: write the Software Bill of Materials (SBOM) of a release, in CycloneDX 1.6 JSON.

    tools/make-sbom.py [output file]        (default: timelapse-switch-<version>.cdx.json)

An SBOM is the ingredient list of a piece of software. Ours is short on purpose: the plugin ships
only its own code; OBS and Qt are provided by the user's OBS installation. So the SBOM lists the
plugin and what it was built against, taken from buildspec.json (the single source of versions).

The output is reproducible: the same commit always produces the same file (no random IDs, the
timestamp is the commit date).
"""

import json
import subprocess
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPOSITORY = "https://github.com/miguetronic/timelapse-switch"


def commit_date() -> str:
    """Date of the current commit (ISO 8601), so the SBOM does not change between runs."""
    result = subprocess.run(["git", "log", "-1", "--format=%cI"], cwd=ROOT, capture_output=True, text=True, check=True)
    return result.stdout.strip()


def dependency(name: str, version: str, purl: str, scope: str, description: str, license_id: str = "") -> dict:
    """One component of the SBOM. scope: "required" = needed at run time, "excluded" = build time only."""
    component = {
        "type": "library",
        "bom-ref": purl,
        "name": name,
        "version": version,
        "purl": purl,
        "scope": scope,
        "description": description,
    }
    if license_id:
        component["licenses"] = [{"expression": license_id}]
    return component


def build_sbom(spec: dict) -> dict:
    """Assemble the CycloneDX document from buildspec.json."""
    deps = spec["dependencies"]
    obs = deps["obs-studio"]["version"]
    obs_deps = deps["prebuilt"]["version"]
    qt = deps["qt6"]["version"]
    plugin_ref = f"pkg:github/miguetronic/timelapse-switch@{spec['version']}"
    components = [
        dependency("obs-studio (libobs, obs-frontend-api)", obs, f"pkg:github/obsproject/obs-studio@{obs}", "required",
                   "Provided at run time by the user's OBS installation; not shipped with the plugin.",
                   "GPL-2.0-or-later"),
        dependency("Qt 6 (obs-deps build)", qt, f"pkg:github/obsproject/obs-deps@{qt}#qt6", "required",
                   "Provided at run time by the user's OBS installation; not shipped with the plugin."),
        dependency("obs-deps (prebuilt)", obs_deps, f"pkg:github/obsproject/obs-deps@{obs_deps}", "excluded",
                   "Build-time dependencies for Windows and macOS builds; not shipped with the plugin. "
                   "Linux builds use the distribution's packages instead."),
    ]
    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "serialNumber": "urn:uuid:" + str(uuid.uuid5(uuid.NAMESPACE_URL, plugin_ref)),
        "version": 1,
        "metadata": {
            "timestamp": commit_date(),
            "tools": {"components": [{"type": "application", "name": "tools/make-sbom.py"}]},
            "component": {
                "type": "library",
                "bom-ref": plugin_ref,
                "name": spec["name"],
                "version": spec["version"],
                "purl": plugin_ref,
                "description": "Timelapse Switch, a plugin for OBS Studio",
                "licenses": [{"expression": "GPL-2.0-or-later"}],
                "supplier": {"name": spec["author"], "url": [spec["website"]]},
                "externalReferences": [{"type": "vcs", "url": REPOSITORY}],
            },
        },
        "components": components,
        "dependencies": [{"ref": plugin_ref, "dependsOn": [c["bom-ref"] for c in components]}],
    }


def main() -> int:
    spec = json.loads((ROOT / "buildspec.json").read_text(encoding="utf-8"))
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(f"{spec['name']}-{spec['version']}.cdx.json")
    output.write_text(json.dumps(build_sbom(spec), indent=2) + "\n", encoding="utf-8")
    print(f"SBOM written: {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
