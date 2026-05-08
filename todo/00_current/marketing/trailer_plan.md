# ALIS Trailer — Index

60-second hero presentation video. Universal piece — website, GitHub social card, conference reels, store pages.

## Thesis

**The music keeps playing. So does he.**

Survival reframed as persistence under collapse. The world is broken AND beautiful — both true, always. Hero's continuity (he leaves, survives, returns) and the music's continuity (one PD track, broken under stress, reasserted at the end) are two mirrors of the same idea.

Contrast (beauty + horror) is the trailer's recurring mechanic, not a one-shot beat. Every shot carries it.

## Structure

One emotional arc, segmented into four production chunks for budgeting. Three parallel motifs (Hero / Music / World) all follow the same arc shape and converge at the end.

### One arc

```
peace → tension/showcase → chaos/climax → return → brand
```

### Four chunks (timeline budget)

| Chunk | t | Δ | What happens on the arc |
|---|---|---|---|
| **Open** | 0:00–0:09 | 9s | peace establish (gramophone, interior, Hero leaves) |
| **Body** | 0:09–0:42 | 33s | tension rises through 10 system shots, contrast in every frame |
| **Climax** | 0:42–0:48 | 6s | snap-cut barrage of 14 beauty/horror pairs; music breaks |
| **Close** | 0:48–0:60 | 12s | Hero returns, music reasserts, end card |

### Three parallel motifs

| Motif | peace | tension | chaos | return |
|---|---|---|---|---|
| **Hero** | home, calm | acts in city | shock | back home |
| **Music** | Chaliapin starts | Chaliapin holds | breaks | reasserts |
| **World** | shelter intact | ruins + systems | smash | shelter intact |

## Files in this folder

| File | Purpose |
|---|---|
| [trailer_plan.md](trailer_plan.md) | This index — concept, structure, README mapping, assets, audio policy, non-goals |
| [trailer_storyboard.md](trailer_storyboard.md) | 16-shot script with timing, per-shot direction, captions, snap-cut barrage, transitions, music arc, verification |
| [trailer_capture.md](trailer_capture.md) | UE5 capture workflow (Take Recorder + Movie Render Queue) |

## README pillars → trailer shots

Captions on each system shot use README phrasing directly — watching the trailer is reading the README in motion.

| README claim | Shot | Caption |
|---|---|---|
| "Open-Source UE5 Survival Game" | 0, 15 | wordmark + end card |
| "post-apocalyptic real-world city / real geography" | 3 | `CITY17 — post-apocalyptic real-world city` |
| **Vitals** — server-side metabolism, threshold states | 4 | `VITALS — metabolism · threshold states` |
| **Interaction** — trace and prompt | 5 | `INTERACTION — trace · prompt` |
| **Object Capabilities** — composable capability components | 6 | `OBJECT CAPABILITIES — composable · hinged · sliding` |
| **Inventory** — server-authoritative, weight, volume, depth stacking | 7 | `INVENTORY — server-authoritative · weight · depth stacking` |
| **Dialogue** — universal data-driven for NPCs, objects, terminals | 8 | `DIALOGUE — universal · NPCs · objects · terminals` |
| **Mind** — inner-voice thought guidance | 9 | `MIND — inner-voice thought guidance` |
| **Object Capabilities** — composable: lockable | 10 | `OBJECT CAPABILITIES — lockable` |
| **GAS** — Gameplay Ability System integration | 11 | `GAS — gameplay ability system` |
| **Motion** — skeleton-agnostic primitives | 12 | `MOTION — skeleton-agnostic primitives` |
| signed releases, public mirror, MIT | 15 | `MIT · Open Source · Signed` |

**Object Capabilities appears twice on purpose** (shots 6 and 10) — this is the README's "composable capability components" claim made visible: same system, different capabilities (hinged + sliding on shot 6's kitchen cabinet vs lockable on shot 10's locker). Same system shown two different ways in the same trailer is the most concrete way to demonstrate composability. Shot 6 also internally demonstrates composability: ONE cabinet object carries TWO capabilities (Hinged door 90° + Sliding drawer) which fire together.

**Not shown** (internal, hard to visualise without context): Loading 6-phase pipeline, Settings persistence, Character base plugin (implicit — Hero is everywhere). README claims that don't translate to a 3-second visual are honest to omit.

## Story constraints

- **No Grandpa NPC.** The entire current playable game is the Grandpa cigarette quest. Showing Grandpa or any quest beat (cigarette trade, KeyPlayerApartment, Luxury Apartment as goal) spoils 100% of available content.
- **Dialogue demo uses an OBJECT, not an NPC.** Shot 8 shows DialoguePanel rendered over an object interaction (door / terminal / gramophone) — this also hits a unique-to-ALIS README claim ("universal — for objects and terminals"), which most survival games can't match.
- **The interior with the gramophone is generic** — not the Luxury Apartment, not Grandpa's apartment. Place gramophone in any non-quest interior for the shoot.

## Production-ready assets

### World
- [City17_Persistent_WP.umap](../../../Plugins/World/City17/Content/Maps/City17_Persistent_WP.umap) — foliage instances restored to Jan 10 peak (commit `07865141d`).

### Open & Close (gramophone)
- [Gramophone.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/Gramophone.json) + [AUDIO_Gramophone.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/AUDIO_Gramophone.json) — `SW_FedorChaliapin_AlongThePetersburg` PD shellac
- [Cigarette.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Consumables/Relaxation/Cigarette.json) — already-lit, no ignition animation needed (Lighter ignition not implemented)

### Body (systems)
- [VitalsHUD.json](../../../Plugins/UI/ProjectVitalsUI/Data/VitalsHUD.json) + [VitalsPanel.json](../../../Plugins/UI/ProjectVitalsUI/Data/VitalsPanel.json)
- [InventoryPanel.json](../../../Plugins/UI/ProjectInventoryUI/Data/InventoryPanel.json)
- [DialoguePanel.json](../../../Plugins/UI/ProjectDialogueUI/Data/DialoguePanel.json) + an object-attached `DialogueTreeAsset` (gramophone or any non-quest door)
- **Object Capabilities composability demo:** [KitchenCabinet_Base_OneDoor_Drawer_Set_1.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Furniture/ReadySet/Kitchen_Set_1/Table_3/KitchenCabinet_Base_OneDoor_Drawer_Set_1.json) — one object with two capabilities (`Hinged` door 90° + `Sliding` drawer). Used for shots 5 (interaction target) and 6 (composability reveal). **Not** in the Grandpa quest loot loop.
- Lockable demo: [Locker_Steel.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Container/Locker/Locker_Steel.json) — placed in locked state for shot 10
- SniperZone hazard: `Plugins/Resources/ProjectObject/Content/HumanMade/Hazard/SniperZone/`

### Motion
- Locomotion: `Human.Default` (idle/walk/jog/sprint, MotionMatching)
- Traversal: `Human.Parkour` (vault, climb, slide)

### Props for contrast / snap-cut barrage (need placement)
- A corpse static mesh in a generic interior (shot 9 + barrage cut 2)
- Family photo prop (shot 6 + barrage cut 5)
- Child's stuffed toy (barrage cut 7)
- Bullet shells / casings on ground (barrage cut 4)
- Bullet hole decal on wall (barrage cut 6)
- Knife on kitchen table (barrage cut 8)
- Flowery wallpaper material on the wall used in shot 11

## Tech stack

| Layer | Tool |
|---|---|
| Footage capture | UE5 Take Recorder + Movie Render Queue → ProRes 422 HQ |
| Title cards + captions | Remotion (intro, end card, system captions, all ProRes 4444 alpha) |
| Master editor | DaVinci Resolve 20 free — manual editing |
| Batch ops | FFmpeg (Remotion-bundled n7.1) via Bash — concat, format conversion, aspect ratios, audio normalize |

DaVinci free does not allow external scripting (verified — segfaults on connection regardless of Python version). For automation: DaVinci Studio ($295), or stay manual. We chose manual.

## Output paths

| Artifact | Path | Tracked |
|---|---|---|
| Sequence assets | `Cinematics/Trailer/Trailer_<Theme>.uasset` | yes |
| MRQ renders | `Saved/Movies/Trailer/<theme>.mov` | no |
| Title and caption renders | `tools/trailer-titles/out/*.mov` | no |
| Final master | `docs/marketing/trailer_master_v1.mp4` | yes if ≤30 MB |

## Audio sourcing policy

The trailer ships under MIT. Audio must not contaminate that license.

**Allowed:**
- CC0 / Public Domain Mark
- Public domain by expired copyright (composer +70 years AND recording also PD/CC0 — verify both layers)
- Original commission with full rights assignment in writing

**Forbidden — even if the source labels them "free":**
- CC-BY, CC-BY-SA, CC-BY-NC, or any non-CC0 Creative Commons variant
- YouTube Audio Library tracks
- Royalty-free libraries with account/key/per-project license (Epidemic, Artlist, Musicbed)
- "Found on the internet" without verifiable license page
- PD compositions paired with copyrighted recordings

**Required artifacts** for every audio file used:
1. Source URL captured at download
2. License name and version
3. License text saved to `tools/trailer-audio/licenses/`
4. SHA-256 of the audio file at download

### For this trailer

**Single anchor track — Chaliapin PD shellac.** ALIS already ships gramophone tracks via [AUDIO_Gramophone.json](../../../Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/AUDIO_Gramophone.json):

| Track ID | Title | Asset | Use |
|---|---|---|---|
| `along_the_petersburg` | *Along The Petersburg* (Fedor Chaliapin) | `SW_FedorChaliapin_AlongThePetersburg` | **Primary anchor** — opens at shot 1, holds through body, breaks at climax, reasserts at close |

The trailer uses this directly. Source already cleared by the project for production.

**Climax cue (0:42–0:48) — TBD.** Two options to decide later:
- **Option A:** A separate CC0 percussive build for the snap-cut barrage. Candidates: FMA Komiku percussive tracks, Pixabay Music CC0 (per-track license verification required).
- **Option B (recommended):** Derivative work on Chaliapin itself — pitch-shift, bitcrush, vinyl-skip, layered CC0 percussion 1-shots. Same single PD source for the entire trailer; no second-track sourcing; thematically stronger ("the same music breaks under stress, then heals").

Music details locked at concept level only — exact climax cue and breaking technique chosen during edit, after first rough cut is in DaVinci.

## Non-goals

- No voiceover. The world speaks for itself.
- No real-world political imagery. Real geography hints only — never identifiable landmarks.
- No quest beats, no story characters (no Grandpa NPC, no quest items by name).
- No "Wishlist" / store CTA. This trailer is for the project, not a single distribution channel.
- No engine/middleware logos. ALIS wordmark and URL are the only branding.
