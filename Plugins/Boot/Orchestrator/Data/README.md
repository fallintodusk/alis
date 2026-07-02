# Development Manifest

## Purpose
This `dev_manifest.json` is used **during editor development only** (Development/DebugGame builds).

## Loading
Orchestrator loads this manifest when running in the editor:
```cpp
// OrchestratorCoreModule.cpp:287
#if UE_BUILD_SHIPPING
    ManifestPath = FPaths::Combine(GetLocalRoot(), TEXT("Manifests/latest.json"));
#else
    ManifestPath = FPaths::ProjectConfigDir() / TEXT("Manifest/dev_manifest.json"); // <- This file
#endif
```

## Format: Simplified Variant

This dev manifest uses a **simplified flat structure** for easy hand-editing during development:

```json
{
  "name": "ProjectCore",
  "version": "0.1.0",
  "activation_strategy": "Boot",
  "module": "ProjectCore",
  "code_hash": "...",
  "url_code": "file://...",
  ...
}
```

## Production Format (Different!)

Production manifests use the **full CDN schema** with nested objects:

```json
{
  "uuid": "550e8400-e29b-41d4-a716-446655440000",
  "name": "ProjectCore",
  "version": "0.1.0",
  "activation_strategy": "Boot",
  "code": {
    "url": "https://cdn.example.com/...",
    "hash": "...",
    "size": 12345
  },
  "assets": [
    {
      "url": "https://cdn.example.com/...",
      "hash": "...",
      "size": 67890,
      "role": "pak"
    }
  ]
}
```

**Schema source of truth:** `<cdn-repo>/docs/manifest.schema.json` (WSL path: `<cdn-repo>/docs/manifest.schema.json`)

## Key Fields

### activation_strategy (Required)
- **"Boot"**: Essential plugins loaded at startup
  - Examples: Orchestrator, ProjectCore, ProjectUI, ProjectMenuMain, MainMenuWorld
  - Performance: ~80% faster startup, ~60% lower baseline memory

- **"OnDemand"**: Lazy-loaded when requested at runtime
  - Examples: ProjectInventory, ProjectCombat, ProjectDialogue, gameplay worlds
  - Performance: 100-500ms load cost on first activation

### url_code (Dev Mode)
In dev mode, use `file://` URLs pointing to plugin directories:
```json
"url_code": "file://<project-root>/Plugins/Foundation/ProjectCore"
```

This allows Orchestrator to find plugins without requiring CDN downloads during development.

## Editing This File

When adding a new plugin to the dev manifest:

1. **Choose activation_strategy:**
   - Boot: Foundation, Systems, UI Framework, Main Menu World
   - OnDemand: Features, Gameplay worlds, Conditional UI

2. **Add dependencies** in `depends_on` if needed:
   ```json
   "depends_on": [
     {
       "name": "ProjectCore",
       "version": ">=0.1.0"
     }
   ]
   ```

3. **Use placeholder hashes** (dev mode doesn't verify):
   ```json
   "code_hash": "0000000000000000000000000000000000000000000000000000000000000000"
   ```

## Validation

The dev manifest should be **compatible with** (not identical to) the CDN schema. Both must have:
- ✅ `activation_strategy` field
- ✅ `depends_on` dependency format
- ✅ `channel` field

## Migration to Production

When building for production:
1. Build Service reads plugin metadata from `.uplugin` files
2. Computes real SHA-256 hashes for code and content
3. Uploads artifacts to CDN
4. Generates production manifest using full CDN schema format
5. Launcher downloads production manifest and plugins

The dev manifest is **never used in production builds**.
