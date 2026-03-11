# ProjectUI Theme System

Centralized styling for colors, fonts, spacing, and animations with runtime switching.

## Scope and Paths
- Plugin: `Plugins/UI/ProjectUI`
- Theme data asset: `Source/ProjectUI/Public/Data/ProjectUIThemeData.h`
- Theme manager: `Source/ProjectUI/Public/Subsystems/ProjectUIThemeManager.h`
- Base widgets that react to theme changes: `Source/ProjectUI/Public/Widgets/ProjectUserWidget.h`, `.../ProjectButton.h`, `.../ProjectTextBlock.h`

## What a Theme Defines
- Colors: Primary, Secondary, Success, Warning, Error, Background, Surface, TextPrimary/Secondary, Border.
- Typography: HeadingLarge/Medium/Small, BodyLarge/Medium/Small, Button, Label.
- Spacing: Padding XS/S/M/L/XL, BorderRadius, BorderThickness.
- Animation timing: Fast/Normal/Slow plus default easing.
- Optional textures and button state assets.

## Using Themes
```cpp
// Get the manager
auto* ThemeMgr = GetGameInstance()->GetSubsystem<UProjectUIThemeManager>();

// Load from settings or set explicitly
ThemeMgr->LoadThemeFromSettings();
ThemeMgr->SetActiveTheme(MyThemeData);

// React to changes inside a widget
void UMyWidget::OnThemeChanged(UProjectUIThemeData* Theme)
{
    TitleText->SetFont(Theme->HeadingLargeFont);
    TitleText->SetColorAndOpacity(Theme->TextColorPrimary);
}
```

## Base Widget Behavior
- `UProjectUserWidget` registers for theme change notifications in `NativeConstruct` and unregisters in `NativeDestruct`.
- Variant widgets (`UProjectButton`, `UProjectTextBlock`, etc.) pull colors/fonts from the active theme and reapply them on change.
- Buttons support variants (Primary/Secondary/Success/Warning/Error/Text) and sizes (Small/Medium/Large) mapped to theme values.

## Creating a Custom Theme (Designer Flow)
1) In Content Browser: Add -> Blueprint Class -> search `ProjectUIThemeData`; name `DA_MyTheme`.
2) Set colors, fonts, spacing, animation timing.
3) In startup code, set it active via `UProjectUIThemeManager`.
4) Verify widgets respond (buttons recolor, text uses new fonts).

## Best Practices
- Use semantic colors instead of hardcoded values in widgets and JSON layouts.
- Keep fonts consistent with the defined typography ladder; avoid ad-hoc sizes.
- Listen for theme changes rather than caching theme values.
- Pair layouts with themes: JSON should reference theme keys (e.g. `PrimaryColor`, `HeadingMediumFont`).

## References
- Layout system that resolves theme keys from JSON: `docs/ui_layout.md`
- MVVM binding guidance: `docs/ui_mvvm.md`
- Global router page: `../../../docs/ui.md`
