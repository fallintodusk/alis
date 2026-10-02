# Case: Shader Compile Worker Hang (Antivirus)

Not a crash - a hang. Game thread sleeps forever inside material shader compilation.

---

## Symptom

VS callstack on Break All:

```
ntdll!NtWaitForSingleObject
KernelBase!WaitForSingleObjectEx
UnrealEditor-Core!FWindowsPlatformProcess::Sleep
UnrealEditor-Engine!FShaderCompilingManager::BlockOnShaderMapCompletion
UnrealEditor-Engine!FShaderCompilingManager::FinishCompilation
UnrealEditor-Engine!FMaterial::FinishCompilation
UnrealEditor-Engine!UMaterialInterface::EnsureIsComplete
UnrealEditor-Engine!UKismetRenderingLibrary::DrawMaterialToRenderTarget   <-- BP call site
... ProcessLocalScriptFunction / UnrealScript VM frames below ...
```

Editor is responsive to nothing. No crash dump. `Saved/Crashes/` has nothing
fresh because the editor did not actually crash.

## Log signature

In `Saved/Logs/Alis.log` (or the rotated backup):

```
LogShaderCompilers: Warning: No shader compile worker state change in NNNN seconds
LogShaderCompilers: Display: ======= ShaderCompileWorker Diagnostics =======
LogShaderCompilers: Display: Worker [N/16]: bAvailable=1, bComplete=0, bIssuedTasksToWorker=1, bLaunchedWorker=0
  Job [1/1]: <MaterialName>_<hash>/.../RayTracingMaterialHitShaders.usf|closesthit=MaterialCHS ...
```

The smoking gun is `bIssuedTasksToWorker=1, bLaunchedWorker=0`: UE assigned
the job but the `ShaderCompileWorker.exe` child process never reached
launched state.

## Root cause

`DrawMaterialToRenderTarget` synchronously calls
`UMaterialInterface::EnsureIsComplete()`, which blocks the game thread until
the material's shadermap is ready. Two permutations commonly trigger this
in ALIS:

- `RayTracingMaterialHitShaders.usf` closest-hit + any-hit - required
  because the project enables ray tracing in `Config/DefaultEngine.ini`
  (see [docs/architecture/conventions.md](../../architecture/conventions.md)).
- `DebugViewModePixelShader.usf` - editor visualization permutation,
  always needed by `EnsureIsComplete` in the editor.

If those jobs land on worker slots whose `ShaderCompileWorker.exe` process
never spawns, the game thread sits in `Sleep` forever. The proximate cause
of the failed spawn is almost always **Windows Defender** (or Avast /
Bitdefender / Kaspersky) intercepting the burst of process creations
during cooks. Confirmed by Epic forums and the iRender knowledge base
(see References).

## Trigger pattern in ALIS

Landmass `BP_Landscape_LayerStack` fires `DrawMaterialToRenderTarget` on
a material that references the Water plugin's `M_WaterVolume_01`, while
PoseSearch DDC builds, Mutable async updates, and texture / static-mesh
cooks are spawning `ShaderCompileWorker.exe` processes in parallel. The
combined burst is what trips Defender.

The Landmass BP warning is the canary:

```
LogBlueprintUserMessages: [LandmassBrushManager_C_0] Brush Requested Render:  Manager was NOT Initiliazed!!!
LogBlueprintUserMessages: [LandmassBrushManager_C_0] Render Requested by:  BP_Landscape_LayerStack
```

If you see this during a cook burst, the next material draw will block.

## Fix (one-time, run elevated PowerShell)

```powershell
Add-MpPreference -ExclusionProcess "ShaderCompileWorker.exe"
Add-MpPreference -ExclusionProcess "UnrealEditor.exe"
Add-MpPreference -ExclusionProcess "UnrealEditor-Cmd.exe"
Add-MpPreference -ExclusionPath   "%UE_PATH%\Engine\Binaries\Win64"
Add-MpPreference -ExclusionPath   "<repo>\Intermediate"
Add-MpPreference -ExclusionPath   "<repo>\DerivedDataCache"
Add-MpPreference -ExclusionPath   "$env:LOCALAPPDATA\UnrealEngine\Common\DerivedDataCache"
```

Verify:

```powershell
Get-MpPreference | Select-Object -ExpandProperty ExclusionProcess
```

For non-Defender AV, do the same in its UI.

## Immediate recovery (current session hung)

```powershell
Stop-Process -Name UnrealEditor,ShaderCompileWorker -Force -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "<repo>\Intermediate\Shaders\WorkingDirectory" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$env:LOCALAPPDATA\Temp\UnrealShaderWorkingDir" -ErrorAction SilentlyContinue
```

Do not nuke `DerivedDataCache` - just triggers another cold compile.

## Do NOT

- Disable the RayTracing plugin
- Set `r.ShaderCompiler.AsyncCompiling=0`
- Strip RT material permutations
- Reinstall the engine

These dodge the symptom and break unrelated rendering setup. AV exclusion
is the KISS fix and matches the failure mode 1:1.

## Diagnosis script

```bash
# Find the rotated session log (current Alis.log will be a fresh session
# after restart - look at the backup with the hang timestamp).
ls -lt Saved/Logs/Alis-backup-*.log | head -3

# Confirm the signature
grep -n "bLaunchedWorker=0\|No shader compile worker state change" Saved/Logs/Alis-backup-<timestamp>.log
```

## References

- https://irendering.net/how-to-fix-the-shadercompileworker-exe-system-error-in-unreal-engine/
- https://forums.unrealengine.com/t/fix-shadercompileworker/150306
- https://forums.unrealengine.com/t/editor-stuck-compiling-shaders/768402
- https://dev.epicgames.com/documentation/en-us/unreal-engine/debugging-the-shader-compile-process-in-unreal-engine
