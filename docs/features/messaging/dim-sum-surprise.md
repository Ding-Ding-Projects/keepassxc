# Dim sum surprise

Feature id: `dim-sum-surprise` · Category: Messages, language and voice

## Behaviour

An eligible launch draws once from 100 equally likely values. Only zero selects a randomly chosen dish. The card names the dish in English and Cantonese, uses bundled SVG artwork, appears without taking keyboard focus, and dismisses after six seconds. Startup never waits for it.

`Material::DimSum::beginStartup()` captures first-run eligibility before database-opening flows can populate recent history. A profile with no recent database history is conservatively treated as a first run. Enabled automatic restoration with any nonempty remembered tab or last-active database also excludes that launch, because its embedded unlock form is a credential flow. Disabled restoration and empty records do not exclude an otherwise eligible launch. The card is also excluded when the application starts minimized, opens a command-line database, receives keyboard, pointer, wheel or touch input, opens a modal dialog or popup, reports an error or warning, or enters the update workflow. A message bar changing from information to error or warning immediately cancels the card even when that bar was already visible. Ordinary informational and success messages do not cancel the draw.

There is one startup scheduling opportunity. After a 1.5-second grace period, presentation rechecks the off preference, desktop quiet state and visible active host. An unavailable host or an excluded state cancels that launch instead of retrying later during the user's work. Destroying the host invalidates the pending presentation safely.

Programmatic database opening through the main window also cancels a pending or visible card before the database tab is constructed or selected. This includes a filename forwarded by another process, whose embedded unlock form may appear without a keyboard, pointer or modal event.

## Configuration

Settings > Behaviour contains **Startup dim sum surprise**, with English, Cantonese and bilingual labels. It persists the existing `GUI/DimSumSurprise` preference. Existing explicit `false` values remain disabled. Turning it off cancels a pending presentation and immediately hides any visible card. Turning it back on does not resurrect a canceled startup; eligibility is reconsidered on the next launch.

Windows notification quiet state and minimized startup suppress presentation. The integrated shared motion policy combines the operating-system reduced-animation preference with the persisted user reduced-motion and low-stimulation preferences. Card transitions use the shared motion controller; hiding stops the hold timer and settles the transition, and showing a settled card rearms its hold timer. Startup suppression still hides and schedules deletion immediately. Combined offscreen behavior and native acceptance are separate verification requirements.

## Security and privacy

All dish names and images come from `:/dimsum/dimsum.json` and the bundled `:/dimsum/` assets. There is no network request, telemetry or database-content lookup for this feature. The card contains only dish information. Error, update and user-interaction suppression hides it immediately rather than delaying that exclusion for an animation.

## Verification

`TestMaterialDimSum.cpp` and `TestDimSum.cpp` use synthetic configuration and unique per-run Qt organization and application identities established before `QApplication`. Private test providers make the draw boundaries and quiet-state decisions deterministic. Coverage includes all 100 draw values, one draw per launch, persisted disabling, first-run latching, remembered-database launch exclusion, active-flow cancellation, informational versus error notifications, visible message severity transitions, popup exclusion, focus preservation, timed dismissal, host destruction, local images and the localized setting. Remembered database paths in these tests are synthetic strings and are never opened.

The tests-first candidate produced 7 passes and 106 failures before the runtime repair. Two additional queued-opening regressions use newly generated, task-owned locked databases and the real main-window slot, checking both pending and already-visible cards. Their isolated profile avoids an unrelated hidden browser-settings warning, keeps the browser service stopped, and rejects input, modal, popup and error-show events as alternative cancellation causes. Both cases were observed failing before the main-window hook was added. These offscreen tests establish behavioral evidence only. Native rendering, supported geometry, operating-system notification integration and the integrated user/system reduced-motion matrix still require built-application acceptance through the sanctioned route. No native capture is claimed here.

The integrated source `7cb43f654ef2cc908a76a5a739bddfda2aa29c52` passed 167 isolated offscreen checks: `testmaterialdimsum` 127, `testdimsum` 7 and `testmaterialmotion` 33. All three processes exited 0, with no skips or timeouts. This preserves both startup cancellation and the shared motion lifetime checks. Real forwarded-process transport and native acceptance remain unverified.

## Related articles

- [Language modes and the voice catalogue](language-modes.md)
- [Dim sum release code names](../delivery/release-code-name.md)
