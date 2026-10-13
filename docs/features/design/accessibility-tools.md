# Accessibility tools

The implementation candidate adds a shared accessibility panel and a visible entry
to Qt windows, dialogs and Material modal overlays. A reserved footer keeps the
entry away from content pointer actions. Native operating-system dialogs are
outside the Qt widget tree and use operating-system accessibility controls.
On Windows, Windows+Ctrl+Enter starts or stops Narrator.

Entries are recreated when a dynamic menu clears its actions, with connections
owned by the action's lifetime. Entry-expiry and icon-application menu handlers
accept only their expected typed action data, so opening accessibility tools does
not change those editor settings. The shared voice catalogue supplies English,
Cantonese and bilingual labels, including existing open controls. Footer space
follows the current controls' minimum size hints.

## Controls and local persistence

| Control | Consumer |
| --- | --- |
| Text size | Existing application font scale, 85 to 140 percent |
| High contrast | Theme surface, text, outline and primary color roles |
| Reduced motion | Existing motion policy |
| Stronger keyboard focus | Non-interactive focus frame following Qt keyboard focus |
| Larger interaction targets | 64-pixel content rows and 48-pixel standard inputs and text buttons |
| Keyboard help | Static instructions for Tab, Shift+Tab, Space, arrows and Escape |
| Reset | Restores the accessibility defaults and visible entry |
| Read aloud | Unavailable, because this build includes no speech engine |

Changes use local profile configuration. The target-size override preserves the
existing density preference. Existing fixed-size caption controls and specialized
custom controls are not resized by this override. Their individual target-size
coverage remains an acceptance gap. High contrast preserves the status-color
families; exhaustive contrast measurements remain pending.

## Hide and restore

Closing the panel does not hide its entry. The separated **Hide accessibility
button…** action opens a confirmation with **Cancel** selected by default. Only
confirming **Hide button** changes entry visibility. It preserves text, contrast,
motion, focus and size preferences.

**Settings → Accessibility → Show accessibility button** restores the entry
without restarting or unlocking a database. The footer Settings control remains
visible while the accessibility entry is hidden. The main Settings page indexes
the restoration row, and a window action exposes it to the command palette.
The restoration dialog itself includes the same tools entry. Confirmation dialogs
retain Cancel access back to the panel rather than opening another nested panel.

## Security and verification

The controller does not inspect a database, focused text, protected fields or the
clipboard. No speech synthesis is implemented or invoked. A visible explanation
lists the unavailable playback and voice controls. This is an explicit capability
gap, not a claim of native read-aloud support.

`TestMaterialAccessibilityTools` covers required controls, reserved content space,
modal-overlay access, deliberate hide and cancellation, local visibility storage,
preference retention, independent restoration, reset, close behavior and real
contrast/row-size consumers, rebuilt-menu lifetime, unchanged editor settings and
language changes. The first candidate, `1f2c2982038fe50d2e557e5b5321edf25197b40b`,
passed `build.bat /s -WithGuiTests` at `2026-10-13T06:06:11.1286454Z`, with executable
SHA-256 `9762867b9f8f000fbb6a838622c2c0d76472d599a78feb aed89a138e503ba416`.
That candidate was not accepted or launched for interaction evidence because
source review found the menu-lifetime and action-routing defects. The subsequent
repair and localization changes require a new exact-source build and test run.
Negative mutation checks, screen-reader proof and the full language,
theme, minimum-size and display-scale matrix are also pending. No historical
capture is acceptance evidence for these new controls.

The separate intermittent physical window-dragging report remains unresolved.
