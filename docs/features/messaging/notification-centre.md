# Notification centre

Feature id: `notification-centre` · Category: Messages, language and voice

## Behaviour

The bell in the app bar opens `Material::NotificationCentre` (`src/gui/material/MaterialNotificationCentre.h`), which keeps dismissed notifications reviewable with their category, title, body and actions, and carries its own search bar wired to the regex builder.

## Configuration

History length is bounded by the notifier's history limit.

## Failure modes

Bulk actions and export on the centre are open inventory rows.

## Security considerations

The centre stores only what was shown; nothing is sent anywhere.

## Verification

Covered by the shell responsive suite and the search registry tests.

## Suggested articles

- [Non-blocking notifications](../messaging/notifications.md)
- bulk-actions (not implemented yet; see `docs/features/inventory.json`)
- export-everything (not implemented yet; see `docs/features/inventory.json`)
