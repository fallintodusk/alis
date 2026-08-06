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

Configuration templates are owned by
[`scripts/config/mcp.json.example`](../../scripts/config/mcp.json.example) and
[`scripts/config/tools.conf.example`](../../scripts/config/tools.conf.example).
Official plugin activation and its Editor-only target are owned by
[`Alis.uproject`](../../Alis.uproject). Do not copy machine-local roots or
credentials into tracked files or duplicate activation state in another doc.

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
