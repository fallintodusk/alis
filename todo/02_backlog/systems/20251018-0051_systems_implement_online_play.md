# Implement Online Play

Status: uninvestigated

## Current gap

ProjectOnlinePlay is a disabled module shell. It does not currently provide a
GameMode, session service, matchmaking, party state, network travel, or online
UI integration.

## Investigation boundary

- Define the smallest supported online-play product route and backend first.
- Keep session/party capability contracts separate from game-mode composition.
- Reuse ProjectLoading for travel execution and ProjectUI owners for
  presentation without moving their state authority.
- Establish server authority, disconnect/rejoin, cancellation, and error
  translation before adding menu flows.
- Prove a local backend route before selecting an external service.

Do not document planned types or flows as implemented behavior.
