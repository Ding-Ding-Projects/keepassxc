# Changelog commit links

Feature id: `changelog-commit-links` · Category: Records and history

## Behaviour

Every released entry links the commit its tag points at. The catalog build refuses a version whose tag cannot be resolved, so a dead link cannot ship.

## Configuration

Full tag history is required at configure time; both CI workflows fetch tags.

## Failure modes

CodeQL was red while its checkout lacked tags; that checkout now fetches them.

## Security considerations

None.

## Verification

`testmaterialchangelog` and the CMake provenance guard.

## Suggested articles

- [Changelog viewer](../records/changelog-viewer.md)
- [Line count in every release](../delivery/line-count-release.md)
