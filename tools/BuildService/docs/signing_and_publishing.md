# Build Service Publishing

Build Service can upload staged artifacts and a manifest to the configured CDN
Control API, request verification/promotion, or request rollback.

```powershell
.\scripts\build.ps1 run -- publish --build-id <id> --stage
.\scripts\build.ps1 run -- publish --build-id <id> --promote
.\scripts\build.ps1 run -- publish --build-id <id> --rollback
```

Exactly one operation is required. Promote verifies the local source-release
receipt against the current Git/source fingerprint and exact staged files
before making the remote request. The sibling CDN owns server-side storage,
promotion, and rollback guarantees.

## Signing status

Manifest signing and DLL Authenticode signing are not implemented in this
pipeline. Optional signature fields therefore remain empty. Build Service must
not be presented as the authority for ALIS's signed public GitHub release; that
workflow belongs to [release packaging](../../../docs/build/packaging_guide.md).

The Control API token is read only from the environment-variable name selected
by configuration. Do not place credentials in committed configuration or logs.
