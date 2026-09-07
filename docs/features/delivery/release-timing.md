# Release timing finalization

Feature id: `release-timing` · Category: Build, install and update

## Behaviour

The packaging workflow creates a release with a scoped timing placeholder. Its completion workflow reads the completed run through `gh`, finds the first real job start and the successful `Create GitHub Release` step completion, and replaces only that placeholder with exact UTC timestamps and duration. It verifies the saved run id, tag, and target commit before editing notes.

The finalizer also selects the highest numeric non-draft stable release and marks it latest. This corrects release completion races without cancelling a queued packaging run.

## Failure modes

Missing job timestamps, a missing publication step, a mismatched marker, or a mismatched target commit stops finalization. The placeholder remains factual rather than claiming an inferred duration. The latest selection is retried at most three times and is verified after each edit.

## Verification

`node scripts/finalize-release.mjs --self-test` proves the version formula, numeric ordering, owned timing replacement, and missing-marker refusal.
