# Animation and Transition authoring

The per-element appearance editor includes an Animation and Transition tab for a bounded first motion authoring unit. Open the appearance editor for an element, choose the motion tab and add a track. Edit its name, state, property, duration, delay, easing and boolean switches directly in the track table. Select a track to edit its keyframes. Offsets must strictly increase from zero to one and include both endpoints. Add midpoint inserts between adjacent frames; the initial and final frames cannot be removed. Invalid edits show a diagnostic and leave the saved document unchanged.

The first live properties are height (24..160 px), corner radius (0..80 px), font size (8..48 pt), spacing (0..48 px) and border width (0..8 px). Values interpolate numerically and render at integer pixel/point values. Supported global curves are `linear`, `inOutCubic` and `outCubic`. Duration is 1..60000 milliseconds and delay is -60000..60000 milliseconds. Tracks use explicit starting values, fill before and after and ordered replace composition. The last eligible track for a property wins. Solo restricts eligible tracks within the requested state; disabled and muted tracks never contribute.

The numeric playhead seeks in milliseconds. Play resumes from the current position, restarting at zero after completion. Reverse resumes backward, starting at the end when stopped at zero. Pause retains the playhead. Stop cancels the timer, resets the playhead and restores the preview's base appearance. Hiding or retargeting the editor stops its preview. Normal-state live tracks play when a matching target is shown and stop when it is hidden. They never replace actual operation completion or grant protected-action authority.

Reduced motion uses the same effective policy as other application motion. It presents static final values without playback and retains all authored values. A player uses a monotonic elapsed clock and a 16 ms timer. At most 16 players may run simultaneously; a further play request remains stopped. A document is limited to 256 KiB, 32 tracks and 128 keyframes per track. Finite numeric values, identity strings, switches and ordering are validated before an authoring change replaces saved data.

Motion is stored under each element's appearance override. Existing style copy/paste, named presets and preset JSON export/import carry that nested document. Unknown fields are retained without renderer effect. Unknown versions and unavailable values remain portable, with unavailable rendering. Reset motion clears its document. Element and global resets also clear motion. Undo/redo retain up to 100 complete snapshots during one target-editing session. Durable append-only history and scheduled settings remain incomplete.

The complete, hand-maintained implementation and remaining-capability inventory is in [the motion authoring handoff](../../../design/motion-authoring/README.md). The remaining canonical workspace includes layered/state targets, transitions and triggers beyond show, paths, transforms, colors, repeat/fill/direction editing, custom and segment easing, timeline tools, multi-selection, grouping, sequencing, advanced inspector tooling, command navigation and durable history. QWidget's lack of native CSS custom-property and scroll/view timeline rendering is a specific platform boundary; the other omissions require implementation.

## Local track inspector search

The motion tab has its own search field, separate from the appearance-property search. It filters displayed track names, states and property identifiers only. It does not inspect keyframe values, unknown portable fields, vault contents or secrets. Search is session-local and does not change the saved motion document or selected track, even when that track is hidden by the filter. Clear restores all rows without changing the other appearance query.

The existing anchored regex builder belongs to search identity `appearance.editor.motion`; its flags apply to this field only. Invalid, blocked or timed-out patterns retain the last valid filter. Row visibility updates atomically after the bounded matcher finishes, and table refreshes reapply that filter. The field exposes a visible result count or unavailable-pattern message and the same accessible description. Ctrl+F within the motion tab focuses its search. Full command-palette teleport and advanced timeline navigation remain incomplete.

New motion-search copy uses the existing motion catalogue adapter. The five new catalogue entries require integration before Cantonese and bilingual copy is complete; until then the adapter visibly uses English source text. Native localization and keyboard acceptance remain pending.

## Verification status

The focused `motionInspectorSearchPreservesDocumentAndSelection` source fixture covers independent queries, clear, case flags, invalid and blocked patterns, selected-row preservation, unknown-field document preservation and registry/builder ownership. It has not been executed.

Source fixtures in `TestMaterialAppearanceEditor` cover interpolation, mute/solo/order conflicts, timing and offset rejection, byte bounds, unknown-field persistence, preset/style round trips, cancellation, a direct live stylesheet consumer and reduced-motion behavior. Repair fixtures also cover unavailable solo tracks, atomic rejection of invalid current-schema preset imports and future-schema round trips. Independent source review accepted both repairs. These fixtures have not been executed for this candidate. Compilation, real target interactions, timed samples, performance/teardown and the complete language/theme/scale/viewport matrix remain pending. All 49 motion catalogue keys are integrated with English, Cantonese and bilingual source copy; rendered localization remains unverified.

After the normal root build, the focused native command is:

```powershell
ctest --test-dir build-windows --output-on-failure --timeout 180 -R "^(testmaterialappearanceeditor|testmaterialmotion)$"
```

No network requests, external execution, credentials or vault content belong to motion documents. Portable data is never evaluated as code.
