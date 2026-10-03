# Interface completeness and evidence

The native application and its documentation website remain incomplete. A successful
inventory schema check means the required records exist and use recognized identities.
It does not establish implementation, runtime behavior, Material Design conformance,
installed behavior, accessibility, or genuine current screenshots.

## Scope and source of requirements

[`scripts/feature-inventory-contract.mjs`](../../../scripts/feature-inventory-contract.mjs)
is the hand-maintained authority, independent of runtime discovery. It names 89
features, 106 distinct surfaces, and 998 capability subcontracts. Removing a runtime
implementation cannot remove its obligation from this list. New controls, nested
destinations and menu families must extend the list before acceptance.

The required matrix has 105,788 surface/capability cells and 9,434 surface/feature
groups. These are **coverage obligations**, not counts of distinct defects or
independent implementations. Shared code may implement a capability; each surface
still needs applicable interaction and evidence. A reviewed capture may support
several capabilities on the same surface when the receipt explicitly declares
those claims and each capability's tuple and evidence requirements are satisfied.
There is no requirement for a unique image per obligation. The sparse evidence ledger currently
contains no accepted current cells. Absence means incomplete, never exempt.

| Record | Purpose |
| --- | --- |
| [`inventory.json`](../inventory.json) | Version 2 platform-level summaries. All 172 original rows and their source/evidence links survive; six newly named platform rows bring the total to 178. Historical status is retained as `priorStatus`. The former implemented personal-wording row is partial until current evidence exists. |
| [`surface-inventory.json`](../surface-inventory.json) | Explicit native and website surface identities, ownership boundary, source hints and assessment state. `source-present` asserts only that a named file existed at the recorded assessment revision. |
| [`capability-inventory.json`](../capability-inventory.json) | Every required capability, its section, its persistence obligation, and sparse per-surface proof records. |
| [`feature-evidence.mjs`](../../../scripts/feature-evidence.mjs) | Bounded-path, revision, hash, interaction, privacy and completeness validation. |
| [`test-feature-inventory-guard.mjs`](../../../scripts/test-feature-inventory-guard.mjs) | Deliberate removal/corruption and restoration tests, isolated from real runtime claims. |

The native list separates shell, vault, reports, history and changelog; application
settings tabs (including Browser Integration, Keyboard Shortcuts, SSH Agent,
KeeShare and the appearance hub); database settings tabs and Security container;
entry editor sections including Properties; separate Group, Icon, Properties,
Browser Integration and KeeShare group-editor sections;
create/unlock/import/CSV/export/merge/clone flows; passkey and TOTP dialogs; attachment
editing/preview; notifications, palette, appearance and regex; every application,
tab, collection, text-editing and rendered-element menu family; dropdowns; locks,
support, authenticator, converter, local models, offline help, schedules and logo;
and the three extension-download states.

Report obligations separately name Statistics, Password Health, Passkeys, Browser
Statistics and Have I Been Pwned, plus the embedded entry-editor stack page.
Creation separately names Metadata, Encryption and Database Key; import separately
names Selection and Review. The family rows remain, and conditional browser-report
registrations remain obligations rather than silently disappearing with a build
flag. The ReportsDialog and both wizard constructors have been checked for sibling
page registrations. The import review's embedded CSV controls retain the existing
CSV surface obligation; table widgets and report cards are content, not additional
registered pages. Existing entry-editor section obligations also remain in force.

Website overview, downloads, documentation, changelog, settings sections, tabs,
dialogs, menus, dropdowns, regex, palette, appearance, notifications, locks,
authenticator, history, converter, local model equivalent, logo and status remain
distinct visitor-owned surfaces. Their controls operate on website content and
visitor state. The website is not a browser-hosted password-manager runtime.

## Capability coverage

The registry explicitly includes the following material subcontracts. The complete
list, including each individual property and input route, is machine-readable in
the capability inventory rather than reduced to these category descriptions.

| Contract family | Required details |
| --- | --- |
| Languages and personal wording | English, Cantonese, bilingual, both independent five-level controls, emoji rules, local bounded upload/cache/replace/clear, literal boundaries, no public or diagnostic leakage. |
| School mode and narration | Shared live state and rename, credential/reset, discovery suppression/restoration, serialized narration, independent stable voice identities, delayed enumeration, offline/missing voices, rate/pitch and assistive-technology coexistence. |
| Schedules and sources | Every appearance value, dates/times/weekdays/timezone/DST, precedence/migration, base restoration, bounded HTTPS and Home Assistant sources, credential protection and recovery. |
| Search and menus | Every surface and dropdown/menu family; independent query, pattern, flags, mode, history and snippets; adjacent full builder; exact target actions; keyboard/touch equivalents; no short-menu exemption. |
| Regex workbench | Guided and raw engine constructs, visible unsupported constructs with reasons, explanation tree, annotations, matches/captures, replacements, case suites, snippets, import/export, navigation, timing, risk diagnostics, bounded trace/evaluation. |
| Material components and motion | Registered primitives with provenance, full component anatomy, typography, colors, elevation, state layers, every element/state transition, interruption, final geometry and reduced motion. |
| Element appearance | Every target/state including the editor itself; layers/groups, selections, channels, masks, adjustments, embedded content, blending/effects/fills/borders, transforms/warp/perspective, geometry, filters, complete typography, inheritance, previews, inspector, undo/history, presets/import/export and granular reset. |
| Color and typography | Continuous translated color spaces, alpha/gamut/contrast, rainbow sentinel/shared phase/reduced motion, every installed/bundled font and Word-style typography, exact unsupported-capability explanations. |
| Tabs and groups | All edges, overflow, pinning and protected region, full grouping and decoration, four independent discovery searches, scoped bulk closes, move picker, persistence and keyboard orientation. |
| Element locks | Six independent ordered credential policies; per-element wizard and credential set; actual disabled-wrapper interception; alternate shortcut/automation/palette protection; keypad/manual parity and shared attempt budget; vault storage, duration/relock, recovery and redacted history. |
| Authenticator and history | Local pairing QR/manual route and typed confirmation; URI/image/clipboard/camera/manual import; RFC vectors, code/countdown/next code, skew; encrypted/redacted append-only mutation history and separately protected manager. |
| Notifications and confirmation | Nonblocking corner messages and persistent errors, review centre and bulk operations, no promotional nagging, exact destructive scope with two keys/full slider/cancel/focus return. |
| Data and documentation | Full exports with format/loss disclosure, complete ZIP/7z options, bulk scope/results/undo, encrypted local history, composed date/action/search filters, factual changelog and validated commit links, editor/VS Code handoff and bundled offline articles. |
| Converter | Eight named categories; complete verified adapter catalog; bundled offline tools; PDF inspect/split/merge/extract/reorder/rotate/metadata and reopening verification; bounded isolation, atomic writes, unlimited paged durable queue, storage preflight and partial outcomes. |
| Local models | Exhaustive official catalog/tags/pages and stale state, conservative evidence-backed fit, durable bounded pulls, full local chat, capability attachments, registered launch profiles, preflight, snapshot/restore/rollback, secret exclusion and offline recovery. |
| Remaining surface behavior | Guided forms, truthful defaults, rich controls, novice/expert shared values, presets, attention accommodations, local logo conversion, command palette, resizable decorated overlays, actual shortcuts, origin progress and contextual recovery. |
| Delivery and evidence | Front-screen version/build time with timezone and seconds, Status Hub and truthful reply delivery, verified downloads, social graphics, complete vendored fonts, package/update provenance, line counts, current screenshots/recording and deterministic design parity. |

The current explicit project instruction governs the startup surprise: a fresh 1%
chance per launch, no more than one display per launch, and a persisted off switch
that is honored absolutely. The earlier installed guidance specified 10% and no
off switch; that conflict is resolved by the current user instruction's priority.
The explicit capabilities are `one-percent-per-launch-draw` and
`persisted-off-switch`, `at-most-once-per-launch` and `no-mid-task-flow`.
Current `MaterialDimSum.cpp` still uses denominator 10 and ignores the retired
off preference, so those runtime mismatches remain open. Acceptance must prove disabled/restarted suppression,
first-run/error/update/mid-task exclusions, bundled local images with meaningful
dish alt text, automatic dismissal, no focus theft, quiet settings and reduced
motion. All behavioral evidence remains pending; this inventory lane changes no
runtime frequency or control.

## Evidence record and acceptance

Each capability row identifies `surface`, `feature`, `capability`, `status`, and
`evidence`. A complete record requires:

1. `implementation`, `configuration`, `registration`, `localizedCopy`, `article`
   and `test` references, plus `persistence` for every stateful capability.
   Every reference has a repository-relative file and SHA-256. Source/test/config/
   registration/persistence references additionally identify a one-based line and
   its entire exact literal text. Arbitrary regular expressions are not executed.
2. The referenced source must match both the file hash and the bytes at the
   explicitly pinned Git revision. CRLF versus LF is normalized for the revision
   comparison only. A current file cannot impersonate an older build's source.
3. English, Cantonese and bilingual copy coverage. Declaring these languages is a
   structural assertion, not a substitute for driving the three rendered modes.
4. A hashed `interaction` JSON receipt of kind `built-ui-interaction`, schema 1,
   with the exact source commit, expected built executable/package SHA-256, cell
   identities, hidden isolated nonpersonal fixture, approved method
   `lowlevel-headless-built-artifact`, no synthetic/DOM-injected surface, privacy
   review, and visual-inspection review confirming every frame was opened.
5. A tuple naming screen, state, theme, language, viewport width/height, and display
   scale. A sequence of numbered steps names input method, exact target, accessible
   name, action, pre-state, expected and actual post-state, verdict and its own
   capture. Every capture's hash and PNG dimensions must match its tuple. The final
   capture must be the final step's capture.
6. A hashed `focused-test-result` JSON receipt, schema 1, bound to the same source,
   artifact and test-file hash. It records command, nonzero passing count, zero
   failures, and an observed negative regression failing when broken and passing
   after restoration.

Language-mode and clipping-matrix language claims must match the receipt's
rendered language. Clipping-matrix theme and scale claims must match the actual
tuple: a light frame cannot prove `dark`, and scale 1 cannot prove `scale-200`.
The `normal-minimum` capability requires all 48 combinations of English,
Cantonese and bilingual, light and dark, scales 1/1.25/1.5/2, and normal/minimum
viewports. Tuples include `viewport: "normal"` or `"minimum"`; logical dimensions
stay consistent for each viewport across the matrix, and minimum is smaller.
The primary `interaction`/`capture` plus 47 additional pairs in `evidence.matrix`
carry this proof. Every pair undergoes the full validation above. Missing or
duplicate combinations fail, including bilingual/dark/2/minimum. Individual axis
examples cannot substitute for the combined matrix. The real capture review must
also confirm that the declared dimensions are the surface's supported minimum
and normal sizes; labels alone cannot establish that runtime fact.

All 48 tuples must name the same `state` as the primary interaction. An empty
state cannot replace a populated state at a difficult combination, even when its
step tuples and receipt hashes agree. A different state needs its own full matrix;
this validator does not discover or prove the complete required state inventory.
Independent review must still establish that the selected states cover the actual
surface behavior before accepting implementation.

An interaction may list additional `{ "feature", "capability" }` records in
`claims`, all for its exact surface. Reuse preserves every claim's tuple,
provenance and source requirements. It does not permit evidence from another
surface or a mismatched language, scale or theme.

Paths are restricted by role to relevant project source, documentation, test and
evidence roots. Absolute paths, traversal, encoded paths, alternate streams,
hidden/admin paths, symbolic links/junctions, hard links, unsupported file types
and oversized files are rejected before content is read. Password databases and
personal credential material are not accepted as evidence paths. Source references
are bounded to 8 MiB; PNG evidence is bounded to 32 MiB. Large media keeps its
existing separate transfer route.

**A valid self-authored receipt is not proof of authenticity.** This validator
checks record integrity, bindings and coverage. It cannot authenticate a person,
infer privacy from pixels, validate every visual property from a PNG header, or
prove a capture route was actually used. Independent review of genuine captured
pixels and the external capture/build receipts remains mandatory before adding an
implemented row. The schema test's simulated records are deliberately temporary
and never qualify as production evidence.

Historical parity images and personal-wording images remain untouched. Their older
source revisions, missing current candidate bindings or missing full interaction
receipts prevent acceptance for the current interface. They must not be relabelled
as current merely because the files exist.

## Commands and verdicts

```powershell
node scripts/check-feature-inventory.mjs --schema-only --summary
node scripts/test-feature-inventory-guard.mjs
node scripts/check-feature-inventory.mjs --summary
node scripts/check-feature-inventory.mjs --source=<full-commit> --app-artifact=<sha256> --site-artifact=<sha256>
```

The first command exits 0 only for a valid inventory schema. It still prints the
independent product verdict. The default/product command exits 1 while any required
surface/capability, summary status, current evidence, or schema requirement is
incomplete. It never treats a schema-only pass as a release decision.

The focused tests remove every registered feature row, surface and capability;
exercise duplicates, unknown identities, legacy records and site ownership;
then corrupt source, hash, revision, artifact, persistence, configuration,
registration, interaction, capture, tuple, privacy, inspection and test evidence
one at a time. Each corruption must be rejected and the restored fixture accepted.
A single valid simulated cell still leaves the entire product incomplete.

`migrate-feature-inventory.mjs` is an explicit one-time version 1 migration. It
preserves every original row and link, retains its prior status and marks its proof
historical. It refuses to overwrite version 2 or existing companion inventories.
The loader can read version 1 for inspection, but marks it `migrationRequired` so
legacy input remains red until deliberately reviewed and migrated.

## Candidate evidence snapshot

- Menu baseline at tests-only commit `5c33af6b74c18bb43d0f0c932e9a38fd71e06c16`: 15 passes and 8 failures. The repair at `38959fc653ed7115b573c543f65fb7f2b2b9a97d` reports 23 passes and zero failures; independent follow-up review remains pending. These offscreen Qt results establish no native interaction or screenshot acceptance.
- Motion candidate `65240d644a4d852b8fc8bfd3ad1f2fe08ad0e29a` built, but reported two explicit failures: `preferencePersistsWithoutDiscardingBaseChoice` read the roaming file instead of the isolated local file, so production preference loss is not established; `reversalStartsAtCurrentValueAndSettlesOnce` observed scalar `0.304590151897` jump synchronously to `0.579813392396`. The next case, `reducedSwitchAndOverlayHaveImmediateFinalStates`, exited `0xC0000005` before its first assertion in the full run and one isolated diagnostic. Constructor `hideEvent` before transition initialization is a source-review hypothesis, awaiting repair and refutation. This evidence is red and does not establish native acceptance.

Both candidates remain separate from release `v2.8.29801` at this snapshot.
No inventory status is upgraded from these results.

## Current boundary and next action

### Required smoke and capture acceptance

Every item below is pending until KeePassXC profile and history isolation is
established. This is a finite acceptance sequence for the combined candidate, not
a substitute for the wider per-surface feature inventory. A passing source check,
offscreen Qt suite, package build or download does not complete a native item.

- [ ] Bind the exact package, source commit and executable SHA-256 to the run.
  Verify the installed or staged executable actually launched has that hash, and
  record its version, process identity and fixture identity before interaction.
- [ ] Establish a fresh nonpersonal profile, configuration, data/history paths and
  Qt identity before `QApplication` or production initialization. Environment-only
  APPDATA/LOCALAPPDATA changes are insufficient for Windows known-folder isolation.
  Preserve unrelated processes and never open a personal database.
- [ ] Create ten deterministic synthetic entries in two groups through the actual
  interface. Retain their expected fields locally within the disposable fixture;
  receipts record comparison outcomes without publishing credential field values.
- [ ] Save atomically through the real application path. Confirm the intended file
  exists, the save operation succeeds and the modified state clears. Do not treat
  a dismissed dialog or changed filename alone as successful persistence.
- [ ] Choose the actual Quit action and prove the owned process exits. A hidden
  window or minimized process is not exit. Relaunch as a distinct process and
  compare all ten entries, group membership and exact expected field values locally.
- [ ] Repeat reopening through the positional filename route using a path with
  spaces and Cantonese characters. Prove the intended database opened without
  substituting a file-picker route, and repeat the exact local field comparison.
- [ ] Exercise menu plain-text and regex filtering, inline builder open/apply/close,
  invalid/resource-limited patterns, disabled-action protection and original action
  activation. Check keyboard navigation, Escape and focus return, popup containment
  and restored width after the builder closes.
- [ ] Interrupt and reverse motion while it is active. Verify final geometry,
  reduced-motion and low-stimulation behavior, and the operating-system veto.
  Verify first-visible vault pane widths, hidden-page restoration and preservation
  of a remembered user division through resize and hide/show.
- [ ] Exercise startup-surprise eligibility and exclusions: first run, error,
  update and mid-task flows, persisted off after restart, at most once per launch,
  quiet/reduced-motion behavior and no focus theft. A random absence does not prove
  an exclusion; use the reviewed deterministic fixture and separately verify the
  declared 1% draw boundary.
- [ ] Retain a genuine capture after every click in this sequence and for each
  final state. Inspect every image for the expected transition, readable controls,
  nonblank content and privacy. Bind each frame to source, executable hash, screen,
  state, language, theme, viewport, scale, input and expected/actual outcome.
- [ ] Show the reviewed genuine captures in chat and commit their evidence to the
  repository. Embed current representative captures in README and the documentation
  website, then verify the delivered image links. Capturing and publishing are
  separate verdicts; neither is inferred from the presence of a filename.

Historical September images lack the required executable binding and cannot fill
current acceptance rows. Preserve them as historical evidence without relabelling
their source or provenance. Do not fabricate, reconstruct or substitute images
when the approved route is unavailable.

The approved hidden-desktop controller is installed using quiet standard input/output
transport at source `e6e42f2066d539256d6480401d7cef867f2b8dfe`. Installation smoke
confirmed 58 tools, the native backend, and a synthetic Qt fixture's hidden launch,
capture, background close and owned-process teardown. This is controller installation
evidence only. The fixture image is not KeePassXC evidence and is not published here.

KeePassXC profile and history isolation remains blocked pending the owner's scope
decision. No packaged lifecycle or current KeePassXC capture has run. Release
`v2.8.30301` binds source `91772a166def5e44f60dd49501f5882d7190287c`; the coordinator
downloaded and verified its seven required assets. Package verification does not
establish native acceptance. Build the exact integrated candidate and obtain genuine
per-click proof only after isolation is established. A checklist is not the
implementation it describes.

Suggested articles: [Database lifecycle acceptance](database-lifecycle-repair.md),
[Design parity](../design/design-parity.md),
[Clipping matrix](../design/clipping-matrix.md),
[Per-element appearance](../design/per-element-appearance-editor.md),
[Regex builder](../search/regex-builder.md).
