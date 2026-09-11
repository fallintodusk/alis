ProjectUI Fonts
===============

TEXT FONTS (Inter)
------------------
Download from: https://github.com/rsms/inter/releases

Required files:
- Inter-Regular.ttf   (Body, Label)
- Inter-Medium.ttf    (Button)
- Inter-SemiBold.ttf  (Headings)


UI ICON FONT (Material Design Icons, v7.4.47)
----------------------------------------------
Font name in code: "Icon"
Download from: https://cdn.jsdelivr.net/npm/@mdi/font@7.4.47/fonts/materialdesignicons-webfont.ttf
License: Apache 2.0
Browse icons: https://pictogrammers.com/library/mdi/

Required file:
- materialdesignicons-webfont.ttf

Codepoint range: U+F0001-U+F1D17 (Supplementary PUA-A)
In JSON use surrogate pairs: U+F040A -> "\uDB81\uDC0A"
In C++ use \U escape: TEXT("\U000F040A")

Current UI icons:
- Play:       U+F040A  mdi-play        JSON: \uDB81\uDC0A
- Cog:        U+F0493  mdi-cog         JSON: \uDB81\uDC93
- Info:       U+F02FC  mdi-information JSON: \uDB80\uDEFC
- Power:      U+F0425  mdi-power       JSON: \uDB81\uDC25
- Flash:      U+F0241  mdi-flash       JSON: \uDB80\uDE41
- Trash:      U+F0A79  mdi-trash-can   JSON: \uDB82\uDE79
- Cut:        U+F0190  mdi-content-cut JSON: \uDB80\uDD90
- Map marker: U+F034E  mdi-map-marker  JSON: \uDB80\uDF4E


GAME ICON FONT (Game Icons, game-icons.net)
-------------------------------------------
Font name in code: "GameIcon"
Download from: https://github.com/seiyria/gameicons-font
License: CC BY 3.0 (credit game-icons.net)
Browse icons: https://game-icons.net/

Required file:
- game-icons.ttf

Font structure (inspected 2026-02-10):
- 4,099 named glyphs, 4,102 mapped codepoints
- cmap: Format 4 (BMP) + Format 12 (Full Unicode), both present
- Actual range: U+F000-U+FFFE (BMP PUA only, NO supplementary plane glyphs)

Codepoint convention:
- CSS file uses 5-hex-digit escapes: \ffXXX
- Actual BMP codepoint: drop leading 'f' -> \uFXXX
- In C++ use: TEXT("\uFXXX")
- Example: CSS \ff155 -> C++ TEXT("\uF155") (barbute)

Current equipment slot icons:
- Backpack:    \uF12A  (back)       https://game-icons.net/1x1/delapouite/backpack.html
- Barbute:     \uF155  (head)       https://game-icons.net/1x1/lorc/barbute.html
- Breastplate: \uF234  (chest)      https://game-icons.net/1x1/lorc/breastplate.html
- Armor pants: \uF0F4  (legs)       https://game-icons.net/1x1/irongamer/armored-pants.html
- Boots:       \uF200  (feet)       https://game-icons.net/1x1/lorc/boots.html
- Broadsword:  \uF23C  (main hand)  https://game-icons.net/1x1/lorc/broadsword.html
- Shield:      \uFC57  (off hand)   https://game-icons.net/1x1/sbed/shield.html


KNOWN ISSUES
------------
- Knapsack (\uF831, CSS: \ff831): cmap names GID 2096 "knapsack" but the glyph
  outline renders as feet/footprint. Font build error - wrong SVG at that GID.
  Workaround: use "backpack" (\uF12A) instead.
  Investigation (2026-02-10): Python cmap parser confirmed U+F831 -> GID 2096
  named "knapsack" in both Format 4 and Format 12 subtables. The cmap entry is
  correct but the glyph outlines at GID 2096 are wrong (font generation bug).
- Do NOT use \U (8-hex) supplementary escapes for this font - it has zero glyphs
  above U+FFFF. The CSS \ffXXX notation is misleading (it looks like U+FFXXX
  supplementary but the font only maps BMP codepoints).


BEST PRACTICES (for agents and developers)
-------------------------------------------
1. ALWAYS verify new icons visually in-game before committing.
   The font has glyph name mismatches (see knapsack bug above).
2. Use the CSS file to look up codepoints: game-icons.css
   Pattern: .game-icon-<name>:before { content: "\ffXXX"; }
   Drop leading f -> \uFXXX for C++/JSON.
3. Prefer icons with simpler names (fewer chances of build errors).
   Known-good alternatives for "back" slot: backpack, light-backpack, school-bag.
4. If an icon doesn't render or shows wrong glyph:
   a. Check codepoint in CSS (grep icon name in game-icons.css)
   b. Verify font is loaded (search "game-icons.ttf" in Alis.log)
   c. Try a nearby codepoint or alternative icon name
   d. Rebuild + restart editor (static font caches persist across hot reload)
5. For C++ static const maps with font codepoints:
   The `static const TMap` is initialized once. Hot reload does NOT reinitialize
   it. Always restart the editor after changing icon codepoints.


HOW TO FIND NEW ICONS
---------------------

Game Icons (4100+ RPG icons):
1. Browse: https://game-icons.net/tags.html (by category)
2. Search: https://game-icons.net/ (search bar)
3. Get codepoint: find icon name in game-icons.css
   CSS "\ffXXX" -> actual codepoint is \uFXXX (drop leading f)
4. Verify: test the icon in-game before committing (some glyphs have build errors)

MDI (7400+ UI icons):
1. Browse: https://pictogrammers.com/library/mdi/
2. Click icon -> copy codepoint (shown as FXXXX)
3. For JSON: convert to surrogate pair (see formula below)

Surrogate pair formula (for codepoints above U+FFFF):
  U' = codepoint - 0x10000
  High = 0xD800 + (U' >> 10)
  Low  = 0xDC00 + (U' & 0x3FF)
  JSON: "\uHigh\uLow"


If fonts missing, system falls back to engine Roboto.
