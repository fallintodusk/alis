# ProjectDialogue TODO

## Completed

- [x] Tree-based dialogue component (FDialogueNode, FDialogueOption, UDialogueTreeDefinition)
- [x] JSON schema + auto-generator integration (dialogue.schema.json)
- [x] IDialogueService interface in ProjectCore
- [x] FDialogueServiceImpl (ServiceLocator bridge)
- [x] FDialogueInteractionHandler (press E triggers dialogue)
- [x] Module startup wiring (service + handler registration)
- [x] ProjectDialogueUI plugin (ViewModel + Widget)
- [x] Unit tests (data structures + component navigation)
- [x] Example JSON dialogues (NPC + gramophone)

## Future

- [ ] Localization support (FString -> FText via string tables)
- [ ] Audio clip references (optional field in JSON)
- [ ] State persistence (track visited nodes, selected choices)
- [ ] Quest integration (conditions beyond GameplayTags)
- [ ] Visual editor (optional, for non-programmer content authors)
- [ ] Animation/camera triggers (OnNodeChanged callbacks)
- [ ] File splitting ($include for large dialogue trees)
