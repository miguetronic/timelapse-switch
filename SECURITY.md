# Security policy

## Supported versions

Security fixes go into the latest release only. Please update before reporting.

## Reporting a vulnerability

Please **do not open a public issue** for a security problem. Report it privately instead:

1. Open the repository on GitHub, go to the **Security** tab and choose **Report a vulnerability**.
2. Describe what you found, how to reproduce it, and which version and operating system you used.

You will get an answer within 7 days. Once a fix is released, the report is published with credit to you, unless you prefer to stay anonymous.

## Scope

- **In scope:** the code in this repository and the installers published in its releases.
- **Out of scope:** OBS Studio and Qt themselves. Please report those to their own projects; if one of their issues affects this plugin, we will say so in the release notes.

## What each release includes

Every release on GitHub comes with:

- **Checksums** (SHA-256) of every file, to verify a download.
- **An SBOM** (`timelapse-switch-<version>.cdx.json`, CycloneDX format): the list of what the plugin was built with. The plugin ships only its own code; OBS and Qt come from your OBS installation.
