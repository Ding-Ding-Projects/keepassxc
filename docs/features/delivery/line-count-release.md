# Line count in every release

Feature id: `line-count-release` · Category: Build, install and update

## Behaviour

`scripts/count-lines.mjs` counts tracked files by area, holds vendored, third-party, upstream-documentation and translation rows out of the project total, reports generated lines, attributes surviving lines to agents and people with `git blame`, and refuses a table whose attribution does not add up. The publish job runs it at the released commit and appends the table to the release notes together with the workflow start, completion and duration.

## Configuration

`--no-attribution` skips blame; `--json` prints the raw result.

## Failure modes

An inconsistent attribution total exits non-zero and blocks the release notes.

## Security considerations

None.

## Verification

Run the script locally and compare with the newest release body.

## Suggested articles

- [Dim sum release code names](../delivery/release-code-name.md)
- [Unsigned Squirrel.Windows installer](../delivery/squirrel-installer.md)
