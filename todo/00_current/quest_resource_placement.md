# Quest Resource Balancing & Placement

> **Full plan:** `.claude/plans/serialized-singing-catmull.md`

## Corrections Applied
- Phase 0 kcal corrected: ~23 kcal → ~28 kcal (MET math verified)
- Hydration OK milestone noted: First OK (~78%) at Phase 6
- Full metabolic tracking added for all phases (critical path + with optional items)

---

## Tasks

### Phase 0: Courtyard — Survival
- [ ] Place mushrooms x2 near fallen tree by the grey crossover
- [ ] Place WaterBottleSmall (150 mL) under driver seat of crashed sedan
- [ ] Place WaterBottle (30 mL dregs) in cup holder of the crossover
- [ ] Place metal lever/pipe by the tank tread
- [ ] (Opt.) WaterBottle in glove compartment of locked car
- [ ] (Opt.) Medkit in glove compartment of crashed sedan

### Phase 1: Old Man's Apartment
- [ ] Place BreadBig (1000 kcal) on kitchen table, on cutting board wrapped in cloth
- [ ] Place WaterBottle (30 mL) on nightstand by the bed
- [ ] Set up bed as sleep point (fatigue 70→30%)
- [ ] Set up dialogue: sacrifice WaterBottleSmall to old man for entry
- [ ] (Opt.) BreadSlice (125 kcal) inside kitchen cabinet

### Phase 2: Cigarette Hunt in Stairwell
- [ ] Place cigarette x1 inside electrical panel on landing
- [ ] Place cigarette x1 in dead flower pot on windowsill
- [ ] Set up dialogue: old man gives door code after receiving cigarettes
- [ ] (Opt.) WaterBottle (30 mL) behind curtain on windowsill
- [ ] (Opt.) BreadBlackQuarters (250 kcal) on newspaper one floor below

### Phase 3: Addicts' Apartment
- [ ] Place crowbar stuck in corpse in corridor (heavy interaction)
- [ ] Place ChiliConCarne (295 kcal) on kitchen counter + lighter nearby
- [ ] Place WaterBottleMedium (300 mL) on bathtub edge in bathroom
- [ ] Place fallen mailboxes with key + note in ground floor lobby
- [ ] (Opt.) FishPaste (765 kcal) in upper wall cabinet in kitchen
- [ ] (Opt.) WaterBottle (30 mL) under couch
- [ ] (Opt.) Cigarettes x3 in crumpled pack on floor

### Phase 4: Repairman's Apartment (1st Floor)
- [ ] Place cigarette packet (20 pcs) in work jacket pocket by front door
- [ ] Place BraisedBeans (360 kcal) in kitchen junk drawer next to batteries
- [ ] Place WaterBottleSmall (150 mL) on workbench in workshop
- [ ] Place lamp on workbench
- [ ] (Opt.) Cigarettes x2 inside toolbox
- [ ] (Opt.) BreadSlice (125 kcal) in non-functional refrigerator
- [ ] (Opt.) WaterBottle (30 mL) in mug on counter

### Phase 5: Trade with Old Man
- [ ] Set up dialogue: trade cigarette packet for family apartment key

### Phase 6: Family Apartment
- [ ] Place backpack on coat rack by front door
- [ ] Place WaterBottleBig (500 mL) in kitchen refrigerator
- [ ] Place BreadBlackQuarters (250 kcal) on kitchen table
- [ ] Place cigarettes x5 in ashtray on living room shelf
- [ ] Place flashlight in hallway console table drawer
- [ ] (Opt.) StewedBeef (785 kcal) in pantry/upper cabinet
- [ ] (Opt.) WaterBottleSmall (150 mL) in child's room on nightstand by stuffed animal
- [ ] (Opt.) Hammer under kitchen sink in bucket with cleaning supplies
- [ ] (Opt.) Medkit in bathroom medicine cabinet

### Verification
- [ ] Run kcal/hydration calculation per phase (calculator check)
- [ ] Confirm no phase drops below Critical (20%) on critical path
- [ ] Confirm hydration stays LOW until Phase 6, then OK (~78%)
- [ ] Playtest full 35-min loop in editor
- [ ] Verify old man dialogue correctly consumes water bottle from inventory

---

## Metabolic Tracking

### Reference Data
| Activity | METs | kcal/min | Water ml/min |
|----------|------|----------|--------------|
| Rest | 1.3 | 1.63 | 1.63 |
| Walking | 2.5 | 3.13 | 3.13 |
| Jogging | 6.0 | 7.50 | 7.50 |
| Sprinting | 10.0 | 12.50 | 12.50 |

**Formula:** kcal/min = METs x 75 kg / 60 | Water loss = 1 ml per kcal (hydrationPerKcal: 0.001)

**Starting state:** Calories: 1100/2500 (44% — LOW) | Hydration: 1500/3000 ml (50% — LOW)
**Thresholds:** OK >70% | Low 40-70% | Critical 20-40% | Empty <=20%

---

### Phase 0: Courtyard (7 min)
**Activity:** Walk 4 min + Rest 2 min + Run 1 min

**Critical Path Only:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 1100 (44%) | — | 1500 (50%) |
| +Mushrooms x2 | +180 | 1280 (51%) | — | 1500 (50%) |
| +WaterBottleSmall | — | 1280 (51%) | +150 | 1650 (55%) |
| -Walk 4 min | -12.5 | 1267 (51%) | -12.5 | 1637 (55%) |
| -Rest 2 min | -3.3 | 1264 (51%) | -3.3 | 1634 (54%) |
| -Run 1 min | -12.5 | 1251 (50%) | -12.5 | 1622 (54%) |

**With Optional Items:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 1100 (44%) | — | 1500 (50%) |
| +Mushrooms x2 | +180 | 1280 (51%) | — | 1500 (50%) |
| +WaterBottleSmall | — | 1280 (51%) | +150 | 1650 (55%) |
| +WaterBottle (opt) | — | 1280 (51%) | +30 | 1680 (56%) |
| -Walk 4 min | -12.5 | 1267 (51%) | -12.5 | 1667 (56%) |
| -Rest 2 min | -3.3 | 1264 (51%) | -3.3 | 1664 (55%) |
| -Run 1 min | -12.5 | 1251 (50%) | -12.5 | 1652 (55%) |

**Result (critical path):** Calories: ~1251 (50%) | Hydration: ~1622 ml (54%)
**Result (with optional):** Calories: ~1251 (50%) | Hydration: ~1652 ml (55%)

---

### Phase 1: Old Man's Apartment (5 min)
**Activity:** Walk 2 min + Rest 3 min

**Critical Path Only:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 1251 (50%) | — | 1622 (54%) |
| -Give WaterBottleSmall | — | 1251 (50%) | -150 | 1472 (49%) |
| +BreadBig | +1000 | 2251 (90%) | — | 1472 (49%) |
| -Walk 2 min | -6.3 | 2245 (90%) | -6.3 | 1466 (49%) |
| -Rest 3 min | -4.9 | 2240 (90%) | -4.9 | 1461 (49%) |

**With Optional Items:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 1251 (50%) | — | 1622 (54%) |
| -Give WaterBottleSmall | — | 1251 (50%) | -150 | 1472 (49%) |
| +BreadBig | +1000 | 2251 (90%) | — | 1472 (49%) |
| +BreadSlice (opt) | +125 | 2376 (95%) | — | 1472 (49%) |
| -Walk 2 min | -6.3 | 2370 (95%) | -6.3 | 1466 (49%) |
| -Rest 3 min | -4.9 | 2365 (95%) | -4.9 | 1461 (49%) |

**Result (critical path):** Calories: ~2240 (90%) | Hydration: ~1461 ml (49%)
**Result (with optional):** Calories: ~2365 (95%) | Hydration: ~1461 ml (49%)

---

### Phase 2: Cigarette Hunt (4 min)
**Activity:** Walk 3 min + Rest 1 min

| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 2240 (90%) | — | 1461 (49%) |
| -Walk 3 min | -9.4 | 2231 (89%) | -9.4 | 1452 (48%) |
| -Rest 1 min | -1.6 | 2229 (89%) | -1.6 | 1450 (48%) |

**Optional items do not affect metabolism (quest items, non-consumable)**

**Result:** Calories: ~2229 (89%) | Hydration: ~1450 ml (48%)

---

### Phase 3: Addicts' Apartment (TBD duration)
**Activity:** TBD

**Critical Path Only:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 2229 (89%) | — | 1450 (48%) |
| +ChiliConCarne | +295 | 2524 (100%) | — | 1450 (48%) |
| +WaterBottleMedium | — | 2524 (100%) | +300 | 1750 (58%) |
| Metabolism | -? | -? | -? | -? |

**With Optional Items:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 2229 (89%) | — | 1450 (48%) |
| +ChiliConCarne | +295 | 2524 (100%) | — | 1450 (48%) |
| +FishPaste (opt) | +765 | 3289 (100%) | — | 1450 (48%) |
| +WaterBottleMedium | — | 3289 (100%) | +300 | 1750 (58%) |
| +WaterBottle (opt) | — | 3289 (100%) | +30 | 1780 (59%) |
| Metabolism | -? | -? | -? | -? |

**Result (critical path):** Calories: ~2524+ (100%) | Hydration: ~1450+ ml (48%+)
**Result (with optional):** Calories: ~3289 (100%) | Hydration: ~1780 ml (59%)

---

### Phase 4: Repairman's Apartment (TBD duration)
**Activity:** TBD

**Critical Path Only:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 2229 (89%) | — | 1450 (48%) |
| +BraisedBeans | +360 | 2589 (100%) | — | 1450 (48%) |
| +WaterBottleSmall | — | 2589 (100%) | +150 | 1600 (53%) |

**With Optional Items:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 2229 (89%) | — | 1450 (48%) |
| +BraisedBeans | +360 | 2589 (100%) | — | 1450 (48%) |
| +BreadSlice (opt) | +125 | 2714 (100%) | — | 1450 (48%) |
| +WaterBottleSmall | — | 2714 (100%) | +150 | 1600 (53%) |
| +WaterBottle (opt) | — | 2714 (100%) | +30 | 1630 (54%) |

**Result (critical path):** Calories: ~2589 (100%) | Hydration: ~1600 ml (53%)
**Result (with optional):** Calories: ~2714 (100%) | Hydration: ~1630 ml (54%)

---

### Phase 5: Trade with Old Man (TBD duration)
**Activity:** Minimal movement (dialogue)

| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 2229 (89%) | — | 1450 (48%) |
| -Rest/metabolism | -? | -? | -? | -? |

**Note:** Hydration still LOW at this point. No significant calories gained until Phase 6.

---

### Phase 6: Family Apartment (TBD duration)
**Activity:** TBD

**Critical Path Only:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 2229 (89%) | — | 1450 (48%) |
| +BreadBlackQuarters | +250 | 2479 (99%) | — | 1450 (48%) |
| +WaterBottleBig | — | 2479 (99%) | +500 | 1950 (65%) |
| Final | — | ~2479 (99%) | — | ~1950 (65%) |

**With Optional Items:**
| Step | kcal | kcal Total | Water ml | Water Total |
|------|------|-----------|----------|-------------|
| Start | — | 2229 (89%) | — | 1450 (48%) |
| +BreadBlackQuarters | +250 | 2479 (99%) | — | 1450 (48%) |
| +StewedBeef (opt) | +785 | 3264 (100%) | — | 1450 (48%) |
| +WaterBottleBig | — | 3264 (100%) | +500 | 1950 (65%) |
| +WaterBottleSmall (opt) | — | 3264 (100%) | +150 | 2100 (70%) |
| Final | — | ~3264 (100%) | — | ~2100 (70%) |

**Result (critical path):** Calories: ~2479 (99%) | Hydration: ~1950 ml (65% — LOW, approaching OK)
**Result (with optional):** Calories: ~3264 (100%) | Hydration: ~2100 ml (70% — OK)

**Hydration first hits OK (>70%) at Phase 6 with WaterBottleBig (500 ml)**

---

## Environmental Storytelling: Item Placement Principles

### Core Principles

**1. Contextual Clusters (Group with Related Objects)**
Items make sense when grouped with what they'd logically be stored with:
- Water bottle + scattered pills + bloody towel -> nightstand of someone sick
- Cigarettes + lighter + ashtray + empty vodka bottle -> worker's break corner
- Water bottle + energy bar + car keys -> glovebox ecosystem

**2. Imperfect Placement (Humans are Messy)**
Real places have objects slightly displaced, not pristine:
- Bottle slightly wedged under a seat, not centered
- Cigarette butts scattered near a vent, not in a neat line
- Can rolling under furniture, not on top

**3. Story-Leaning Objects (The "Ghost" Object)**
Place one object that implies a missing counterpart:
- Water bottle + empty holster on belt -> implies a gun was here
- Cigarettes + ashtray overflowing + someone's photo -> implies they left in a hurry
- Open tin of food + fork on plate -> implies interrupted meal

**4. Layered Discovery (3 Distance Tiers)**
| Distance | Visibility | Example |
|----------|-----------|---------|
| Far | Obvious | Water bottle in cupholder |
| Medium | Requires look | Bottle under car seat |
| Close | Requires search | Small bottle in glovebox |

---

## Phase-by-Phase Environmental Storytelling

### Phase 0: Courtyard

#### WaterBottleSmall (150 mL) — under driver seat of crashed sedan

**Witness objects to add:**
- Frayed seatbelt still buckled
- Dust pattern on dashboard showing where hands gripped wheel
- Small religious icon hanging from rearview (Orthodox cross)
- Half-melted candle on back seat floor

**Mini-story:** The driver was a religious person who kept their water close. They unbuckled and left in such a hurry the seatbelt is still extended. The candle suggests they knew something was wrong before the evacuation.

#### WaterBottle (30 mL dregs) — in cup holder of crossover

**Witness objects to add:**
- Two other bottles in cup holder (both empty, labels peeled off)
- Parking receipt from 3 days before evacuation crumpled on floor
- Single flip-flop visible under passenger seat

**Mini-story:** This was not the driver's bottle — it belonged to a passenger. Three bottles suggests multiple people used this car. The peeling labels mean they drank whatever they could find. One flip-flop implies they left with only one shoe.

#### Mushrooms x2 — near fallen tree

**Witness objects to add:**
- Rotting logs nearby with more mushrooms (non-interactive, shows authenticity)
- Torn plastic bag caught in branches overhead
- Old Soviet newspaper page (weathered) nearby

**Mini-story:** Someone was foraging here before everything fell apart. The newspaper is from a week before the event — suggesting this area was still being used for normal life until recently.

#### Metal lever/pipe — by tank tread

**Witness objects to add:**
- Track marks in mud leading both directions
- Small oil stain on concrete beneath where tank was parked
- Dog collar (empty) hanging from a nearby fence post

**Mini-story:** Military vehicle pushed through here. The lever was either dropped by a mechanic working on the tank, or deliberately left as a marker. The dog collar implies residents had pets they couldn't take.

---

### Phase 1: Old Man's Apartment

#### BreadBig (1000 kcal) — on kitchen table, cutting board wrapped in cloth

**Witness objects to add:**
- Two plates set (one in front of empty chair, one where player sits)
- Glass of water (empty, ring stain on table)
- Reading glasses folded next to a book (face down, spine cracked)
- Photograph on wall: family photo, faded colors

**Mini-story:** Grandpa set two places expecting company, or perhaps he always ate alone at the table and the second plate was for his late wife. The empty glass of water shows his last act was hospitality even when he had almost nothing. The cloth wrapping was to keep the bread from going stale — he rationed carefully.

#### WaterBottle (30 mL) — on nightstand by bed

**Witness objects to add:**
- Pill bottle (empty, label from local pharmacy)
- Small radio on nightstand, antenna slightly bent
- Handwritten note under the lamp: "Stay hydrated. Even when you don't feel like it."
- Cigarette butts in a saucer (not an ashtray)

**Mini-story:** This was Grandpa's personal bottle — he kept it by his bed. The note suggests he was managing his health. The cigarette butts in a saucer instead of an ashtray means he was discreet about smoking (perhaps hiding it from someone who told him to quit).

---

### Phase 2: Stairwell

#### Cigarette x1 — inside electrical panel on landing

**Witness objects to add:**
- Fresh scratches around panel screws (opened recently)
- Single matchbook page tucked behind panel hinge
- Dust disturbed on landing floor showing foot traffic

**Mini-story:** A maintenance worker used this panel as their secret smoke spot. The scratches show they opened it multiple times. The matchbook page was left accidentally — they probably used a lighter (which is why only one match).

#### Cigarette x1 — in dead flower pot on windowsill

**Witness objects to add:**
- Dry soil with visible fingerprints in the dust
- Several other pots: all dead, one with a small dead succulent
- Condensation ring on windowsill (from someone's breath, months ago)
- Cracked window frame letting in cold air

**Mini-story:** Someone smoked here while looking out the window. They were watching for something — or someone. The dead plants show how long it's been: no one has watered them since. The condensation ring is a ghost — their breath is still "visible" in the cold air.

---

### Phase 3: Addicts' Apartment

#### ChiliConCarne (295 kcal) — on kitchen counter + lighter nearby

**Witness objects to add:**
- Can opener still attached to the drawer
- Uneaten portion in pot (solidified, cold)
- Used syringe on counter (non-interactive, behind dishes)
- TV still on (static), volume loud enough to be uncomfortable

**Mini-story:** The occupant was high when they made this. They got distracted by the TV (still on static after all this time) and never ate the food. The syringe is their story. The lighter was their tool — they used it to prepare what was in the syringe.

#### WaterBottleMedium (300 mL) — on bathtub edge in bathroom

**Witness objects to add:**
- Bathrobe on floor (urine-stained, never picked up)
- Medicine cabinet open (empty except for cotton balls)
- Mold growing on soap bar (unused)
- Wet floor mat (stained, undisturbed for weeks)

**Mini-story:** Someone drank the water after drawing a bath. The bathrobe wasn't picked up because the person never came back from wherever the syringe took them. The mold on soap shows hygiene completely stopped.

#### Crowbar — stuck in corpse in corridor

**Witness objects to add:**
- Dark stain radiating from body onto floorboards
- One shoe missing near the body
- Handwritten note on floor near outstretched fingers: "Don't trust the water."
- Buzzing flies (audio cue if possible)

**Mini-story:** The crowbar was used in self-defense — or attack. The note suggests paranoia about contamination. One shoe missing means they were trying to run. "Don't trust the water" is either true survival advice or the ramblings of an addict.

---

### Phase 4: Repairman's Apartment

#### Cigarette packet (20 pcs) — in work jacket pocket by front door

**Witness objects to add:**
- Work boots (size 43) by door, still laced
- Radio clipped to jacket belt
- Keys on a Soviet-era hardware store keychain
- Toolbox in hallway (open, organized)

**Mini-story:** The repairman left for work and never came home. His boots are ready, his radio is clipped, his keys are here — but his jacket is the only thing left inside. He either left quickly or something called him back outside.

#### BraisedBeans (360 kcal) — in kitchen junk drawer next to batteries

**Witness objects to add:**
- Multiple batteries (some dead, some new, sorted by type)
- Assorted screws and nails in compartmentalized organizer
- Hand-drawn map of building's plumbing (on drawer bottom)
- Burner phone (dead, cracked screen)

**Mini-story:** The repairman was preparing for something — the batteries suggest he needed power, the map shows he was studying the building's systems, the burner phone implies he was communicating with someone about something he didn't want recorded.

#### WaterBottleSmall (150 mL) — on workbench in workshop

**Witness objects to add:**
- Coffee rings on workbench surface (multiple, never cleaned)
- Half-finished DIY humidifier (parts scattered)
- Blueprint roll (unrolled partially, showing radiator modifications)
- Photo tucked under a tool: man + woman, wedding photo, woman's face scratched out

**Mini-story:** The repairman worked long hours here. His wedding photo with the face scratched out tells a story — either divorce, death, or anger. The humidifier project was never finished. The water bottle was his daily companion while working.

---

### Phase 6: Family Apartment

#### WaterBottleBig (500 mL) — in kitchen refrigerator

**Witness objects to add:**
- Other items in fridge: all expired, unopened packages of mayonnaise, suspicious jar of something
- Shopping list on fridge: "Milk, bread, [child's name]'s favorite juice"
- Children's drawings held by magnet on door
- Thermostat set to 10°C (max cold) — someone was trying to preserve food

**Mini-story:** A family with children lived here. The shopping list shows they were in the middle of normal life. The drawings are still up — the children may have left earlier, or... The max-cold thermostat was someone's attempt to keep food from spoiling when the power started failing.

#### Backpack — on coat rack by front door

**Witness objects to add:**
- Adult coat still hanging (child's coat missing)
- House keys left in lock (inside)
- Small suitcase (open, empty) near door
- Stroller collapsed in corner

**Mini-story:** The family evacuated in stages — adult first, then came back for the child. Or: one parent took the child, the other stayed behind and never returned. Keys left in lock means whoever left last expected to come back.

#### Flashlight — in hallway console table drawer

**Witness objects to add:**
- Other drawer contents: batteries (new, still in packaging), candles (unused), first aid manual
- Coat hook with multiple car keys (4 keys on a ring)
- Mirror above console (cracked, shape of fist visible)
- Shoe rack: one pair of adult shoes, one pair of small shoes

**Mini-story:** Someone was ready for emergencies — the flashlight, batteries, candles, first aid manual. Multiple car keys means multiple vehicles (or spare keys). The cracked mirror from a fist suggests an argument happened here.

---

## Mini-Story Checklist for Each Placement

For each item placed, verify:
- [ ] **Who** left this here? (driver, worker, resident, victim)
- [ ] **When** did they leave it? (during evacuation, months before, recently)
- [ ] **Why** is it here? (forgot, cached, dropped fleeing)
- [ ] **What** is missing that they'd normally have with them?

This transforms a generic "water bottle" into a narrative artifact.

---

## Visual Composition Tips

### Do:
- Angle bottles at slight tilt (3-7 degrees) — perfectly upright looks artificial
- Partially obscure items behind debris or furniture edges
- Group items of similar origin (all car items together, all personal items separate)
- Add "witness objects" nearby (dust, cobwebs, rust stains)

### Don't:
- Place items in geometric patterns unless clearly intentional (military stash)
- Put items in empty spaces without environmental justification
- Stack identical items unless it's a known cache
