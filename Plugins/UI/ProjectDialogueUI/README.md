# ProjectDialogueUI

Dialogue panel widget and ViewModel for the ALIS dialogue system.

## Architecture

```
IDialogueService -> DialogueViewModel -> W_DialoguePanel
                    (auto_visibility)    (JSON layout)
```

- **DialogueViewModel**: Subscribes to `IDialogueService` via `FProjectServiceLocator`. Exposes `bIsActive`, `SpeakerName`, `DialogueText`, `Options` as VIEWMODEL_PROPERTY fields.
- **W_DialoguePanel**: Inherits `UProjectUserWidget`. Visual layout loaded from `Data/DialoguePanel.json`. Widget children resolved by `FProjectUIWidgetBinder`.
- **ui_definitions.json**: Registers panel with `vm_creation: "Global"`, `auto_visibility: "bIsActive"`, `layout_json: "DialoguePanel.json"`.

Option state rendering:
- `FDialogueOptionView.bLocked=true` -> option button disabled + greyed text (`(Unavailable)` suffix).
- `FDialogueOptionView.bConditionSatisfied=true` -> subtle green highlight for conditional choices that are currently available.

## Why UProjectUserWidget (not UUserWidget)

The original implementation inherited `UUserWidget` directly and used `BindWidgetOptional` + manual `SetDialogueViewModel()`. This broke in two ways:

1. **ViewModel injection failed silently** - `ProjectUIFactorySubsystem::InjectViewModel()` checks three paths: `IProjectViewModelConsumer`, `UProjectActivatableWidget`, `UProjectUserWidget`. Plain `UUserWidget` matches none, so `DialogueVM` stayed null and the panel collapsed itself.

2. **No visual content** - `BindWidgetOptional` requires a Blueprint subclass to populate widget references. Without one, `SpeakerText`, `DialogueText`, `OptionsContainer`, `ContinueButton` were all null.

The fix aligns with the project-wide data-driven UI pattern (same as `W_InventoryPanel`):
- `UProjectUserWidget` provides JSON layout loading, ViewModel lifecycle, and theme support
- `DialoguePanel.json` defines the widget tree declaratively
- `FProjectUIWidgetBinder` resolves children by name (type-safe, logged)
- No Blueprint widget needed

See `Plugins/UI/ProjectUI/Source/ProjectUI/Public/Widgets/ProjectUserWidget.h` for the base class contract.

## Files

| File | Purpose |
|------|---------|
| `Data/ui_definitions.json` | Panel registration (layer, VM, auto_visibility, layout) |
| `Data/DialoguePanel.json` | Visual layout (JSON widget tree) |
| `Source/.../MVVM/DialogueViewModel.h/.cpp` | ViewModel (service subscription, property notification) |
| `Source/.../Widgets/W_DialoguePanel.h/.cpp` | Widget (layout binding, option buttons, continue) |

## Modifying the Layout

Edit `Data/DialoguePanel.json`. The JSON format supports:
- Widget types: `CanvasPanel`, `VerticalBox`, `HorizontalBox`, `Border`, `TextBlock`, `Button`, `SizeBox`, etc.
- Anchors: `Fill`, `BottomCenter`, `Center`, `TopLeft`, etc.
- Theme refs: `font: "HeadingSmall"`, `color: "Text"`, `background: "Background"`
- Hot reload in editor (file watcher rebuilds layout on save)

See `Plugins/UI/ProjectInventoryUI/Data/InventoryPanel.json` for a complex layout example.
