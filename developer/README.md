# Developer and Contributor Quick Start

ALIS development uses an exact public Git tag plus the matching signed
developer payload. The tag owns public source and documentation; the payload
provides approved generated Unreal assets that do not live in Git.

## Prerequisites

- Windows
- Git for Windows
- the supported launcher-installed Unreal Engine version
- the Visual Studio toolchain required by that engine

## Install

1. Open the [latest ALIS release](https://github.com/fallintodusk/alis/releases/latest).
2. Clone the exact release tag.
3. Download the matching Developer payload ZIP and extract it into the checkout
   with 7-Zip.
4. Configure the local Unreal Engine path and build the project.

For optional automatic setup, keep the Developer payload ZIP and manifest, the
two `INSTALL_ALIS_DEVELOPER` files, and the signed checksum files together, then
double-click the BAT file. The bootstrap clones the exact release tag, verifies
the matching payload, and delegates installation to the trusted script in that
checkout. It does not install Unreal Engine, Visual Studio, or other
machine-wide dependencies.

## Work on ALIS

- Setup and build details: [build documentation](../docs/build/README.md)
- Contribution rules: [CONTRIBUTING.md](../CONTRIBUTING.md)
- Architecture: [architecture router](../docs/architecture/README.md)
- Testing: [testing router](../docs/testing/README.md)
- Licensing and provenance: [legal router](../docs/legal/README.md)
