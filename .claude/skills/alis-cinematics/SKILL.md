---
name: alis-cinematics
description: >-
  Use for capturing footage of ALIS worlds - trailer material, release video, a
  screenshot board, a camera move, "make a video of Manhattan or Kazan". Owns the
  ALIS specifics: which worlds exist, where the capture route and its constraints
  are documented, and the commands. Pair it with the cinematic-capture-director
  skill, which owns the craft of directing a shot. Do not use for montage, audio,
  titles, colour grade, or for engine or plugin work that merely touches
  cinematics.
metadata:
  author: alis-team
  version: "1.0"
---

# ALIS cinematics

This skill is the ALIS routing half of a pair. Load
`cinematic-capture-director` for camera and editorial craft. The stable owners below
hold all ALIS-specific procedure, constraints, and commands; this skill does not copy
them.

## Read first

| Need | Open |
|---|---|
| Style: what the footage claims, composition, motion, what we never do | `docs/cinematics/visual_language.md` |
| Numbers: speed, pitch floors, diagonal rule, water, the takes | `scripts/ue/cinematic/shots/README.md` |
| The capture route, its traps, shot-plan fields | `docs/cinematics/raw_capture.md` |
| Release-bound renders, Shipping boundary | `Plugins/Editor/ProjectCinematic/README.md` |
| Authenticated scouting stills | `tools/World/VisualVerification/README.md` |
| Delivery encode | `docs/cinematics/ffmpeg.md` |

Follow the command and evidence boundary in the selected owner. Raw capture ends at
an editable clip for human montage; a Candidate-bound release render uses the
ProjectCinematic route. Do not move facts between those owners or into this router.
