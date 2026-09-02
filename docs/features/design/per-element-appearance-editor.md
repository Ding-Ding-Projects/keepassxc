# Per-element appearance editor

Feature id: `per-element-appearance-editor` · Category: Design and appearance

## Behaviour

Every element with an object name can be restyled in place. **Shift+right-click** any element, or press **Ctrl+Shift+E** with the element focused, and the appearance editor (`Material::AppearanceEditor`, `src/gui/material/MaterialAppearanceEditor.h`) opens as a non-modal 440 px panel beside it, tracks the window as it moves or resizes, and returns focus to where it came from when closed with its close button or <kbd>Escape</kbd>. The panel names the element (its object name and class) and carries its own property search with the Regex chip and anchored builder (`appearance.editor`); an unparsable pattern changes nothing.

The editor has four tabs:

- **Type**: font family (a Material select of every installed face, each rendered in its own typeface, with its own search), size as a slider and a free-entry box, weight (Light, Regular, Medium, Bold), italic, underline, strikethrough and overline switches, capitalization (as written, ALL CAPS, all lowercase, Small Caps, Capitalize Each Word), character spacing and line height.
- **Colour**: background and text colour through two infinite colour pickers (`Material::ColorPicker`): a saturation/brightness field, a hue bar and an opacity bar, a translator that reads and writes the colour as HEX, RGB, HSL, HSV, HWB, CMYK, LAB, LCH, OKLAB and OKLCH (typing into any notation moves all the others; bad text is kept in front of the user and marked invalid), the CSS name when there is one, a WCAG contrast readout against the other colour (AAA / AA / large-text / fails), recent colours, and the **animated rainbow** as one of the choices with a speed level from 1 to 5.
- **Shape**: corner radius, element height, inner spacing, border width and colour, elevation (0 to 5, a drop shadow) and opacity.
- **Presets**: copy the current style and paste it onto another element; save the style under a name; apply or delete a saved preset from a searchable select; export every preset to the clipboard as JSON and import from it. Presets persist in `GUI/AppearancePresets`.

Every change writes `Material::ElementOverrides` immediately and the live element follows through `Material::AppearanceApplier`, which watches every widget as it appears so a customised element looks customised wherever it is shown. **Reset element** returns one element to the shipped look exactly (stylesheet, font, minimum height and effects restored); **Reset all** clears every override.

The rainbow is stored as a flag and a level, never as a colour string, so nothing downstream can build a tint from it. All rainbow surfaces turn together on one shared timer whose period is the level's cycle length (24 s at level 1 down to 3 s at level 5); under reduced motion the hue settles on one value instead of cycling.

## Configuration

`GUI/ElementOverrides` holds the per-element overrides as JSON keyed by object name; `GUI/AppearancePresets` holds the named presets. Every field is bounded on the way in (an edited file cannot smuggle a 900 px border or a 40 % line height).

## Failure modes

An element with no object name cannot be targeted; the hook walks up to the nearest named ancestor. Line height has no widget stylesheet equivalent in Qt, so the extra leading is applied as vertical padding around the text. Elevation and opacity are both graphics effects, and Qt allows one per widget: with elevation set, opacity is folded into the background's alpha instead.

## Security considerations

No network access. Presets are exported to and imported from the clipboard only; import rejects anything that is not a JSON object, an unsupported schema version, or more than 200 presets.

## Verification

`testmaterialappearanceeditor` covers the override model round trip and bounds, the editor writing every category of override with the live element following, reset restoring the shipped look, presets (save, apply, delete, copy, paste, export, import including bad input), the property search with regex opt-in, and the Shift+right-click hook. `testmaterialcolorpicker` covers the translator against published CIELAB/OKLab figures, parsing every notation back, the WCAG contrast figures, the picker's drivers and keyboard operation, and the rainbow sentinel.

## Suggested articles

- [Material Design 3 appearance](material-3-appearance.md)
- [Dropdowns are list boxes with a search bar](../search/dropdown-select.md)
- [A search bar on every surface](../search/search-bar-every-surface.md)
