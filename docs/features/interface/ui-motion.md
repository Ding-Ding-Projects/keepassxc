# Native interface motion

MaterialMotion provides one shared native Qt motion policy and bounded scalar transitions.
The user can enable **Reduce motion** or **Low stimulation** in Settings, Appearance.
Both preferences are local, persisted settings. Either preference suppresses decorative
motion, and the Windows client-area animation preference always takes priority. Low
stimulation defaults off and never overwrites the user's separate reduced-motion choice.
The status line states whether Windows or the application preference is currently active.
The seven `motion.*` voice entries cover English, Cantonese and bilingual rendering.

## Timing and interruption

State layers use 140 ms, switches use 160 ms, selection uses 180 ms and surface scrims
use 240 ms. Every duration is capped at 240 ms. The standard easing is
`cubic-bezier(0.2, 0, 0, 1)`. A reversal starts at the current scalar value; a superseded
transition never runs a stale completion handler. Repeated identical targets do not
restart a running transition. Invalid non-finite targets are ignored.

Reduced motion reaches the final state synchronously. Enabling a veto while a transition
is running settles it immediately. Hiding or disabling its owning widget also settles it;
deletion destroys the transition and disconnects its callbacks. There is no motion polling
timer. Windows preference changes arrive through `WM_SETTINGCHANGE`, with an additional
refresh on application activation. Static and hidden controls have no running transition.

## Control and state inventory

| Family / state | Behavior and implementation |
| --- | --- |
| Filled, tonal, outlined, text, icon and floating buttons | `ButtonBase` uses the shared hover/press/focus state layer. The focus outline appears immediately. |
| Chips / filter chips | `Chip` uses the same state layer. Selection, labels, trailing action and hit testing remain immediate. |
| Switches | `Switch` interpolates its knob, track and check glyph with the shared transition; hidden switches settle synchronously. |
| Native and promoted check boxes / radio buttons | `Style`, `CheckBox` and `RadioButton` animate decorative selected fill or dot and state layers. Semantic checked state is immediate. |
| Native tool buttons | `ToolButton` uses the shared hover/press/focus ramp. Icon and action state are immediate. |
| Navigation rail | Existing selection and hover ramps now obey the shared policy and visibility lifetime. |
| Database tabs | `TabStrip` fades the new active container without moving tabs or retaining old page contents. Closing, reordering, overflow and focus remain immediate. |
| Segmented choices | `SegmentedButton` fades the new selected container; selected state and accessible description change immediately. |
| Sheets, modal decisions, generator and regex overlays | `Overlay` fades only the scrim. Child geometry stays fixed, no opacity effect captures the sheet, and dismissal hides its contents immediately. |
| Notifications and dim-sum card | Existing bounded entrance/exit motion uses the shared policy. Hiding stops their lifetime timer. Dismissal and focus semantics are unchanged. |
| Indeterminate linear progress | A sweep runs only while visible, enabled and motion is permitted. Reduced motion displays a stationary segment; it never falsely reports a completion percentage. |
| Determinate progress, slider handles and numeric values | Data changes are immediate and truthful. No interpolation of the displayed value or pointer target is introduced. |
| Appearance rainbow override | An explicit user-selected continuous effect only. It settles to one hue under either veto and stops its timer when no matching target is visible. |
| Text fields, search fields, dates, combo/select fields and editors | Text, caret, validation and focus remain immediate. No captured pixels or geometry animation is applied. Existing native field painting remains; an animated decorative field outline is not implemented in this change. |
| Menus, list/table/tree rows and disclosure arrows | Existing semantic hover, selection and expansion stay immediate. Per-row and menu-item interpolation is not implemented in this change. |
| Scrollbars, title bar, colour picker, cards, dividers, labels, badges and static icons | Existing input/data state stays immediate. Static decoration does not create timer work. |
| Page replacement, vault lock/hide and credential clearing | Immediate, without snapshots, cross-fades, queued hiding or copied framebuffers. Navigation indication supplies the transition. |

The inventory is explicit about remaining instantaneous families. It is not a claim that
every rendered element has a bespoke animation, nor that the whole application has passed
visual or accessibility acceptance.

## Verification and limitations

`testmaterialmotion` exercises composition of all three vetoes, persistence, reversal,
rapid changes, non-finite input, mid-transition preference changes, hidden/disabled/deleted
owners, switch/overlay final states, progress timer suppression, the legacy appearance
bridge and localized visible preference controls. Its application data identity and Config
files are isolated before control construction. The platform query is injectable so OS
preference changes can be tested without modifying the host's settings.

Animation retargeting preserves the currently published scalar while Qt configures the
next endpoints. Construction creates hide-event prerequisites before explicitly hiding
overlays and notification cards. Finite notification timers stop while hidden and resume
when a settled notification becomes visible again; hiding during entrance settles that
entrance without running a hidden timer. Snackbar focus and hover still hold its timer.
The focused notification matrix covers both notification types, direct and parent hiding,
standard and reduced motion, and interrupted and settled entrances.

The first notification hide/show regression at source `876db09c36b6d2cd2b526a567cae63bbb6ce1e86`
returned four failing data rows, all at the timer-active assertion after showing again.
Timer resumption was repaired only after that result. The appearance, responsive-shell,
tabs and motion test programs enable Qt test paths and unique application identities before
constructing QApplication, and provide separate temporary roaming and local Config files.
The persistence regression reads the local file and reloads it through Config before
creating shared motion singletons. Offscreen results remain separate from native acceptance.

A negative regression should temporarily remove the policy-change settlement call from
`MotionTransition` and run `userVetoSettlesActiveTransitionImmediately`, observe failure,
restore the implementation, rebuild and observe success. This mutation belongs only in a
disposable verification checkout. The checked-in implementation is never altered beneath
an active build.

Native Windows interaction, screenshots, screen-reader acceptance and layout review remain
unverified until the approved hidden-desktop capture service is available. A Qt offscreen
behavioral test result cannot substitute for those checks. No credential-bearing pixmaps,
widget grabs or content snapshots are introduced by this motion implementation.
