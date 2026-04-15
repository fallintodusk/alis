# Multiplayer Roadmap (Phase 8)

**Goal:** Ship a reusable multiplayer stack (sessions, parties, matchmaking) that plugs into the existing loading pipeline and UI without reinventing engine systems.

## 8.1 — Create `ProjectOnline` module (Plugins/Online/ProjectOnline)
- [ ] Add module boilerplate + Build.cs (deps: `OnlineSubsystem`, `OnlineSubsystemUtils`, `OnlineServices`, `Sockets`, `NetCore`, `ProjectCore`)
- [ ] Implement `UProjectOnlineSubsystem` (GameInstance subsystem wrapping OSSv1/OSSv2)
  - [ ] Async APIs: `CreateSession`, `FindSessions`, `JoinSession`, `DestroySession`, `StartMatchmaking`, `CancelMatchmaking`, `SendInvite`, `AcceptInvite`, `SetPresence`
  - [ ] Translate between project structs (`FProjectSessionDescriptor`, `FProjectPartyState`, `FProjectPlayerStatus`) and provider handles
  - [ ] Error translation table (provider codes → project enums/messages)
  - [ ] Config loader (`ProjectOnlineDevSettings`, EOS/Null config overrides)
- [ ] Register with `FProjectServiceLocator` + expose Blueprint helper library

## 8.2 — Integrate with loading subsystem (`ProjectLoading`)
- [ ] Extend `FLoadRequest` with optional session descriptor + travel intent
- [ ] Add `OnTravelRequested` multicast + network telemetry fields to `UProjectLoadingSubsystem`
- [ ] Update `FTravelPhaseExecutor` to:
  - [ ] Wait for session create/matchmaking callbacks before issuing travel
  - [ ] Coordinate party readiness + content-pack compliance prior to travel
  - [ ] Support host migration (new host re-creates session, clients auto-travel)
- [ ] Ensure content-pack mounting (Phase 2) completes before join/host travel

## 8.3 — Lobby & party UI (`ProjectMenuUI`)
- [ ] Implement `UProjectLobbyViewModel` (roster, readiness, matchmaking status)
- [ ] Build lobby screen: roster list, ready toggles, invite button, host controls, content-pack compliance indicator
- [ ] Add matchmaking modal + `OnMatchmakingStatus` status feed
- [ ] Provide Null-backend friendly fallbacks (offline/solo flows stay usable)

## 8.4 — Multiplayer validation
- [ ] Unit tests for session/matchmaking adapters (mock OSS providers)
- [ ] Functional tests (Null backend) covering create/find/join/leave flows
- [ ] GAuntlet scenario: dedicated server + two clients (create lobby, ready, start match, disconnect/rejoin)
- [ ] Telemetry integration (record matchmaking/join durations via `ProjectSessionSubsystem`)
