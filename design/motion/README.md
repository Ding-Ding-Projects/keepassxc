# Motion implementation handoff

Source of truth: native Qt implementation and `docs/features/interface/ui-motion.md`.
This is an implementation handoff, not a rendered design or runtime screenshot.

- Use Material standard easing `(0.2, 0, 0, 1)` with bounded 140/160/180/240 ms durations.
- Use paint-only scalar state transitions. Layout and interactive hit rectangles do not move.
- Treat reduced motion as a veto composed from Windows, the persisted user setting, and
  low stimulation. Every running transition must settle when the veto becomes active.
- Render focus and semantic state immediately. Never animate credential text or carry an
  outgoing vault image across a navigation, lock, clear or hide boundary.
- Keep a stationary indeterminate progress indicator under reduced motion, without claiming
  a determinate percentage. Static labels and icons never animate continuously.
- Before visual acceptance, inspect normal/minimum viewports, 100/125/150/200% scaling,
  English/Cantonese/bilingual, light/dark, keyboard interaction, rapid reversals and reduced
  motion. Record actual captures from the built executable with its source and binary hash.
- Current native capture route is unavailable. No design preview is runtime evidence.

The control inventory in the feature article distinguishes implemented ramps from immediate
state changes and identifies remaining instantaneous field, menu-item and list-row families.
