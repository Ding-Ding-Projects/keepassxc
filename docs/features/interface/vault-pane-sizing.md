# Vault pane sizing

The vault uses a resizable three-pane splitter. On first display without a valid
remembered state, ExtraLarge starts with a 250-pixel group pane and a 392-pixel
detail pane. Large starts with 216 and 360 pixels respectively. The entry list
takes the remaining width, and minimum pane widths still apply in smaller layouts.

Construction and breakpoint changes can occur while the vault or its containing
stacked page is hidden. Splitter restoration stays pending until the panes are
visible and have geometry. A repeated request for the same breakpoint can finish
a pending restoration, but does not reset widths after restoration has completed.
Ordinary resize and hide/show cycles also preserve the current division.

When a valid remembered `GUI_MaterialVaultSplitterState` exists, it takes priority
over the reference widths. Restoring that state does not write a replacement to
settings. User splitter movement continues to save the chosen state through the
existing handler. Missing or invalid state falls back to the reference widths.

## Verification

`TestMaterialShellResponsive` establishes empty configuration before constructing
the first-run fixture. It covers default and explicit unchanged breakpoints,
changed breakpoints while hidden, activation of a hidden stacked page, repeat
show/update behavior, and a real serialized splitter state seeded before a new
screen is constructed. Expected reference widths remain exact.

The suite uses Qt test paths and temporary configuration set before `QApplication`.
It creates no database and reads no personal vault. Offscreen widget assertions
verify the sizing lifecycle only; native rendering, supported display scales,
language modes, and screenshot acceptance remain unverified until the approved
hidden-desktop route is available.
