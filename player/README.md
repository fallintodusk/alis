# Player Quick Start

ALIS currently provides a packaged Windows build through GitHub Releases. A
source checkout, Unreal Engine, and development tools are not required.

## Install

1. Open the [latest ALIS release](https://github.com/fallintodusk/alis/releases/latest).
2. Download every numbered `ALIS_Win64` ZIP part into one folder.
3. With 7-Zip, extract only the `.zip.001` file. Keep the other numbered parts
   beside it.
4. Run `Alis.exe` from the extracted folder.

For optional automatic setup, keep `INSTALL_ALIS_PLAYER.bat` and
`INSTALL_ALIS_PLAYER.ps1` beside the archive parts, then double-click the BAT
file. It works offline, requires no administrator access, verifies every part,
and does not overwrite an existing destination.

Authenticity verification is optional for normal installation. To verify the
publisher signature and every release file first, run the release's
`VERIFY_RELEASE.bat`.

The packaged game is governed by the `PRODUCT_TERMS.txt` included with its
release. Report problems through [GitHub Issues](https://github.com/fallintodusk/alis/issues).
