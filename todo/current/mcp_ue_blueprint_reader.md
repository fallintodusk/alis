# MCP: Read UE Blueprints from Agent

Goal: evaluate MCP servers that expose UE Editor data (Blueprints, assets) to AI agents.

## Why This Is Needed

UE 5.7 does NOT fix Blueprint visibility out of the box.

- Blueprints are `.uasset` graph assets, not text files - agents are blind.
- Epic's Blueprint API docs are still "early work in progress" and may be missing/outdated.
- Epic's official tooling is editor automation: Python, Editor Utility Blueprints, Remote Control - not a built-in MCP/agent copilot.
- Without a bridge that serializes BP graphs or edits through editor APIs, agents cannot see Blueprint logic.

## Candidates

### 1. mirno-ehf/ue5-mcp - Best match: dedicated Blueprint MCP (RECOMMENDED FIRST)

- https://github.com/mirno-ehf/ue5-mcp
- Explicitly lets Claude Code or any MCP client read, modify, and create Blueprints, Materials, and Anim Blueprints
- How: exposes project Blueprints over a local HTTP server from an UE editor plugin
- Most direct answer to "agents can't see BP" - centered on Blueprint asset access, not generic editor control
- Avoids the weak path of blindly writing Unreal Python

### 2. Natfii/UnrealClaude - Good in-editor Claude option for UE 5.7 (SECOND OPTION)

- https://github.com/Natfii/UnrealClaude
- https://github.com/Natfii/unrealclaude-mcp-bridge
- Specifically aimed at UE 5.7, integrates Claude Code CLI into the editor
- 20+ MCP tools including Blueprint editing and Animation Blueprint work
- Caveat: README says Blueprint editing still has "few bugs still, don't rely on fully"
- MCP bridge can manipulate levels, actors, Blueprints, and Anim Blueprints through standard MCP

### 3. ChiR24/Unreal_mcp - More general Unreal MCP bridge (BROAD OPTION)

- https://github.com/ChiR24/Unreal_mcp
- Supports UE 5.0-5.7, has packaged releases
- Broad editor automation through a native C++ bridge
- More operationally mature than many hobby repos
- README emphasis is broader engine control, not as sharply Blueprint-graph-specific as option 1

## Recommendation for ALIS

### Quick path: test mirno-ehf/ue5-mcp first

- Most direct answer to "agents can't see BP"
- Centered on Blueprint asset access
- Avoids weak path of blind Unreal Python

### If unstable: build internal exporter (long-term robust path)

Epic supports editor automation via Python, Editor Utility Blueprints, Remote Control.
Engine exposes `FBlueprintEditorUtils` for graph creation/manipulation.

Stable architecture:
1. UE editor plugin extracts BP data
2. Emit JSON (nodes, pins, links, vars, functions, macros)
3. Optional MCP wrapper for Claude/Cursor/any client

Benefits: no vendor lock, loopback-only, exact schema for ALIS, easier diff/review.

## Implementation Plan

### Phase 1 - Sandbox test

- Throwaway ALIS branch or sandbox project
- Install mirno-ehf/ue5-mcp
- Keep bridge bound to 127.0.0.1 only
- Verify:
  - [ ] Read selected BP graph
  - [ ] Explain graph in text
  - [ ] Add variable + function node
  - [ ] Re-open and compile safely

### Phase 2 - Fallback: internal BP exporter

Only if phase 1 is unstable:
- Build internal BP graph exporter plugin
- Export JSON covering:
  - asset path, parent class
  - variables, functions
  - event graphs, nodes, pins, links
  - comments, compile status

### Phase 3 - MCP wrapper

- Add minimal MCP wrapper for read/search first
- Editing later, behind explicit confirmation

## Status

- [ ] Review each repo README, compare features
- [ ] Pick candidate for local test (current rec: mirno-ehf/ue5-mcp)
- [ ] Install UE plugin + configure MCP in `.claude/settings.json`
- [ ] Phase 1 validation (4 tasks above)
- [ ] Decision: use third-party or build internal exporter

## References

- Epic Blueprint API: https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI
- Epic Python scripting: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python
- FBlueprintEditorUtils: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/FBlueprintEditorUtils
