# Handoff — Material 3 Expressive rewrite and shared UI requirements

A working prototype plus the Qt contracts to build it against. The prototype is
the source of truth for every number in this document: where the two disagree,
open the prototype, click the element, and read the inspector.

**Entry point: `KeePassXC Material.dc.html`.** Everything else is a destination
mounted into it. The destination files open on their own too — each loads the
same `lib/kpxc.css` — but a page opened alone has no rail, no app bar and no tab
strip, because those belong to the shell.

---

## 0. Where these files are, and how to drop them in

**These files were written into this design project, not into the KeePassXC
working copy.** The repository was mounted read-only for the session that
produced them, so nothing here has touched `develop`, no branch was created and
no commit was made. Landing them is a deliberate act by whoever picks this up —
which is the right default for a password manager, but it does mean step 1 is
manual.

Everything lives under `handoff/`, laid out in the repository's own directory
shape so the copy is a straight overlay:

```
handoff/
├── HANDOFF.md                     ← this file
├── MANIFEST.md                    ← every file to create or modify
├── ACCEPTANCE.md                  ← the shared UI checklist, one row per rule
├── TOKENS.md                      ← the MaterialTheme.cpp diff
├── sheets-additions.json          ← new pages for utils/design/sheets.json
└── src/gui/material/              ← ten new classes, headers + skeletons
    ├── MaterialBreakpoints.{h,cpp}
    ├── MaterialShapeMorph.{h,cpp}
    ├── MaterialSearchRegistry.{h,cpp}
    ├── MaterialTabOverflow.{h,cpp}
    ├── MaterialFabMenu.{h,cpp}
    ├── MaterialRegexTokens.{h,cpp}
    ├── MaterialRegexSafety.{h,cpp}
    ├── MaterialElementOverrides.{h,cpp}
    ├── MaterialVoiceStrings.{h,cpp}
    └── MaterialExternalEditor.{h,cpp}
```

To land it, from the root of a KeePassXC checkout, with this project unpacked
alongside it:

```sh
git switch -c feature/material-expressive develop

# The ten new classes. Nothing is overwritten: none of these exist in the fork.
cp -n ../keepassxc-material-design-3-rebuild/handoff/src/gui/material/*.{h,cpp} \
      src/gui/material/

# The prototype and its design system, kept beside the sheet catalogue's own
# source data so the two stay together.
mkdir -p utils/design/prototype
cp ../keepassxc-material-design-3-rebuild/*.dc.html utils/design/prototype/
cp -r ../keepassxc-material-design-3-rebuild/lib    utils/design/prototype/

# The documents.
cp ../keepassxc-material-design-3-rebuild/handoff/*.md   docs/design/
```

Then, before anything else:

1. Add the ten source pairs to `src/gui/CMakeLists.txt`. They will not build
   until you do, and a header nobody compiles is a header nobody checks.
2. Merge `handoff/sheets-additions.json` into `utils/design/sheets.json` and
   **re-run `utils/generate_sheet_catalogue.mjs`.** Do not hand-edit
   `MaterialSheetCatalogue.{h,cpp}` — it is generated, and the whole point of
   generating it is that a transcription can be diffed rather than trusted.
3. Add the Config keys from `TOKENS.md` to `src/core/Config.cpp`. Every row in
   the prototype's Settings hub names the key it binds; a row whose key does not
   exist yet is a row that will silently do nothing.

`cp -n` is deliberate: if any of those filenames has appeared in the fork since
this was written, the copy should refuse rather than clobber it.

**One caution on the prototype copy.** `utils/design/prototype/` is reference
material, not build input — it must not end up in `install()` rules or in the
packaged artifact. The previous MSI failure in this repository was exactly this
class of mistake: `share/windows/wix-template.xml` declared Start Menu shortcuts
to files that a feature flag had removed from the install, and `light.exe`
refused to link. Nothing is packaged unless something explicitly packages it.

---

## 1. What is here

| Path | What it is |
| --- | --- |
| `KeePassXC Material.dc.html` | The shell: window chrome, rail, app bar, tab strip, destination stack, FAB menu, snackbar host, command palette, notification centre, dim sum card, Qt inspector |
| `Vault.dc.html` | Group pane · entry list · detail pane |
| `Reports.dc.html` | Health stats and the six findings reports |
| `History.dc.html` | Git-backed revision timeline, diff, restore |
| `Changelog.dc.html` | Every released version, date filter, regex search |
| `Settings.dc.html` | The eleven-page hub, every row bound to a Config key |
| `Appearance.dc.html` | Theme, typography, voice, element overrides, motion |
| `Sheet.dc.html` | Entry · Database · Tools · Help spec sheets (27 pages) |
| `RegexBuilder.dc.html` | The builder, anchored to whichever search bar opened it |
| `lib/kpxc.css` | The whole token set — colour roles, shape, motion, breakpoints |
| `lib/regex-lab.js` | Tokeniser, explainer, bounded evaluator, dialect export |
| `lib/copy.js` | Bilingual strings and the five funny levels |
| `lib/vault-data.js` | The demo vault. No real credential is in this repository |
| `lib/contracts.js` | The per-widget Qt contracts the inspector reads |
| `handoff/src/gui/material/*` | Ten new classes: documented headers and .cpp skeletons |
| `handoff/TOKENS.md` | The MaterialTheme.cpp diff |
| `handoff/MANIFEST.md` | Every file to create or modify |
| `handoff/ACCEPTANCE.md` | The shared UI checklist, one row per rule, with where to look |
| `handoff/sheets-additions.json` | New pages in the format `utils/design/sheets.json` already uses |

## 2. What changed, and why it is not a reskin

The existing `src/gui/material/` layer is Material 3. This is **M3 Expressive**,
which is a different commitment in four specific places:

- **Shape morphs on press.** Rail indicator 17 → 10, FAB 28 → 16, filled button
  Full → 14, chips 8 → 16. The transition is a spring
  (`cubic-bezier(.2,.9,.24,1.06)`), not an ease — the overshoot is what makes it
  read as physical rather than as a slow resize. `MaterialShapeMorph` is new.
- **Weight-driven hierarchy.** The active rail label is 700 against 400, not a
  colour change alone, so the destination is legible without relying on hue.
- **Colour-flooded containers.** The entry detail hero floods with the health
  state's container role — `ErrorContainer` for breached, `PrimaryContainer` for
  healthy — instead of a tinted strip. It is the first thing read on the pane.
- **Asymmetric layout.** The group pane is 250 and the detail 392; the list takes
  the remainder. The three panes are deliberately not equal, and they drop in a
  fixed order as the window narrows (§4).

Everything else is the existing token set, unchanged. `Material::Theme` still
owns colour; no widget hard-codes a value.

## 3. The ten destinations

Unchanged from `MainWindow.cpp:777`: vault, reports, editor, database, tools,
history, changelog, settings, appearance, help. Symbols unchanged.

The rail is **painted, not built from widgets**, so nothing on it is a QAction
and none of it reaches the command palette on its own. `Shell` carries one
QAction per destination for exactly this reason. **Keep that when adding a
destination** — this is the single easiest thing to break here, and it breaks
silently: the rail keeps working and the keyboard route quietly disappears.

## 4. Breakpoints

`MaterialBreakpoints.h` is new and carries the boundaries. Logical pixels, so a
200% display at 2880 physical px is Large, not ExtraLarge.

| Class | Width | Layout |
| --- | --- | --- |
| ExtraLarge | ≥1440 | rail 88 + group 250 + list + detail 392 |
| Large | ≥1200 | rail 88 + group 216 + list + detail 360 |
| Expanded | ≥840 | rail 88 + list + detail 340; groups fold into the search scope |
| Medium | ≥600 | rail 72 icons-only + list; detail opens as a sheet |
| Compact | <600 | bottom bar (5 destinations) + list; the other 5 in More |

**Nothing is ever merely clipped.** Every dropped pane's content stays reachable
somewhere else. That is the no-clipping requirement, and it is why the tab overflow is
a searchable sheet rather than a scroll.

## 5. The regex work

This is the largest single piece. `MaterialRegexBuilder` gains two new
collaborators, `MaterialRegexTokens` and `MaterialRegexSafety`, and one new
piece of infrastructure, `MaterialSearchRegistry`.

**Twelve surfaces carry a search bar wired to the builder:** vault entries, the
group tree, every settings page, every sheet page, changelog, history, reports,
the command palette, the notification centre, the appearance editor, attachments
and custom fields, and the tab list.

**Plain text stays the default.** Regex is opt-in per bar, and the query,
pattern, flags, validation and mode synchronise bidirectionally.

**The pattern returns to the bar that opened the builder.** Never
unconditionally to the vault. This is what `SearchRegistry` exists for: with a
dozen bars appearing and disappearing as destinations mount, the obvious
shortcut is a bug a user meets the first time they refine a Settings filter and
find their vault search replaced.

**The engine is QRegularExpression (PCRE2), and the builder says so on screen.**
The prototype executes ECMAScript RegExp because a browser has nothing else; the
Dialects tab names every construct where the two differ — atomic groups,
possessive quantifiers, recursion, `\\A`, `\\z` — rather than pretending they
are the same language.

**Evaluation is bounded twice.** A static shape check flags nested quantifiers
before anything runs; a 120 ms wall-clock budget stops the match loop after. The
limits are constants, not settings: a user who could raise them would be given a
way to hang their own vault. When the budget expires the surface says so — the
matches shown are real, and there may be more.

## 6. The voice system

`MaterialVoiceStrings` replaces `MaterialVoice`. Three language modes and two
independent 1–5 sliders, English and Cantonese, persisted separately.

**The invariant, and the only thing worth reviewing in that file:** the five
levels of a message say the same thing. They differ in how it reads, never in
what it reports. If level 1 names the file, level 5 names the file. If level 1
says the action is irreversible, so does level 5. If level 1 quotes the error
verbatim, so does level 5.

The generator **fails the build** when an entry's five levels do not share the
same placeholder set. That check is the only thing standing between a funny
level and a vaguer warning, and it is cheap.

Compare in `lib/copy.js`: `saveError` at level 1 and level 5 both name the
database, both say another process holds the file, and both say the changes are
still in memory. Level 5 adds a cat.

## 7. History

`HistoryStore` already holds a `QWeakPointer<Database>` and follows the root
group's destruction — a strong pointer kept the decrypted database reachable for
the life of the window. Rows already hold a `QPointer` to the revision rather
than a position in `Entry::historyItems()`, because `truncateHistory()` drops
from the oldest end and every surviving index shifts.

**Both of those must survive this rewrite.** They are not visible in a
screenshot and they are the two defects in this area that mattered.

The new surface adds: a kind filter, a per-revision diff, and restore as an
explicitly-labelled **new** revision. The prototype states that in the UI, on the
banner above the timeline, because a history panel whose restore discards the
branch it replaced is the one failure mode that makes the feature unsafe to use.

## 8. What the prototype does NOT prove

- **Nothing here has been compiled.** These are headers and skeletons, not a
  build. `-fsyntax-only` on a working tree proves nothing about a commit.
- **The Qt regex dialect is described, not executed.** Every claim on the
  Dialects tab needs checking against PCRE2 before it ships as help text.
- **Contrast is asserted, not measured.** Run a checker over the token pairs in
  `lib/kpxc.css` for both modes and all four seeds before calling §4 of
  ACCEPTANCE done.
- **The 1% dim sum draw is real in the prototype** — which means you will almost
  never see it. Appearance has a button that forces one.
- **Screen capture.** `WinUtils` applies `WDA_EXCLUDEFROMCAPTURE` to every
  top-level window, so any capture of the real application comes back black
  unless it was launched with `--allow-screencapture`. That has cost two
  sessions hours. It is the feature working.

## 9. Pick it up here

1. Land `MaterialBreakpoints` and reparent the rail into a bottom bar below 600.
   Everything else assumes it.
2. Land `MaterialSearchRegistry` before touching any second search bar.
3. Generate `MaterialVoiceStrings` from a new `utils/design/voice-strings.json`
   with the placeholder-parity check in the generator, the same way
   `MaterialSheetCatalogue` is generated from `sheets.json`. Do not hand-write
   the table.
4. Then the rest, in any order. `MANIFEST.md` has the full list.

Run the dead-code check from the previous handoff after each component lands —
grep for the type outside its own directory. Three finished surfaces
(`SettingsHub`, `SettingsScreen`, `GeneratorSheet`, 3 225 lines) once compiled on
every build and were never constructed.
