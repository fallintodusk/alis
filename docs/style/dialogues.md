# Dialogue Writing Style

How NPC and player lines should read in ALIS. Companion to [../gameplay/dialogue_guide.md](../gameplay/dialogue_guide.md) (authoring/technical). This doc owns **voice and prose**; that one owns **structure and assets**.

ALIS is post-apocalyptic survival: chaos, starvation, isolation, fear. Lines must read like a real person under that load, not a writer's idea of one.

## Core Principles

1. **Real, not theatrical.** No metaphors a real person wouldn't reach for. No "knife is sharp" bravado. No "I don't leave my fortress." If it sounds like a screenwriter pitching, cut it.
2. **Short over crafted.** Real desperate speech is short and broken. Parallel structure ("lucky you forgetting / me, I remember it all") is a writer's tic. Survivors don't talk in callbacks.
3. **Concrete over abstract.** "I'm so cold." "I haven't drunk in two days." "Every time a bomb fell, the building shook." NOT "the world has fallen" or "darkness consumes us."
4. **Listen, don't pivot.** Player asks WHAT, NPC answers WHAT. Player asks WHY, NPC answers WHY (or honestly says they don't know). Mismatched questions read as two people not listening to each other.
5. **Repetition is real.** Real desperate speech repeats *one observation* ("Why is this happening? Why is this happening?"), not poetic refrains. Repetition signals breakdown, not style.
6. **Other-focus signals character.** People in extremis often ask about *others* before themselves. Reserve this for moments the character has earned warmth - it lands hard.

## Character Behavior in Critical States

Each row: how a real person speaks vs. the writer-version trap to avoid.

| State | Real speech | Writer-version trap |
|---|---|---|
| **Severe thirst** | "Water - please. Just water." Repetition of one word. "I haven't drunk in days." | "Water first, then we talk. Deal?" - survivors don't negotiate, they beg. |
| **Severe hunger** | "Bread. Just a piece." "I forgot what it tastes like." | "I hunger for sustenance" - never. |
| **Isolation (weeks alone)** | "Are you alone out there?" "How many days has it been? I lost count." | "I have not seen another soul in many a moon" - too archaic, too crafted. |
| **Fear at door / locked-in** | "Wait. Wait. Don't break it." Short, repetitive, reflexive panic. | "Speak fast - or leave" - that's a 1980s movie villain, not a frightened old man. |
| **Disorientation / amnesia** | "I... I... I can't... Why is this?..." Fragmented, trailing, unable to finish. | "Verily I cannot recollect the path that led me to this door" - over-articulate. |
| **Dying / exhaustion** | "I'm so tired." "Hurry. Please." Honest weakness. Asking for sleep, not rescue. | "The shadow grows long upon me" - poetic distance from a real moment. |
| **Sudden warmth (after relief)** | Small concrete observation: "Been a long time since bread smelled like bread." | "Joy floods my soul" - abstract emotion-naming. |
| **Confusion at scale of disaster** | "Why is this happening?" Three words. The dying mind reaches for the smallest question. | "What manner of catastrophe has befallen us all" - exposition disguised as speech. |

## Real Testimony Reference

These are documented accounts and short quoted fragments from real people in conditions that map onto ALIS scenarios. Some entries are direct quotes; some are journalists' descriptions of the speaker's state. Use them as the *texture* you're matching, not as verbatim citations -- if you reuse a line, check the primary source.

**Vanda Obiedkova** (91, dying of thirst in a Mariupol basement, 2022):
> "Why is this happening?"

That's it. Three words, repeated. The dying mind doesn't compose monologues.

**Larissa** (Vanda's daughter, on her mother's speech):
> "My mother kept saying she didn't remember anything like this during the Great Patriotic War."
> "We were living like animals."

Plain vocabulary. Short sentences. One observation, repeated.

**Mustafa Avci** (rescued after 261 hours under Turkey earthquake rubble, 2023, his first words asked about his mother by name):
> "Did everyone escape OK, Nazli? ... Let me hear their voices, if for a moment."

After 10+ days dying, his first thought was others. Not himself. Naming his mother makes the other-focused pattern even sharper -- in extremis, the mind reaches for *one specific person*, not abstractions.

**Trapped woman** (5 days under Turkey rubble, on her state before rescue):
> "She had given up hope of being found - and prayed to be put to sleep because she was so cold."

Beyond pleading. Praying for sleep, not rescue. Concrete: cold.

**Teenager** (94 hours under rubble, on rescue):
> "Thank God you arrived."

Three words. Gratitude. No drama.

**Mariupol photographer** (63, on siege life):
> "I was thinking - what would we run out of first? Food? Water? Or will a bomb land on us?"
> "The biggest delicacy was to pour 1.5 litres of boiling water into a thermos and drink it."

Pragmatic, not philosophical. Specific quantities. Concrete delicacies.

## Anti-Patterns Caught in the Wild

Examples removed from `DLG_GrandPa_Entry.json` during the kindness-arc rewrite, with the diagnosis:

| Cut | Why |
|---|---|
| "I'm old, but my knife is sharp. Speak fast - or leave." | Bravado theatrics. A real frightened old man begs, he doesn't threaten. |
| "Dangerous times, son. I don't leave my fortress." | "Dangerous times" is generic exposition. "Fortress" is self-aware grandpa humor - fine *later*, wrong tone here. |
| "Water first - then talk. Deal?" | Merchant-haggler voice. A man who hasn't drunk in days doesn't negotiate. He pleads. |
| "If I die before you get back, the door stays locked forever." | Threat / leverage. Real dying people fear *for themselves*; they don't weaponize their death against the rescuer. |
| "Lucky you, son - forgetting. Me, I remember it all." | Writer's parallel structure. Real desperate speech doesn't compose poetic counterpoint to the listener's state. |
| "Sorry, I can't help you." | Polite cover for "I'm leaving you to die." If the player chooses to walk away from a begging man, the option text should *name* that choice, not hide it. |

## ALIS-Specific Tone Notes

### "Old voice" - what it means

Weathered, slow, plain, and older in rhythm. Shorter sentences, occasional fragments, practical word choices. **Not** theatrical archaic ("verily," "many a moon," "speak fast or leave"). An old man in 2026 Kazan does not sound like a fantasy NPC. He sounds like an old man.

### Threshold trust: stranger vs. ally

In survival contexts, **crossing a threshold** (locked door opens; someone is invited inside; resources are shared) shifts the relationship from *stranger* to *ally* in a single beat. Lines written for one phase do not fit the other.

| Phase | What is true | Voice |
|---|---|---|
| Outside / before trust | They could still be a threat. Wary, distant, transactional. | "What do you want?" / "State your business." / no names exchanged. |
| Inside / after the trust transfer | They have been let in. They have shared survival. They are not friends yet, but the door has opened. | First warmth allowed. Small acknowledgments. Concrete observations about the other person. Beginnings of personal questions ("What is your name?"). |

**Rule:** once someone is inside your apartment in a post-apocalyptic world, you have trusted them with your life. Lines for that phase should reflect that trust without overdoing intimacy. The relationship is *forming* -- not formed, not still hostile.

Mistakes to avoid in the post-threshold phase:
- **Continuing stranger-distance** ("Mhm. Go on, then. Help yourself.") -- too cold; the trust has already happened, the player will read it as the NPC retreating.
- **Jumping to soul-baring intimacy** ("I miss... talking. I have been alone too long.") -- too fast; they still do not know each other, this lands as desperate-clingy not warm.
- **Right zone**: small, real recognitions, or quiet self-disclosures the speaker would not tell a stranger. *"Funny - I do not even know your name." / "Seems I had locked myself in to die." / "Glad you came back."* These either name what just happened or trust the listener with one fact about the speaker. They name; they do not declare. Prefer realization-shape ("seems...", "turns out...", "I had not noticed...") over confession-shape ("truth is...", "I have to tell you...") -- realization lets the line land *in the moment of speaking*, confession announces a Big Reveal.

### "Son" budget for paternal address

Reserve "son" (or any paternal address) for moments where the address itself does work. Default: leave it off. Reach for it only when:
- It is the *first* warm address from the NPC (lands as a tone shift)
- The line is otherwise too cold and "son" carries the only warmth
- A real older person *would* reach for it (e.g. a plea or a worried farewell)

Anti-pattern: trailing every line with "son" as a closer. It becomes a tic that drains the address of meaning. Across an arc of 5-10 lines, "son" should appear maybe 2-3 times total.

### Kindness arc

NPC tone scales with composure. A character at maximum fear/thirst/exhaustion has zero capacity for warmth or wit. As their immediate need is met (water, food, trust), small warmth and dry humor return.

Pattern for trade-NPCs: **wary -> transactional -> warmer -> quietly affectionate.** Don't skip stages. Don't front-load warmth that hasn't been earned.

### Gallows humor

Russian/post-Soviet dark humor is welcome but **only when the speaker has composure**. A man whose hands are shaking too hard to drink water can't be wry - he can only beg. Save dry observations for nodes after the immediate crisis is past.

### Bracket convention -- two uses

UI uses plain `UTextBlock` (not `URichTextBlock`), so markdown `*italic*` won't render. Use `[Brackets.]` for any text the player should read as **action or stage direction, not speech**.

**Use 1: player non-verbal choices** (player options that describe an action instead of a spoken line):

| Light weight | `[Walk away.]` |
| Medium weight | `[Walk away. Leave him.]` |
| Heavy weight (player should feel the choice) | `[Walking away from a dying man.]` |

Use heavier framing when the *cover* of polite verbal refusal would let the player escape the moral weight ("Sorry, I can't help you" -> walk away politely). The bracket forces honesty.

**Use 2: NPC emotional state inline** (a stage direction inside or after an NPC line, when the line earns visible emotion):

> "Fucking TV not worked. [Crying.]"

Reserve for moments where the speaker has *earned* visible emotion through the scene -- not for early panic or transactional beats. The same dialogues.md rule applies: real grief is specific (a small object, a single failed thing), not abstract ("the world has fallen"). The [stage direction] tells the player which beat the line *broke* on.

### Player voice

Player is disoriented (recently woke up in a ruined world, missing memory). Voice is:
- **Less articulate** than NPCs who have lived through it
- Fragmented under stress: `"I... I... I can't... Why is this?..."`
- Honest, blunt, short. Never narrator-voice ("I'll find water and come back" is narration; "Hold on. I'll be back." is speech)

## Cross-References

- Authoring/technical: [../gameplay/dialogue_guide.md](../gameplay/dialogue_guide.md)
- Narrative principles: [../architecture/principles.md](../architecture/principles.md)
- Worked example: `Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_Entry.json` and the grandpa dialogue chain
- Active rewrite todo: `todo/00_current/fix_grandpa_dialogue.md`

## Sources

Real testimony pulled to ground this style:

- [Holocaust Survivor, 91, Dies 'Pleading for Water' in Mariupol Basement (NBC New York)](https://www.nbcnewyork.com/news/national-international/holocaust-survivor-91-dies-pleading-for-water-in-mariupol-basement/3656673/)
- [91-year-old Holocaust Survivor Perishes in Mariupol Basement (Chabad)](https://www.chabad.org/news/article_cdo/aid/5495941/jewish/91-year-old-Holocaust-Survivor-Perishes-in-Mariupol-Basement.htm)
- ['We waited for death': Mariupol siege survivor recounts ordeal (Al Jazeera)](https://www.aljazeera.com/news/2022/3/18/russia-ukraine-war-mariupol-siege-survivor)
- [Turkey quake survivor's first concern after 10 days trapped: 'How is mother?' (CNN)](https://www.cnn.com/2023/02/17/middleeast/turkey-earthquake-survivor-emotional-phone-call-intl-hnk/index.html)
- ['I thought I was going to die': Turkey quake survivors' ordeal (Al Jazeera)](https://www.aljazeera.com/news/2023/2/15/i-thought-i-was-going-to-die-turks-share-their-survival-story)
- [How 'extraordinary' survivors are still being pulled from rubble 10 days after massive earthquake (CNN)](https://www.cnn.com/2023/02/18/health/turkey-quake-rescues/index.html)
- [Leningrad Siege Survivor Left To Die In Her Apartment (RFE/RL)](https://www.rferl.org/a/leningrad-siege-survivor-left-to-die-in-her-flat/27096988.html)

When in doubt, **read one of these accounts** before writing the line. The texture will land in your fingers in five minutes; faster than any rule sheet.
