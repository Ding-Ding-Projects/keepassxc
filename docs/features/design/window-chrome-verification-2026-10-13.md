# Window movement and resizing investigation

## Scope and result

The reported intermittent title-strip dragging and resizing problem remains
unresolved. No window-chrome implementation was changed. The tested states refute
missing native frame styles and loss of hit testing after Always on Top as simple
explanations. They do not reproduce or disprove an intermittent pointer failure.

## Source and executable identities

| Identity | Verified value |
| --- | --- |
| Current source | `2b2a683ba16d9a5dc116808100c2e8418f4fa681` |
| Exact entrypoint | `build.bat /s -WithGuiTests` |
| Build result | Exit 0, completed `2026-10-13T04:02:53.6619988Z` |
| Current executable SHA-256 | `f3f8fdc487e2ee43b9d5697ca7486f8b54ecde13a2f9d90b2769ae17da51b491` |
| Installed version | `2.8.32701` |
| Installed executable SHA-256 | `aec444f78aa4f446ddcd4e5a0d9fed9455703a8f0c4b75a7a5233f2fa3bc2ff0` |
| Installed release source | `3356cdfece47a33fca6fb02db4dcd82f8c576e12` |
| Historical comparison source | `3ce199347e48d0f5497dd1646cff73965152c187` |
| Historical executable SHA-256 | `b3c6034388cd0e9295e73c55c9058bb36d0fb8f1e7869a4a13ea0a3cef277089` |

The installed version's window-chrome and title-bar sources are identical to the
current candidate's. Relevant MainWindow native-event, show-event and window-flag
handling are also unchanged. The installed executable lacks the later isolated
verification-profile route, so it was identified read-only and was not launched
against personal data. Source identity does not establish installed behavior.

## Native observations

Current and historical executables ran on separate named hidden Windows desktops
with unique verification profiles and no vault open. Launches were bound to the
executable hash, source receipt, process creation time and live window ownership.
The installed Lowlevel runtime's background window binding supplied the route.
No visible-desktop input was performed.

At native DPI 144 (150%), the current candidate rendered a 1200 by 800 logical
surface. Physical-pixel captures measured 1800 by 1200. Private raw captures and
native-message receipts are retained separately, with no capture substituted for
an interaction result.

| Operation | Observation |
| --- | --- |
| Normal window style | `0x96cf0000`, including caption, thick frame, system menu and minimize/maximize capabilities |
| Title strip `WM_NCHITTEST` | `HTCAPTION` (`2`) |
| Content `WM_NCHITTEST` | `HTCLIENT` (`1`) |
| Left, right, top, bottom | `10`, `11`, `12`, `15` |
| Top-left, top-right, bottom-left, bottom-right | `13`, `14`, `16`, `17` |
| Maximize button background input | Native maximized style `0x97cf0000`; borders stop reporting resize codes |
| Restore button background input | Normal geometry/style and all eight resize hit-test codes return |
| Always on Top through the command palette | Extended style becomes `0x108`, including `WS_EX_TOPMOST`; HWND stays unchanged on this run and caption/eight-edge hit testing still works |

The flag-change observation matters: Qt can reconfigure an existing native handle.
Do not assume every window-flag change creates a different handle or infer a
lifecycle failure merely from the handle staying the same.

Historical native `SC_MOVE` and `SC_SIZE` keyboard-message probes changed placement
and sizing. The movement probe reached maximized placement and the sizing probe
returned a larger normal rectangle. These prove responses to system commands,
not controlled physical title-strip or edge dragging. Posted non-client/mouse
messages alone did not move the historical window.

## Remaining verification

- [ ] Reproduce the exact gesture, normal/maximized state, title-strip location,
  display scale and preceding window-state transition.
- [ ] Prove physical title dragging and all eight edge/corner drag gestures through
  a supported isolated input route. The available `mouse_drag` operation requires
  foreground input and was not used. Posted messages are not equivalent evidence.
- [ ] Execute the native title-bar test. Its executable SHA-256 is
  `497faba5d65161947f1451ee06093fdbaa64a3101a229488774177500e74dab3`.
  Application Control blocked it before execution: `CreateProcessW failed
  (GetLastError=4551): An Application Control policy has blocked this file.`
  Zero cases ran. Security settings were unchanged and no alternate route was
  used to evade this block.
- [ ] Verify minimum sizing, monitor/DPI transitions, snap behavior and any
  confirmed repair against the same reproduction.

No installer update, release, installed-profile migration or completion claim is
part of this diagnostic result.
