# Front-screen version and updated-at provenance

Feature id: `in-app-version-provenance` · Category: Build, install and update

## Behaviour

The welcome screen, which is the first screen before any database is opened, shows the running version, the short Git revision the build came from, and the exact moment that revision was last updated: the committer date of the built commit converted to the user's local time with seconds and a labelled time-zone abbreviation, for example `Version 2.8.8501 · revision e09d6e2 · updated 2026-09-01 19:20:33 HKT`. The value is bound to the artifact: CMake records `git show -s --format=%cI HEAD` at configure time into `KEEPASSXC_COMMIT_DATE` and the short revision into `KEEPASSXC_GIT_HEAD`, so it never comes from launch time, a file timestamp or a hand-entered label.

## Configuration

None. The line is not user-editable and does not depend on any setting.

## Failure modes

A build made without Git metadata (no `.git`, or Git absent from the build machine) carries an empty commit date; the line then says `updated-at time unavailable: the build carried no Git commit date` and `revision unavailable` rather than inventing a time.

## Security considerations

Only public build metadata is displayed.

## Verification

Launch the built application without a database and read the label named `versionProvenanceLabel` on the welcome screen; the capture route `kpxc://capture/vault` on a profile with no last databases shows it. The negative case is proven by configuring with `GIT_HEAD_OVERRIDE` in a tree without Git and confirming the unavailable wording.

## Suggested articles

- [One-click build and installer scripts](build-scripts.md)
- [Unsigned Squirrel.Windows installer](squirrel-installer.md)
- [Line count in every release](line-count-release.md)
