# ALIS Trailer — Shot-by-Shot Script (60s)

One emotional arc, 4 production chunks, 16 shots. Concept and asset paths in [trailer_plan.md](trailer_plan.md). Capture workflow per shot in [trailer_capture.md](trailer_capture.md).

| Field | Value |
|---|---|
| Length | 60 seconds |
| Shots | 16 (numbered 0–15) |
| Output | H.264 1080p60, ≤40 MB |
| End frame | `fall.is` · `github.com/<user>/alis` · `MIT · Open Source · Signed` |
| Visual approach | mostly first-person POV (matches `Hero.json defaultMode: FirstPerson`); 3 cinematic third-person breaks (shots 3, 12, 14) |
| Recurring mechanic | beauty + horror in every shot |
| System captions | 10 lower-third overlays in body, README phrasing |

---

## Chunk 1 — Open (0:00–0:09)

Peace establish. Audio anchor (gramophone) starts. Hero leaves shelter.

| # | t | Δ | Shot | Source | Audio |
|---|---|---|---|---|---|
| 0 | 0:00–0:02 | 2s | Black, fine grain → `ALIS` wordmark fades in | Remotion `IntroWordmark.tsx` | wind ambient |
| 1 | 0:02–0:06 | 4s | **Gramophone closeup** — rust, dust, scratched lacquer; Hero's hand enters frame; mechanical needle-arm SFX + vinyl scratch; Chaliapin starts | [Gramophone.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/Gramophone.json) + [AUDIO_Gramophone.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/AUDIO_Gramophone.json) | needle-arm + scratch SFX → **Chaliapin** (PD shellac) starts |
| 2 | 0:06–0:09 | 3s | Pull-back — interior reveals: dust in light shafts, curtain shadows, gramophone in frame; Hero (back-of-head) walks toward door | generic non-quest interior (NOT Luxury Apartment) | Chaliapin builds, soft footsteps |

---

## Chunk 2 — Body (0:09–0:42)

Tension rises through 10 system shots. Caption appears 0.3s after each shot starts, holds ~1.8s, fades 0.3s. Caption template: bold uppercase top line + sentence-case subtitle (README phrasing).

| # | t | Δ | Shot | Source | Caption | Contrast |
|---|---|---|---|---|---|---|
| 3 | 0:09–0:13 | 4s | Wide City17 — golden hour, ruined district, recognizable urban geometry | [City17_Persistent_WP](../../../Plugins/World/City17/Content/Maps/City17_Persistent_WP.umap) | `CITY17`<br>`post-apocalyptic real-world city` | golden sun + ruins |
| 4 | 0:13–0:16 | 3s | VitalsHUD ConditionBar pulses red → VitalsPanel slides in, Calories/Hydration/Fatigue dropping; quiet street in background | [VitalsHUD.json](../../../Plugins/UI/ProjectVitalsUI/Data/VitalsHUD.json) + [VitalsPanel.json](../../../Plugins/UI/ProjectVitalsUI/Data/VitalsPanel.json) | `VITALS`<br>`metabolism · threshold states` | serene background + bloody UI |
| 5 | 0:16–0:19 | 3s | Hero traces a kitchen cabinet → `[E] Open` prompt; abandoned kitchen, dust on the counter, dishes left mid-meal | [KitchenCabinet_Base_OneDoor_Drawer_Set_1.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Furniture/ReadySet/Kitchen_Set_1/Table_3/KitchenCabinet_Base_OneDoor_Drawer_Set_1.json) | `INTERACTION`<br>`trace · prompt` | mundane UX + abandoned home |
| 6 | 0:19–0:22 | 3s | Cabinet activates: **door swings open (Hinged 90°) AND drawer pulls out (Sliding) simultaneously**; contents revealed inside — old food cans, water bottle, family photo on the drawer floor. No keys, no quest items | same kitchen cabinet (composable: `Hinged` + `Sliding`) | `OBJECT CAPABILITIES`<br>`composable · hinged · sliding` | domestic ritual + decay inside |
| 7 | 0:22–0:25 | 3s | InventoryPanel — drag-drop items into 5×4 grid; weight bar climbs; depth-stacking visible (item-on-item) | [InventoryPanel.json](../../../Plugins/UI/ProjectInventoryUI/Data/InventoryPanel.json) | `INVENTORY`<br>`server-authoritative · weight · depth stacking` | system UI + cost of survival |
| 8 | 0:25–0:28 | 3s | DialoguePanel renders over an **object** (gramophone or non-quest door) — option list visible on UI; no NPC in frame | [DialoguePanel.json](../../../Plugins/UI/ProjectDialogueUI/Data/DialoguePanel.json) + object `DialogueTreeAsset` | `DIALOGUE`<br>`universal · NPCs · objects · terminals` | working UX + ruined object |
| 9 | 0:28–0:33 | 5s | **Hero walks sunny street, birds singing, leaves rustling**; inner-voice text overlay (no speaker): "...keep moving" → **MATCH-CUT: corpse on apartment floor**, dust in light, single ray through curtain | inner-voice text overlay pattern (cf. [DLG_Door_Handyman](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Building/Fenestration/Door/Scenario/DLG_Door_Handyman.json) — no speaker = inner monologue) + corpse static mesh placement | `MIND`<br>`inner-voice thought guidance` | **the trailer's core contrast beat** — birds singing + corpse |
| 10 | 0:33–0:36 | 3s | Locker_Steel locked — hand on handle, `Locked` indicator; child's drawing held to door with magnet | [Locker_Steel.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Container/Locker/Locker_Steel.json) | `OBJECT CAPABILITIES`<br>`lockable` | mundane lock + somebody's optimism |
| 11 | 0:36–0:39 | 3s | Sniper laser sweeps across **flowery wallpaper**, brick chips fly, Hero ducks into cover | [SniperZone](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Hazard/SniperZone/) | `GAS`<br>`gameplay ability system` | classical music holds + threat of death |
| 12 | 0:39–0:42 | 3s | Motion Matching — sprint, vault over fence, slide; whip-pan camera | Hero with `Human.Parkour` traversal profile | `MOTION`<br>`skeleton-agnostic primitives` | athletic mastery + worn-out world |

`OBJECT CAPABILITIES` appears twice (shots 6 and 10) deliberately — same system, different capabilities. Demonstrates "composable capability components" as a single system, not as one beat.

---

## Chunk 3 — Climax (0:42–0:48)

Snap-cut barrage. The world breaks; the music breaks. 14 cuts of ~0.4s each, beauty/horror pairs back-to-back. Music drops into a percussive distortion or a fast CC0 cue — see [trailer_plan.md audio policy](trailer_plan.md#for-this-trailer).

| Cut | Frame | Source | Vector |
|-----|---|---|---|
| 1 | sun ray through curtain | reuse from shot 2 / new still | beauty |
| 2 | corpse on floor | reuse from shot 9 (post-cut) | horror |
| 3 | butterfly on weed | new VFX / placement | beauty |
| 4 | bullet shells on ground | prop placement | horror |
| 5 | wedding photo in frame | prop placement | civilization |
| 6 | bullet hole in wall | decal placement | violence |
| 7 | child's stuffed toy in dust | prop placement | innocence |
| 8 | knife on kitchen table | prop placement | threat |
| 9 | Hero running, back-of-head | reuse from shot 12 | hero |
| 10 | sniper laser on wall | reuse from shot 11 | death |
| 11 | gramophone needle on disc | reuse from shot 1 | art |
| 12 | VitalsHUD red flash | reuse from shot 4 | mortality |
| 13 | Hero's reflection in window glass | reuse single frame from shot 14 | identity hint, not reveal |
| 14 | cigarette burning down | reuse from shot 14 (close) | time running out |

After cut 14: ~0.4s hard hold on black before close starts. No captions during barrage — no time to read.

8 cuts reuse main-trailer frames (~static stills extracted from existing shots); 6 cuts are new placements (props + a butterfly composite + a Hero face closeup).

---

## Chunk 4 — Close (0:48–0:60)

Hero returns; music reasserts; brand.

| # | t | Δ | Shot | Source | Audio |
|---|---|---|---|---|---|
| 14 | 0:48–0:55 | 7s | Hero in same interior as shot 2; at window with already-lit cigarette, smoke curling in dusk light. **Face visible for the first time as a reflection in the window glass** — Hero's face overlaid with the city outside through the dusty pane, half ghost / half man. No direct portrait, no face animation. Camera holds steady. | [Cigarette.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Consumables/Relaxation/Cigarette.json), same generic interior as shot 2; window with reflective glass material; Lumen HWRT or Path Tracer per [trailer_capture.md](trailer_capture.md) | brief silence (~0.4s) → faint Chaliapin returns, plays final phrase |
| 15 | 0:55–0:60 | 5s | End card: `fall.is` / `github.com/<user>/alis` / `MIT · Open Source · Signed` | Remotion `EndCard.tsx` | Chaliapin tail decays to silence |

---

## Music arc

Single anchor track (Chaliapin PD shellac) with a derivative-distorted climax. One instrument across the whole minute — three states.

| t | State | Audio |
|---|---|---|
| 0:00–0:03 | Silence | wind ambient only |
| 0:03 | Needle drop | mechanical SFX + vinyl scratch |
| 0:03–0:42 | **Chaliapin holds** | full PD recording, slightly muted under SFX during action |
| 0:42 | Pivot | vinyl scratch / record skip — Chaliapin distorts |
| 0:42–0:48 | **Chaos** | fast cue (TBD: derivative-distorted Chaliapin OR external CC0 percussive build); each barrage cut lands on a percussion hit |
| 0:48 | Breath | ~0.4s silence |
| 0:48–0:55 | **Reassert** | Chaliapin returns faintly, plays final phrase |
| 0:55–0:60 | Decay | tail to silence under end card |

Climax cue technique chosen during edit, after first rough cut is in DaVinci. Lock target: same single PD source carries all 60s.

---

## Caption template

System-shot lower-third overlay, rendered via Remotion to ProRes 4444 with alpha, composited over UE footage in DaVinci.

| Property | Value |
|---|---|
| Position | bottom-left, ~12% from left edge, ~12% from bottom |
| Top line | uppercase system name, project sans or monospace, white, ~24pt at 1080p |
| Bottom line | sentence-case description from README, ~16pt at 1080p, lighter weight |
| Separator | `·` between feature words, matches end-card style |
| Animation | fade-in 0.3s after shot start, hold 1.8s, fade-out 0.3s |
| Background | none (hero text only) or 50% black scrim if footage too busy |

---

## Per-shot direction notes

**Shot 1 (gramophone closeup)** — Macro lens (50mm), shallow DoF on the device. Camera worships the rust. Light rakes from window left, dust visible in the beam. **Audio is the moment** — the visual is a static frame; vinyl scratch + needle SFX cue the music start.

**Shot 2 (interior pull-back)** — Slow pull-back, Hero (back-of-head) passes through frame. Curtain shadows striped on floor. Reads as private moment, not gameplay. Hero's face is hidden — he's a viewpoint the audience inhabits, anyone could be him. Face is reserved for shot 14.

**Shot 3 (City17 wide)** — Same district that shows up later in the action sequence; locks geography. Beautiful music over dead world is the contrast.

**Shot 4 (Vitals)** — VitalsHUD ConditionBar pulses Warning state colors. VitalsPanel slides in showing the four metrics dropping. Background is intentionally peaceful (a quiet street, leaves moving) — UI and world disagree.

**Shot 5 (interaction prompt)** — Camera reads the prompt clearly. Set dressing is the contrast: dusty kitchen counter, dirty dishes left mid-meal, a half-cup of dried coffee. Mundane domestic life arrested.

**Shot 6 (composable capabilities)** — The cabinet's two capabilities fire **together** on the same beat: door swings 90° AND drawer pulls out (Sliding). Camera holds wide enough to read both motions in one frame. Inside the drawer: old food cans (stewed beef, peas — see project Food/Can assets), one empty water bottle, and a family photo lying face-up on the drawer floor. No keys, no cigarettes, no quest items — all loot association deliberately removed. The visible composability (one object = two capabilities firing together) is the literal demonstration of the README's "composable capability components" claim. Optional ~50% speed ramp on the drawer landing for emphasis.

**Shot 7 (inventory)** — Grid 5×4 (DuffleBag), drag-drop slow enough at 60fps to read. Weight bar climbing tells the story: "we're carrying his world, plus everyone else's."

**Shot 8 (object dialogue)** — DialoguePanel rendered cleanly over a non-NPC interaction. Use a generic door, terminal, or a second gramophone — anything with a `DialogueTreeAsset` that doesn't reveal Grandpa quest beats.

**Shot 9 (Mind + birds-corpse)** — The trailer's core thesis in one beat. ~1.5s of sunny street with bird ambient and inner-voice whisper text → match-cut to corpse interior, dust, single light shaft. Audio bridges: birds carry into corpse shot, then fade to room-tone, then Chaliapin reasserts.

**Shot 10 (Locker locked)** — Closeup on the lock indicator + child's drawing magnet. Hero's hand pulls — doesn't open. Camera doesn't try to "read" the lock; the indicator does the work.

**Shot 11 (sniper)** — Red laser dot on a beautiful flowery wallpaper, brick chips, sound design carries. Hero reacts physically — duck, roll. The wallpaper is the contrast — civilized interior, barbaric weapon.

**Shot 12 (motion matching)** — The "look at our tech" beat. Sprint, vault over fence, slide. Camera *not* stationary; whip-pans, low angles, follow-through. Speed ramp at vault apex.

**Shot 13 (snap-cut barrage)** — Each cut ~0.4s. No captions. Audio drives — every visual cut lands on a percussion hit. The world coming apart, compressed.

**Shot 14 (Hero at window)** — Slow down dramatically. Cigarette already lit (no ignition animation). Smoke curls in dusk backlight. Hero sits facing the window; **his face appears as a reflection in the dusty window glass**, layered over the city visible through it. No direct portrait, no face-animation rig. Two compositional layers in one frame: Hero's reflected face (foreground, dim, faintly distorted) + the city skyline (background, sharper). Hold without movement; let Chaliapin land its final phrase. **The reflection lands the trailer's "everyone could be him" framing** — viewer never confronts Hero directly, only through glass. This preserves the inhabitable POV that opened the trailer.

Render with Lumen HWRT Hit Lighting (cheap path) or Path Tracer (cinematic-clean reflections, fine for a 7-second static hold) per [trailer_capture.md](trailer_capture.md).

**Shot 15 (end card)** — Hold 5 seconds. URL must be readable on a phone screen at arm's length. Caption stack matches body-shot caption style — visual coherence between system reveals and project identity.

---

## Transitions

Default is hard cut. Specific transitions only where listed.

| Boundary | Transition |
|---|---|
| 0 → 1 | 0.6s cross-dissolve from black through grain |
| 2 → 3 | hard cut on the audio Chaliapin first downbeat — interior to exterior |
| 8 → 9 | hard cut into birds ambient |
| 9 (within shot) | match-cut on the inner-voice last word — sunny street to corpse interior |
| 12 → 13 | hard cut on percussion accent — Chaliapin breaks |
| 13 → 14 | 0.4s hard hold on black after final barrage cut, then fade to close |
| 14 → 15 | 1.0s fade to black |

## Speed ramps

Five marked moments. All ramps eased curves (DaVinci → Retime → Ease In/Out), never linear. Source 60fps.

| Where | Ramp | Purpose |
|---|---|---|
| Shot 1, last 0.3s before needle drop | 100% → 60% | landing the moment |
| Shot 6, drawer reaching open position | 100% → 50% on drawer landing | sells the composable-capability moment |
| Shot 9, transition to corpse cut | 100% → 70% on last 0.3s of birds | breath before shock |
| Shot 12, parkour vault peak | 100% → 60% at apex | hero moment |
| Shot 13, snap-cut barrage | each cut compressed to 0.4s | breakneck pacing |

---

## Production approach

Storyboard is the **editing target, not a strict shooting plan**. Capture per shot per [trailer_capture.md](trailer_capture.md). If raw footage delivers a better arc, the edit wins. The thesis ("the music keeps playing, so does he") and the contrast mechanic must be readable in the final cut even if specific shots get reordered or replaced.

---

## Verification checklist

- [ ] Final timeline length 58–62 seconds
- [ ] All UE5 footage 1920×1080 60fps, ProRes source
- [ ] Hero appears assembled (clothed, MutableCustomization applied) — not default mannequin
- [ ] Chaliapin is the only musical source; climax cue is a derivative or one CC0 track, license documented
- [ ] Shot 1 audio: needle-arm SFX + vinyl scratch overlaid; Chaliapin starts on the scratch
- [ ] Shot 9: birds ambient audio bridges into corpse cut, then fades to room tone
- [ ] No frame above 95% luma or below 5% holds longer than 0.5s
- [ ] City17 district recognizable across atmospheric and action shots
- [ ] InventoryPanel grid items readable at 1080p
- [ ] VitalsHUD warning color matches `VitalsHUD.json` Warning state
- [ ] Each system caption uses README phrasing exactly
- [ ] Caption position bottom-left does not overlap UI on shots 4 and 7
- [ ] `OBJECT CAPABILITIES` caption appears twice (shots 6 and 10) — composability demonstrated
- [ ] No Grandpa NPC, no `KeyPlayerApartment`, no Luxury Apartment named or visually identifiable
- [ ] Gramophone interior in shots 1, 2, 14 is generic — not the quest goal
- [ ] Hero face appears only as a window reflection in shot 14 (and reused single-frame in barrage cut 13) — never as a direct portrait, no earlier reveals
- [ ] Snap-cut barrage has 14 cuts, all under 0.45s, music synced to cut points
- [ ] End-card URL readable on phone at arm's length
- [ ] Every speed ramp uses an eased curve (no linear)
- [ ] All audio cleared per [audio sourcing policy](trailer_plan.md#audio-sourcing-policy) — both composition and recording layers PD/CC0
