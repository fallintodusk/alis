# Unreal Editor MCP Control

Stable policy for agent control and inspection of a live Unreal Editor. MCP is
an optional development accelerator; deterministic scripts, tests, and
machine-readable receipts remain the acceptance authority.

## Tool Routing

| Route | Preferred use |
|---|---|
| Official `unreal-mcp` | Native discovery, viewport control, editor queries, screenshots, and supervised operations where its tool is proven |
| `blueprint-mcp` | Blueprint graph and pin work, material graph precision, and editor console commands |
| `ue-mcp` | Specialized Landscape, NavMesh, Niagara, Sequencer, GAS, spline, and broad level automation not covered by the preferred route |

Tool availability never changes ownership. Generated ProjectWorld content is
changed through its supported realization wrapper, not repaired manually over
MCP. An exploratory editor change must be encoded in its owning profile or
generator and reproduced through the normal command before acceptance.

## Security and Concurrency

- Official Unreal MCP is loopback-only and Editor-only. Never expose it over
  LAN, VPN, a reverse proxy, a tunnel, or a public interface.
- Never overlap official MCP calls. They execute serially on Unreal's game
  thread.
- Serialize every mutating editor operation across every MCP route and agent.
  Multiple servers do not create independent mutation lanes.
- Read-only inspection may use another route only when it cannot race an
  editor mutation.
- Keep Semantic Search AI mode and indexing inactive until its provider, cost,
  asset-data transfer, and clear-text per-user credential storage are approved.

## Connection Proof

After configuration or an editor/client restart, use a fresh MCP client and
call `server_status`. This proves that the configured server can launch, speak
the protocol, and reach the current editor. It does not prove that an external
client process restarted; where restart identity matters, record operator
attestation separately.

The official route needs its own proof because it fails silently. `unreal-mcp`
is an HTTP attachment to the in-editor listener, so the client binds it once at
session start; if the editor is still loading a heavy map then, the game thread
cannot service the handshake and the server is dropped with no error surfaced.
The session simply runs with zero `mcp__unreal-mcp__*` tools while the stdio
routes keep working, which reads as "the official route is unavailable" rather
than "it was never attached". Start the editor and let it go idle before
starting the agent, then confirm `unreal-mcp` is listed by `/mcp` and that
`list_toolsets` returns toolsets. A raw
`POST http://127.0.0.1:8000/mcp` `initialize` is the out-of-band check when the
client shows nothing; note that `GET /mcp` answering `405` is correct, because
the server is POST-only and offers no SSE channel.

Configuration templates are owned by
[`scripts/config/mcp.json.example`](../../scripts/config/mcp.json.example) and
[`scripts/config/tools.conf.example`](../../scripts/config/tools.conf.example).
Official plugin activation and its Editor-only target are owned by
[`Alis.uproject`](../../Alis.uproject). Do not copy machine-local roots or
credentials into tracked files or duplicate activation state in another doc.

## Agent-owned startup and recovery

An agent that needs a live editor owns routine startup. Do not stop and ask the
operator merely because `UnrealEditor.exe` or an MCP listener is absent.

1. Check for an existing `UnrealEditor` process. Never terminate an existing
   interactive editor just to obtain a fresh connection.
2. If none exists, launch the project in the background through
   [`run_editor.bat`](../../scripts/ue/run/run_editor.bat). That script resolves
   the project root and launcher engine from repository-owned paths.
3. Wait for `LogInit: Display: Engine is initialized` and the required
   loopback listener (`8000` official, `8091` `ue-mcp`, or `9847`
   `blueprint-mcp`). A window title or mounted-plugin log line is not ready.
4. Retry the connection proof, then perform editor calls sequentially.
5. If startup fails because editor binaries are stale, use the project build
   route in [`workflow.md`](../build/workflow.md), relaunch, and inspect
   `Saved/Logs/Alis.log`. Escalate only an actual build/startup failure or an
   operation that requires human interaction.

### UE 5.8.1 map-switch and shutdown boundaries

- Do not live-switch from an SM5 preview world to the generated Kazan SM6 map
  with `MAP LOAD ... FEATURELEVEL=...`. UE 5.8.1 can finish World Partition
  initialization and then assert in `GetGlobalShaderMap` while releasing the
  prior render scene. Start a cold editor process with the target map package
  on its command line instead. A successful cold start is the required control
  before attributing the live-switch failure to project content.
- A live editor that has used Sequencer/Details integration can crash during
  `QUIT_EDITOR` after world and MCP teardown are already complete. The observed
  UE 5.8.1 stack enters `UBrowseToAssetOverrideSubsystem` from a Details refresh
  while `FLevelEditorSequencerIntegration` is releasing resources. Preserve the
  crash context and verify terminal receipts/logs; do not treat this engine
  shutdown stack as generated-world corruption or add a World-code workaround.
- The unattended visual-evidence wrapper remains the preferred no-window route
  when fixed scene-capture vantages are sufficient. A green capture receipt
  authenticates files, dimensions, hashes, and frame freshness only. The agent
  must still inspect the images and reject a set that does not frame its stated
  subject.

If one MCP route is still starting, use another connected route only within
the routing and concurrency rules above. Starting the editor and waiting for
its owned endpoints is normal task work, not a user dependency.

## Evidence Boundary

MCP may provide live evidence for:

- the loaded world and actor/component state;
- `Map Check` and exact automation-test dispatch;
- coordinate and GeoReferencing probes;
- fixed-view screenshots and visual inspection.

An MCP command returning `queued` or `success` proves dispatch only. Confirm
the terminal test result or `Map Check` counts in the editor log. Screenshots
and live queries support review but do not replace accepted receipts.

Keep transient screenshots and MCP output under ignored `Saved/` evidence
roots. Feature-specific verification owns the exact map, profile, probes, and
acceptance comparisons.
