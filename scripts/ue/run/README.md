# Run Scripts

Small runtime launch helpers for ALIS.

## Scripts

### `run_editor.bat`

Launches the Unreal Editor for the project. It resolves both the repository
root and the configured launcher engine; it has no machine-local project path.

Use when:
- you want a quick local editor start command
- you are validating that the project opens with the configured engine
- an agent needs a live editor for MCP inspection or supervised control

From an agent PowerShell session, start it without blocking that session:

```powershell
Start-Process -FilePath "cmd.exe" `
  -ArgumentList "/c", "scripts\ue\run\run_editor.bat" `
  -WorkingDirectory (Get-Location)
```

The window appearing is not a readiness proof. For MCP work, follow the
engine-initialization and loopback-listener checks in
[mcp_editor_control.md](../../../docs/ue_engine/mcp_editor_control.md).

## Notes

- Agents own routine editor startup. An absent editor is not an operator
  blocker by itself.
- Local engine paths can differ by machine and remain owned by the config SOT.
- Public mirror sanitization replaces maintainer-local paths with neutral placeholders.
- For build and packaging workflows, use the higher-level routes in:
  - [../../../docs/build/README.md](../../../docs/build/README.md)
  - [../package/README.md](../package/README.md)
