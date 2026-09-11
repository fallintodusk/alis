# ALIS

Learn. Survive. Connect.

ALIS is an open-source Unreal Engine 5 survival-game prototype. Its current
world-generation showcase rebuilds real geography as playable, streamed
blockout worlds. The public project is work in progress, not finished city art.

## Choose Your Route

| You are | Start here |
|---|---|
| Player | Download the packaged prototype from the [latest GitHub release](https://github.com/fallintodusk/alis/releases/latest) and follow its `INSTALL.txt`. |
| Developer | Use the [build and setup router](docs/build/README.md). Source releases pair one exact Git tag with one signed developer asset payload. |
| World builder | Start at the [World plugin router](Plugins/World/README.md) for generation ownership and tools. |
| Architecture reviewer | Start at the [architecture router](docs/architecture/README.md) for principles, plugin boundaries, and diagrams. |
| Contributor | Read [CONTRIBUTING.md](CONTRIBUTING.md), then use the [testing router](docs/testing/README.md). |
| Legal or security reviewer | Use the [legal router](docs/legal/README.md). Report a vulnerability privately through [GitHub Security Advisories](https://github.com/fallintodusk/alis/security/advisories/new). |

Project purpose and social contract live in [VISION.md](VISION.md) and
[ALIS_PACT.md](ALIS_PACT.md). The complete documentation tree starts at
[docs/README.md](docs/README.md).

## Run or Build

Players do not need a source checkout. Download the game archive from the
matching GitHub release and follow its included instructions.

Developers use two matching release artifacts:

```text
public Git tag v2.0.0
        +
signed ALIS developer payload v2.0.0
        -> buildable Unreal project
```

The Git tag contains public source, JSON, schemas, configuration templates,
documentation, and tools. Unreal `.uasset` and `.umap` projections are kept out
of Git and installed from the matching signed payload. Restricted third-party
content is not redistributed.

From an exact clean tag checkout on Windows:

```powershell
git clone --config core.longpaths=true --branch v2.0.0 --depth 1 `
  https://github.com/fallintodusk/alis.git Alis
Set-Location .\Alis

.\scripts\git\mirror\install_developer_payload.ps1 `
  -ProjectRoot . `
  -ReleaseDir C:\path\to\ALIS_DeveloperProject_v2.0.0 `
  -RequireReleaseSignature

Copy-Item .\scripts\config\ue_path.conf.example .\scripts\config\ue_path.local.conf
# Edit ue_path.local.conf and set UE_PATH to a launcher-installed Unreal Engine 5.8.
.\scripts\ue\standalone\build.ps1
```

The installer verifies the public signature, exact tag and revision, archive
inventory, paths, and conflicts before copying. It uses only the public key;
developers never need the private release-signing key.

Detailed setup, prerequisites, and troubleshooting are owned by the
[build documentation](docs/build/README.md). Payload composition and trust
boundaries are owned by the [public mirror tooling](scripts/git/mirror/README.md).

## What Is Implemented

The repository exposes current project code and verifiable patterns, including:

- modular Unreal plugins with explicit dependency direction;
- JSON-driven definitions, UI, experiences, and world-generation inputs;
- deterministic source-to-canonical-to-Unreal world generation;
- generated terrain, roads, water, vegetation, and building layers;
- World Partition runtime streaming for city-scale blockout worlds;
- inventory, vitals, dialogue, interaction, character, loading, and UI systems;
- focused validation, packaging, signing, and release automation.

The generated Kazan and Manhattan maps demonstrate the World pipeline. Legacy
Old City 17 content is not part of the public developer acceptance route.

## Repository Map

| Path | Owner |
|---|---|
| `Source/` | Game targets and the top-level game module |
| `Plugins/` | Boot, foundation, systems, UI, gameplay, feature, resource, editor, and World owners |
| `docs/` | Project documentation routers and durable contracts |
| `scripts/` | Build, test, setup, package, verification, and mirror automation |
| `tools/` | Standalone project tooling, including the World data pipeline |

Use each directory's README rather than guessing ownership from filenames.

## License and Release Trust

Component licensing starts at [LICENSE](LICENSE). Asset provenance and release
policy are routed through [docs/legal/README.md](docs/legal/README.md).

Every public release publishes hashes and a detached signature. The trusted
checkout-local verifier checks them before developer payload installation.
Release commands and evidence requirements are owned by the
[packaging documentation](docs/build/README.md).
