# Character Collision Policy

Collision capsule sizing and traversal mechanics for DefinitionCharacter.

---

## Canonical Locomotion Capsule

| Param | Value | Notes |
|-------|-------|-------|
| Standing radius | 23 | 46 cm diameter |
| Standing half-height | 88 | 176 cm total |
| Crouched half-height | 60 | Same width, shorter |
| Crouch width change | None | Crouch affects height only, never width |

Radius 23 is the proven ALIS baseline inherited from legacy AProjectCharacter.
Existing levels were authored around this capsule. UE5 default (34) is
substantially wider -- ALIS is already on the slim side by engine standards.

UE Character collision uses a vertically aligned capsule.
Strafing does not reduce collision width.

---

## Level Design Metrics

### Hard minimum passability (bare edge-case clearance)

- Doorways: 56 cm
- Corridors: 56 cm
- Walkable furniture gaps: 56 cm

55 cm for a 46 cm capsule is only 4.5 cm clearance per side.
That works only with clean, straight geometry and no trims.

### Preferred authored spaces (comfortable normal movement)

- Doorways: 60-65 cm
- Corridors: 60-70 cm
- Walkable furniture gaps: 60 cm minimum

### Not normal locomotion routes

- Spaces narrower than 46 cm require squeeze traversal (see below)
- Do not design sub-46cm gaps as regular walk paths

---

## Navigation Policy

- If a character uses navigation/path following, nav agent radius and
  height should match the movement capsule
- If `UpdateNavAgentWithOwnersCollision` is enabled (UE default), Unreal
  syncs nav agent radius/height from capsule automatically
- Do not rely on dynamic squeeze radius as a general nav solution;
  authored squeeze traversal should use dedicated nav links instead

---

## What the Capsule Does NOT Solve

- Hands/arms clipping through walls (mesh is wider than capsule)
- Body presentation during wall brushing
- Fitting through gaps narrower than capsule diameter
- Cover usefulness (depends on height, camera exposure, peek rules)

---

## Future: Squeeze Traversal Mechanic

For gaps narrower than 46 cm (e.g. 30-40 cm), the golden standard is a
dedicated traversal state, not a smaller capsule.

### Why not just shrink the capsule?

- UE capsule is always vertically aligned, strafing does not make it thinner
- Capsule < 20 radius breaks cover, melee spacing, AI, hit registration
- Motion matching cannot solve collision width (only visual presentation)
- "Magic thin body" feels wrong in a realism-first survival game

### Squeeze traversal rules

- Enter only through authored traversal markers / trigger volumes
- Never auto-activate from generic wall contact
- While squeezing:
  - lock movement to the traversal axis
  - disable jump
  - disable free turning or heavily limit yaw
  - optionally disable step-up
- Exit only after a full-size capsule clearance test passes
- If restore test fails, remain in squeeze state (prevents unstable overlap)

### Implementation sketch

```cpp
void ADefinitionCharacter::EnterSqueeze(float NewRadius)
{
    GetCapsuleComponent()->SetCapsuleRadius(NewRadius, true);
    GetCharacterMovement()->SetMovementMode(MOVE_Custom, ECustomMove::Squeeze);
    // lock input to forward/back only
    // play squeeze montage
}

void ADefinitionCharacter::ExitSqueeze()
{
    // Clearance test before restoring full capsule
    FCollisionShape TestShape = FCollisionShape::MakeCapsule(23.f, 88.f);
    if (GetWorld()->OverlapBlockingTestByChannel(..., TestShape, ...))
    {
        return; // still blocked, stay in squeeze
    }
    GetCapsuleComponent()->SetCapsuleRadius(23.f, true);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}
```

### Priority

Low -- only needed when level design requires sub-46cm passages.
Current levels work with radius 23.

---

## Future: Arm/Hand Wall Clipping

Separate from capsule collision. The visible mesh (especially arms in
first-person) extends beyond the capsule and clips through walls.

Approaches to investigate:
- IK arm retraction when near walls (trace from shoulder, pull elbow in)
- Material-based near-wall fade on extremities
- Animation-driven arm tuck when close to surfaces
- Collision channel on arm bones (expensive, last resort)

### Priority

Medium -- visible in tight spaces, but not blocking gameplay.

---

## References

- UE5 default ACharacter capsule: radius 34, halfHeight 88
- UE Character capsule is vertically aligned, used as movement collision primitive
- UE crouch: changes crouched half-height only, not radius
- Dynamic capsule resize: `SetCapsuleSize()` / `SetCapsuleRadius()`
- Nav agent sync: `UpdateNavAgentWithOwnersCollision` property
