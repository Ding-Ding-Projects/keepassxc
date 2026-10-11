# Motion authoring handoff

This is a target-native Qt source handoff, not a rendered reference or runtime acceptance record. Material Designer discovery found no checkout at the resolved project location, registered project or checked local installation. The existing QWidget appearance editor is the implementation route for this bounded unit.

## States and interactions

The existing anchored appearance editor adds an **Animation and Transition** tab. It retains the exact appearance key as its target. A table edits named tracks and a second table edits explicit numeric keyframes. Keyboard table editing and numeric timeline entry provide alternatives to dragging. Track selection only changes the inspector. Add, duplicate, remove and move-up actions change track order. Enabled, mute and solo are explicit boolean cells. Validation rejects an invalid candidate without replacing the last saved document. Reset, session undo and redo operate on complete motion documents.

Preview states: empty, valid and stopped, playing forward, paused, reversing, completed, reduced motion, invalid retained document, unavailable track property/state/easing, hidden, retargeted and destroyed. Hiding the tab or editor stops preview playback. Live targets play normal-state tracks on show and stop on hide. Target destruction owns and destroys its player. Editing a document replaces its old playback. No target selector resolves unrelated data or code.

## Hand-maintained capability inventory

| Capability | Source / consumer | Persistence and editing | Acceptance |
| --- | --- | --- | --- |
| Height, 24..160 px | MotionDocument sampler, AppearanceApplier minimum height, preview minimum height | Scalar keyframes; integer pixel rendering | Native pending |
| Corner radius, 0..80 px | Sampler and live/preview border radius | Scalar keyframes | Native pending |
| Font size, 8..48 pt | Sampler and live/preview QFont | Scalar keyframes | Native pending |
| Spacing, 0..48 px | Sampler and live/preview padding | Scalar keyframes | Native pending |
| Border width, 0..8 px | Sampler and live/preview border width | Scalar keyframes | Native pending |
| Exact target and state | Appearance key, normal/show live consumer; sampler state argument | Target-local document, state strings retained | Other live state triggers incomplete |
| Track name/order/create/duplicate/remove | Track table; ordered replace composition | Complete motion snapshot undo/redo, 100 steps | Grouping and move-down button incomplete; repeated move-up can reorder |
| Enabled/mute/solo | Shared sampler | Boolean cells; solo applies within selected state | Native pending |
| Initial/final/intermediate keyframes | Explicit offsets 0..1, strictly increasing | 2..128 frames; add midpoint, remove interior frame, edit numeric cells | Multi-selection, copying, markers and drag timeline incomplete |
| Duration and delay | Monotonic elapsed clock; 1..60000 ms duration, -60000..60000 ms delay | Finite numeric input retained; explicit endpoints; fill before/after | Repeats, end delay, alternate, fill controls and computed starting values incomplete |
| Easing | Linear, inOutCubic, outCubic shared sampler | Named string; unavailable names retained without rendering | Custom curves, segment easing, steps and spring tools incomplete |
| Seek/play/pause/resume/stop/reverse | Shared MotionPlayer | Numeric milliseconds; no autoplay on loading editor preview | Zoom, snapping, markers, range, speed, looping, frame stepping and before/after incomplete |
| Reduced motion | MotionPolicy drives preview and live players | Authored data unchanged; static final values; no timer | Native timed evidence pending |
| Bounds and cancellation | 256 KiB/document, 32 tracks, 128 frames/track, 16 concurrent players, 16 ms timer | Validation before authoring replacement; invalid portable input retained with diagnostics | Native teardown/performance pending |
| Style copy and presets/import/export | Existing ElementOverrides JSON, existing appearance operations | Nested versioned motion object, unknown root/track/frame fields preserved | Focused source fixtures written; execution pending |
| Reset/history | Motion reset, element/global reset; bounded session undo/redo | Session snapshots only | Durable append-only history and scheduled settings incomplete |
| Local track inspector search | Independent SearchBar `appearance.editor.motion`, existing anchored builder, Ctrl+F | Names/state/property only; session-local; last valid filter retained; selected track and document unchanged | Focused fixture written, not executed; five new catalogue entries pending integration |
| Localization/accessibility | Voice catalogue keys, change notifier, table names, keyboard editing | Private key patch requires integration | English fallback until integration; three-mode native acceptance pending |

## Renderer boundaries and remaining work

QWidget has no native CSS custom-property renderer or document/scroll/view timeline model. Portable inputs for these capabilities remain data, with no live preview. Layer targeting, transitions, hover/focus/press/selection/validation/loading/success/warning/error trigger authoring, repeat/fill/direction controls, paths, transforms, colors, opacity/elevation composition, grouping, sequencing, conflict tooling, arbitrary supported QWidget properties, advanced inspector tooling and palette teleport require implementation. These are incomplete product capabilities, not blanket platform impossibilities.

Unknown fields are preserved verbatim and have no renderer effect. Unknown schema versions remain portable but cannot be edited or rendered in this unit; reset remains available. Known scalar tracks with unavailable property/state/easing are retained and omitted by the normal-state consumer. There is no claim that this is the complete canonical motion workspace.

No mock images, built screenshots, timed recordings or native interaction receipts accompany this source handoff. The normal/minimum geometry, English/Cantonese/bilingual, light/dark and display-scale matrix remains pending against a future exact build.
