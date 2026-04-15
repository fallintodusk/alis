# Add Audio Capability with Dialogue Action Dispatch

## Architecture Decision

**Pattern:** Dialogue dispatches generic actions outward. Audio receives them via interface.
**Key constraint:** ProjectObjectCapabilities (Gameplay) MUST NOT depend on ProjectDialogue (Feature).

```
ProjectCore (foundation)
  IProjectActionReceiver    <- generic interface, not dialogue-specific

ProjectDialogue (feature, depends on Core)
  Parses node "actions" array
  On node enter: iterates IProjectActionReceiver on same actor, calls HandleAction()
  On conversation end: calls HandleAction with "$end" context

ProjectObjectCapabilities (gameplay, depends on Core only)
  AudioCapabilityComponent implements IProjectActionReceiver
  Loads UAudioPresetDefinition from AUDIO_*.json (via generator)
  Parses "audio.*" actions, ignores the rest
```

Dependency flow: Dialogue -> Core <- Audio (no Dialogue <-> Audio coupling)

---

## Step 1: IProjectActionReceiver in ProjectCore

**File:** `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IProjectActionReceiver.h`

```cpp
UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class PROJECTCORE_API UProjectActionReceiver : public UInterface
{
    GENERATED_BODY()
};

class PROJECTCORE_API IProjectActionReceiver
{
    GENERATED_BODY()
public:
    // Context: node ID, "$conversation_end", or other system marker
    // Action: namespaced string e.g. "audio.play:classical_01", "anim.open_lid"
    virtual void HandleAction(const FString& Context, const FString& Action) = 0;
};
```

- PROJECTCORE_API on both types (safe cross-module export, avoids MinimalAPI link issues)
- Generic: any capability can implement it
- Namespaced actions: each receiver filters by prefix (e.g. "audio.")
- Context is the node ID (or "$conversation_end" for end events)

---

## Step 2: Add `actions` to dialogue schema + C++ types

### 2a. Dialogue schema update

**File:** `Plugins/Features/ProjectDialogue/Data/Schemas/dialogue.schema.json`

Add to node properties (inside additionalProperties.properties, after "next"):
```json
"actions": {
  "type": "array",
  "items": { "type": "string" },
  "description": "Generic action strings dispatched on node entry. Namespaced: audio.play:id, anim.open_lid"
}
```

### 2b. FDialogueNode struct update

**File:** `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Public/Data/ProjectDialogueTypes.h`

Add to FDialogueNode:
```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
TArray<FString> Actions;
```

Generator auto-parses this (FJsonObjectConverter handles TArray<FString> natively).

### 2c. Bump generator version

In `dialogue.schema.json` x-alis-generator block:
```json
"GeneratorVersion": 2
```

This triggers regeneration of all DLG_*.json assets to pick up the new Actions field.

---

## Step 3: Dialogue dispatches actions in NavigateToNode()

**File:** `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Public/Components/ProjectDialogueComponent.h`

Add private declaration:
```cpp
void DispatchActions(const FString& Context, const TArray<FString>& Actions);
```

**File:** `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Private/Components/ProjectDialogueComponent.cpp`

In `NavigateToNode()` (after setting CurrentNodeId and broadcasting OnNodeChanged):
```cpp
// Dispatch node actions to receivers on same actor
if (Node->Actions.Num() > 0)
{
    DispatchActions(CurrentNodeId, Node->Actions);
}
```

In `EndConversation()`:
```cpp
// Notify action receivers that conversation ended
TArray<FString> EndActions;
EndActions.Add(TEXT("$end"));
DispatchActions(TEXT("$conversation_end"), EndActions);
```

New helper method (uses ImplementsInterface for safe cross-module cast):
```cpp
void UProjectDialogueComponent::DispatchActions(const FString& Context, const TArray<FString>& Actions)
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    TInlineComponentArray<UActorComponent*> Components;
    Owner->GetComponents(Components);

    for (UActorComponent* Comp : Components)
    {
        if (!Comp || !Comp->GetClass()->ImplementsInterface(UProjectActionReceiver::StaticClass()))
        {
            continue;
        }

        IProjectActionReceiver* Receiver = Cast<IProjectActionReceiver>(Comp);
        if (!Receiver)
        {
            continue;
        }

        for (const FString& Action : Actions)
        {
            Receiver->HandleAction(Context, Action);
        }
    }
}
```

**Module dependency:** ProjectDialogue.Build.cs already depends on ProjectCore -> IProjectActionReceiver available.
Add `#include "Interfaces/IProjectActionReceiver.h"` to the .cpp.

---

## Step 4: Audio preset definition + schema + generator

### 4a. Audio types

**File:** `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Audio/AudioTypes.h`

```cpp
USTRUCT(BlueprintType)
struct FAudioTrack
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<USoundBase> Sound;
};
```

### 4b. Audio preset definition

**File:** `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Audio/AudioPresetDefinition.h`

```cpp
UCLASS(BlueprintType)
class PROJECTOBJECTCAPABILITIES_API UAudioPresetDefinition : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    FName AudioId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    TArray<FAudioTrack> Tracks;

    // Generator metadata
    UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
    bool bGenerated = false;
    UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
    int32 GeneratorVersion = 0;
    UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
    FString SourceJsonPath;
    UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
    FString SourceJsonHash;

    const FAudioTrack* FindTrack(const FString& TrackId) const;
};
```

### 4c. Audio schema (auto-discovered by generator)

**File:** `Plugins/Gameplay/ProjectObjectCapabilities/Data/Schemas/audio.schema.json`

Generator config mirrors dialogue.schema.json (verified working):
- PluginName=ProjectObject + SourceSubDir=../Content -> scans ProjectObject/Content/** for AUDIO_*.json
- Generated assets go to /ProjectObject/** mirroring source hierarchy

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "audio.schema.json",
  "title": "Audio Preset Definition",
  "description": "Schema for ALIS audio presets. Field names must match USTRUCT property names (case-insensitive) for FJsonObjectConverter auto-parsing. Every AUDIO_*.json file MUST include a $schema field.",

  "x-alis-generator": {
    "TypeName": "Audio",
    "DefinitionClass": "/Script/ProjectObjectCapabilities.AudioPresetDefinition",
    "PluginName": "ProjectObject",
    "SourceSubDir": "../Content",
    "SourceFileGlob": "AUDIO_*.json",
    "GeneratedContentPath": "/ProjectObject",
    "IdPropertyName": "AudioId",
    "GeneratorVersion": 1
  },

  "type": "object",
  "required": ["id", "tracks"],
  "additionalProperties": false,
  "properties": {
    "$schema": { "type": "string" },
    "id": {
      "type": "string",
      "pattern": "^AUDIO_[A-Za-z][A-Za-z0-9_]*$"
    },
    "tracks": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["id", "name", "sound"],
        "additionalProperties": false,
        "properties": {
          "id": { "type": "string" },
          "name": { "type": "string" },
          "sound": { "type": "string", "description": "Asset path to USoundBase (full form: /Mount/Path/Asset.Asset)" }
        }
      }
    }
  }
}
```

---

## Step 5: AudioCapabilityComponent

**File:** `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Audio/AudioCapabilityComponent.h`
**File:** `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Private/Audio/AudioCapabilityComponent.cpp`

```cpp
UCLASS(ClassGroup = (Capabilities), meta = (BlueprintSpawnableComponent))
class PROJECTOBJECTCAPABILITIES_API UAudioCapabilityComponent
    : public UActorComponent
    , public IProjectActionReceiver
{
    GENERATED_BODY()
public:
    virtual void HandleAction(const FString& Context, const FString& Action) override;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    // Returns: FPrimaryAssetId("CapabilityComponent", "Audio")

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<UAudioPresetDefinition> AudioPresetAsset;

private:
    void PlayTrack(const FString& TrackId);
    void StopTrack();

    UPROPERTY()
    TObjectPtr<UAudioComponent> ActiveAudioComp;

    // Built once from preset in BeginPlay: TrackId -> SoftPtr
    TMap<FString, TSoftObjectPtr<USoundBase>> TrackMap;
};
```

HandleAction filters by namespace:
```cpp
void UAudioCapabilityComponent::HandleAction(const FString& Context, const FString& Action)
{
    if (Action == TEXT("$end"))
    {
        StopTrack();
        return;
    }
    if (!Action.StartsWith(TEXT("audio.")))
    {
        return;
    }

    FString Command = Action.Mid(6); // strip "audio."
    if (Command == TEXT("stop"))
    {
        StopTrack();
    }
    else if (Command.StartsWith(TEXT("play:")))
    {
        FString TrackId = Command.Mid(5);
        PlayTrack(TrackId);
    }
}
```

BeginPlay: sync-loads AudioPresetAsset, builds TrackMap from Tracks array.
PlayTrack: looks up TrackId in TrackMap -> async loads Sound -> SpawnSoundAttached on owner.
StopTrack: stops and destroys ActiveAudioComp.
EndPlay: calls StopTrack() for cleanup.

---

## Step 6: Data files

### 6a. AUDIO_Gramophone.json (NEW)

**File:** `Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/AUDIO_Gramophone.json`

Sound paths use full object form (generator normalizes short paths, but explicit is safer):
```json
{
  "$schema": "../../../../../../../Gameplay/ProjectObjectCapabilities/Data/Schemas/audio.schema.json",
  "id": "AUDIO_Gramophone",
  "tracks": [
    { "id": "katyusha", "name": "Katyusha", "sound": "/Game/Audio/Music/Gramophone_Katyusha.Gramophone_Katyusha" },
    { "id": "dark_eyes", "name": "Dark Eyes", "sound": "/Game/Audio/Music/Gramophone_DarkEyes.Gramophone_DarkEyes" }
  ]
}
```

### 6b. Gramophone.json (EDIT - add Audio capability)

```json
"capabilities": [
  {
    "type": "Dialogue",
    "scope": ["actor"],
    "properties": {
      "DialogueTreeAsset": "/ProjectObject/HumanMade/Device/Music/Rarity/DLG_Gramophone_Main.DLG_Gramophone_Main"
    }
  },
  {
    "type": "Audio",
    "scope": ["actor"],
    "properties": {
      "AudioPresetAsset": "/ProjectObject/HumanMade/Device/Music/Rarity/AUDIO_Gramophone.AUDIO_Gramophone"
    }
  }
]
```

### 6c. DLG_Gramophone_Main.json (EDIT - add actions to nodes)

```json
"playing": {
  "text": "The gramophone begins playing...",
  "actions": ["audio.play:katyusha"],
  "next": "menu"
},
"paused": {
  "text": "The gramophone stops.",
  "actions": ["audio.stop"],
  "next": "menu"
},
"song_katyusha": {
  "text": "Now playing: Katyusha",
  "actions": ["audio.play:katyusha"],
  "next": "menu"
},
"song_dark_eyes": {
  "text": "Now playing: Dark Eyes",
  "actions": ["audio.play:dark_eyes"],
  "next": "menu"
}
```

---

## Acceptance Checks

- [ ] ProjectObjectCapabilities.Build.cs does NOT list ProjectDialogue
- [ ] Gramophone: "Play" node -> audio.play:katyusha -> music plays in 3D
- [ ] Gramophone: "Pause" node -> audio.stop -> music stops
- [ ] Gramophone: selecting composition switches track
- [ ] Conversation end (E toggle or Leave) -> $end action -> music stops
- [ ] Non-audio actions ignored (no crashes from unknown namespace)
- [ ] Generator creates UAudioPresetDefinition from AUDIO_Gramophone.json
- [ ] Dialogue schema validates with new actions field

---

## File Summary

| File | Action | Plugin |
|------|--------|--------|
| IProjectActionReceiver.h | NEW | ProjectCore |
| ProjectDialogueTypes.h | EDIT (add Actions to FDialogueNode) | ProjectDialogue |
| dialogue.schema.json | EDIT (add actions field, bump version) | ProjectDialogue |
| ProjectDialogueComponent.h | EDIT (add DispatchActions decl) | ProjectDialogue |
| ProjectDialogueComponent.cpp | EDIT (dispatch in NavigateToNode/EndConversation) | ProjectDialogue |
| AudioTypes.h | NEW | ProjectObjectCapabilities |
| AudioPresetDefinition.h | NEW | ProjectObjectCapabilities |
| AudioPresetDefinition.cpp | NEW | ProjectObjectCapabilities |
| AudioCapabilityComponent.h | NEW | ProjectObjectCapabilities |
| AudioCapabilityComponent.cpp | NEW | ProjectObjectCapabilities |
| audio.schema.json | NEW | ProjectObjectCapabilities |
| AUDIO_Gramophone.json | NEW | ProjectObject (content) |
| Gramophone.json | EDIT (add Audio capability) | ProjectObject (content) |
| DLG_Gramophone_Main.json | EDIT (add actions to nodes) | ProjectObject (content) |

---

## Known Limitations / Future Work

- **Replication:** Currently single-player only (action dispatch runs where NavigateToNode runs). For shared world audio (nearby players hear gramophone), needs server-authoritative track state with OnRep. Defer until multiplayer scope.
- **Fade support:** Add `audio.fadeout:<seconds>` action parsing when needed.
- **Sound assets:** /Game/Audio/Music/* must be imported into UE separately (WAV/OGG). For placeholder testing use any existing project sound.
- **Dynamic track injection:** If jukebox needs to populate dialogue options from track list at runtime (avoid DLG/AUDIO duplication), add IDialogueOptionsProvider in ProjectCore. Deferred - not needed for static menus.
