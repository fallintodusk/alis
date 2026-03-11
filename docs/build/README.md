# Build & Compile

Quick routing for build tasks. For detailed guide, see [workflow.md](workflow.md).

---

## Decision Tree: What Do You Want to Build?

```
Q: What do you want to build?
[?] First time setup -> [#first-time-setup]
[?] Editor (for development) -> [#build-editor]
[?] Specific module (fast iteration) -> [#build-module]
[?] Public release package -> [#package-release]
[?] Client (packaged game) -> [#build-client]
[?] Dedicated server -> [#build-server]
[?] Everything (full rebuild) -> [#build-all]
[?] Fix build errors -> [#troubleshooting]
```

---

## First-Time Setup

**Prerequisites:**
1. Unreal Engine 5.5 installed
2. Visual Studio 2022 with C++ workload
3. Git LFS configured (workaround: `git config http.version HTTP/1.1`)

**Initial Setup:**
```bash
# Set UE path (one-time)
cp scripts/config/ue_path.conf.example scripts/config/ue_path.conf
# Edit: UE_PATH="<ue-path>"

# Generate project files
# Right-click Alis.uproject ? "Generate Visual Studio project files"

# First build (takes 15-30 min)
scripts/ue/build/build.bat AlisEditor Win64 Development
```

**Detailed guide:** [workflow.md#first-time-setup](workflow.md#first-time-setup)

---

## Build Editor (Development)

**Command:**
```bash
scripts/ue/build/build.bat AlisEditor Win64 Development
```

**When to use:**
- After pulling latest code
- After changing C++ code
- Before running editor

**Duration:** 5-15 minutes (incremental), 15-30 min (clean)

**Detailed guide:** [workflow.md#build-targets](workflow.md#build-targets)

---

## Build Module (Fast Iteration)

**Command:**
```bash
scripts/ue/build/rebuild_module_safe.ps1 -ModuleName ProjectMenuMain
```

**When to use:**
- Working on single plugin/module
- Need fast iteration (<5 min builds)
- Module has no cross-dependencies changed

**Example Modules:**
- `ProjectMenuMain` - Menu feature plugin
- `ProjectBoot` - Boot system
- `ProjectLoading` - Loading pipeline
- `ProjectCore` - Core utilities

**Limitations:**
- Won't catch cross-module issues
- May need full build if dependencies changed

**Detailed guide:** [workflow.md#fast-iteration-techniques](workflow.md#fast-iteration-techniques)

---

## Build Client (Packaged)

**Command:**
```bash
scripts/ue/build/build.bat AlisClient Win64 Shipping
```

**When to use:**
- Creating distributable build
- Testing packaged build behavior
- Performance testing (Shipping config)

**Output:** `Binaries/Win64/AlisClient.exe`

**Note:** AlisClient excludes server code (`WITH_SERVER_CODE = 0`)

**Detailed guide:** [workflow.md#build-targets](workflow.md#build-targets)

---

## Package Release

**Command:**
```powershell
.\scripts\ue\package\package_release.ps1 -EngineRoot <ue-path> -CreateReleaseArchive
.\scripts\ue\package\sign_release.ps1 -ReleaseDir <release_dir>
.\scripts\ue\package\verify_release.ps1 -ReleaseDir <release_dir>
```

**When to use:**
- Creating a real public Win64 release archive
- Validating GitHub Releases size limits
- Producing `package_summary.txt`, `SHA256SUMS.txt`, `SHA256SUMS.txt.asc`, optional `verify_release_summary.txt`, and a tested transport artifact

**Canonical guide:** [packaging_guide.md](packaging_guide.md)

---

## Build Dedicated Server

**Command:**
```bash
scripts/ue/build/build.bat AlisServer Win64 Development
```

**When to use:**
- Testing server-only code
- Setting up dedicated server
- Multiplayer testing

**Server-specific code:** Wrapped in `#if WITH_SERVER_CODE`

**Detailed guide:** [../gameplay/multiplayer/architecture.md](../gameplay/multiplayer/architecture.md)

---

## Build Everything (Full Rebuild)

**When to use:**
- After major engine/plugin changes
- Resolving mysterious build issues
- After switching branches with large changes

**Steps:**
```bash
# 1. Clean intermediate files
rd /s /q Binaries Intermediate

# 2. Regenerate project files
# Right-click Alis.uproject ? "Generate Visual Studio project files"

# 3. Full build
scripts/ue/build/build.bat AlisEditor Win64 Development
```

**Duration:** 20-40 minutes

---

## Troubleshooting
 
 **See:** [workflow.md#troubleshooting](workflow.md#troubleshooting)

---

### Fast Pre-Commit Checks (No Full Build)

**Run before committing:**
```bash
# Validate C++ reflection (30 sec)
scripts/ue/check/validate_uht.bat

# Validate Blueprints (2-5 min)
scripts/ue/check/validate_blueprints.bat

# Validate assets (1-3 min)
scripts/ue/check/validate_assets.bat
```

**See:** [../../scripts/docs/architecture.md](../../scripts/docs/architecture.md)

---

## Related Documentation

### Detailed Guides
- **Complete build guide:** [workflow.md](workflow.md)
- **Release packaging guide:** [packaging_guide.md](packaging_guide.md)
- **Source build optimization:** [../ue_engine/build.md](../ue_engine/build.md) - Avoid full engine rebuilds
- **Build scripts reference:** [../../scripts/docs/architecture.md#Build-Scripts](../../scripts/docs/architecture.md#Build-Scripts)
- **Plugin architecture:** [../architecture/plugin_architecture.md](../architecture/plugin_architecture.md)
- **Core principles:** [../architecture/core_principles.md](../architecture/core_principles.md)
- **Feature setup:** [../guides/Feature_setup.md](../guides/Feature_setup.md)
- **CI/CD builds:** [../ci/README.md](../ci/README.md)

### Quick References
- **Setup:** [workflow.md](workflow.md)
- **Conventions:** [../architecture/conventions.md](../architecture/conventions.md)
- **Troubleshooting:** [../troubleshooting.md](../troubleshooting.md)

---

## Quick Links

- Back to [Master Index](../README.md)
- Test your build: [../testing/](../testing/README.md)
- Understand architecture: [../architecture/README.md](../architecture/README.md)
- Primary router: [../../AGENTS.md](../../AGENTS.md)

---

**Last Updated:** 2025-10-30
**Category:** Build & Development
**Tokens:** ~500 (lightweight router)
