# External editor

Feature id: `external-editor` · Category: Navigation

## Behaviour

The vault offers an action to open the database folder in an external editor, detecting installed editors and reporting clearly when none is found.

## Configuration

The chosen editor is persisted in configuration.

## Failure modes

Opening exports directly in Visual Studio Code is an open inventory row.

## Security considerations

Only the folder path is handed to the editor process.

## Verification

Manual; a focused test is pending.

## Suggested articles

- vscode-handoff (not implemented yet; see `docs/features/inventory.json`)
- export-everything (not implemented yet; see `docs/features/inventory.json`)
