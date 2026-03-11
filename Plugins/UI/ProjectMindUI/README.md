# ProjectMindUI

Mind UI plugin following the common UI plugin pattern:
- data-driven registration via `Data/ui_definitions.json`
- MVVM widget + view model in `Source/ProjectMindUI`
- no direct dependency on ProjectMind internals, only `IMindService` from ProjectCore

Flow:
- `IMindService` -> `UMindThoughtViewModel` -> `UW_MindThoughtToast`
- `IMindService` -> `UMindJournalViewModel` -> `UW_MindJournalPanel` (toggle from player `J` action)
- journal panel tabs:
- `Important` for durable records
- `Recent Log` for temporary events
- HUD toast format:
- header for inner voice context
- source/meta line from thought source type
- concise body line with TTL-based auto-hide
