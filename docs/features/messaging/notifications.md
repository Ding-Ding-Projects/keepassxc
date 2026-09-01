# Non-blocking notifications

Feature id: `notifications` · Category: Messages, language and voice

## Behaviour

Informational, success, progress and non-decision error messages are snackbars from `Material::Notify` (`src/gui/material/MaterialNotifier.h`) hosted over the shell content, never modal dialogs. Errors and warnings persist until dismissed; other toasts auto-dismiss. Progress cards are keyed by an operation id so a long operation updates one card and dismisses it when the percentage drops below zero. Long operations in the main window report through this route since the legacy status bar was hidden.

## Configuration

Timeout and history limits are configuration keys on the notifier.

## Failure modes

Some upstream dialogs still use modal message boxes for informational text; each remaining one is an inventory finding.

## Security considerations

Notification text is subject to the funny level but keeps its facts at every level.

## Verification

Notification behaviour is exercised in the responsive shell tests; every dismissed notification stays in the centre.

## Suggested articles

- [Notification centre](../messaging/notification-centre.md)
- [Language modes and the voice catalogue](../messaging/language-modes.md)
- [Funny level, English](../messaging/funny-level-english.md)
