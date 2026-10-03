# Feature documentation

One article per feature, grouped by category. The platform summaries in `inventory.json`, explicit `surface-inventory.json`, and `capability-inventory.json` are checked against the hand-maintained contract by `scripts/check-feature-inventory.mjs`. Schema validity and current built-product completeness have separate verdicts. Missing current evidence remains incomplete. See [interface completeness](delivery/interface-completion.md) for the receipt format, migration and honest acceptance boundary.

| Category | Index |
| --- | --- |
| Design and appearance | [design/README.md](design/README.md) |
| Search and regex | [search/README.md](search/README.md) |
| Messages, language and voice | [messaging/README.md](messaging/README.md) |
| Records and history | [records/README.md](records/README.md) |
| Navigation | [navigation/README.md](navigation/README.md) |
| Build, install and update | [delivery/README.md](delivery/README.md) |

There is no HTTP API in this project, so no Postman collection applies.
