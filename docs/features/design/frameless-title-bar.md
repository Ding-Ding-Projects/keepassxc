# Frameless window with a Material title bar

Feature id: `frameless-material-title-bar` · Category: Design and appearance

## Behaviour

The desktop's caption is not product chrome. On Windows the main window keeps its native frame (resize borders, shadow, snap layouts, the minimise and restore animations, the system menu) but hands the caption strip to the application: `Material::TitleBar` (`src/gui/material/MaterialTitleBar.h`) is the first row of the shell, 44 px tall in the reference anatomy, with the shield mark, the product name, the open database's name beside it, and the three caption buttons (Minimise, Maximise or Restore, Close) at 46 × 44 px each on the right.

Dragging the bar moves the window, double-clicking it maximises or restores, right-clicking it opens the system menu, and the Close button turns the desktop's own red on hover so the one destructive control reads the way every Windows user expects. The bar paints from the theme, so a dark application has a dark caption without asking the desktop for one.

## Configuration

None. Launching with `--native-caption` keeps the desktop's caption for diagnosis; the bar still renders and its buttons still work.

## How it works

`Material::WindowChrome::installFrameless()` (`src/gui/material/MaterialWindowChrome.cpp`) asks the window manager to recompute the frame, and `MainWindow::nativeEvent` answers two messages through `WindowChrome::handleNativeEvent()`: `WM_NCCALCSIZE` returns a client area that starts at the top edge (inset by the frame thickness when maximised so the first row is not lost off-screen), and `WM_NCHITTEST` reports the resize borders, then the bar minus its buttons as caption, then client. The bar itself only emits requests; the window decides.

## Failure modes

On an operating system other than Windows the bar renders inside the client area beneath the native caption; the frameless behaviour is Windows-only by design. If the window manager refuses the frame change the native caption stays and the bar is simply a second one, which is visible rather than silent.

## Security considerations

None beyond the window: the bar reads the window title and nothing else.

## Verification

`testmaterialtitlebar` covers the button requests, the 46 × 44 targets and their order, the caption hit-test excluding buttons, the maximise/restore glyph swap, the shell placing the bar above the rail, and a 320 px width keeping every button inside the bar. Parity row `shell-default` carries the client capture; `design/parity/evidence/shell-default/window-frame.png` is a whole-window capture from the off-screen desktop showing no desktop caption above the bar.

## Suggested articles

- [Material Design 3 appearance](material-3-appearance.md)
- [Design-reference parity](design-parity.md)
- [Tabs](../navigation/tabs.md)
