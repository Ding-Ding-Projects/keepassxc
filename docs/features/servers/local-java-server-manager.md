# Owned local Java server manager

## Implemented source scope

The Minecraft servers destination provides an owned inventory, typed creation fields, explicit edition/distribution/target selectors, memory and port steppers, file pickers, explicit EULA consent, version discovery and local start/stop/restart actions. Only official vanilla Java with a local Windows process is implemented. Local Docker, SSH process and SSH Docker remain visible with unavailable reasons. Bedrock installation remains unavailable.

Refresh obtains the complete official Java manifest from `https://piston-meta.mojang.com/mc/game/version_manifest_v2.json`, including returned historical and snapshot entries. Selecting a version obtains its metadata and verifies the manifest SHA-1. Metadata requests allow fixed official HTTPS hosts, reject redirects and embedded credentials, limit responses to 4 MiB and time out after 15 seconds. Cancel invalidates pending generations. Offline responses retain the existing choices with an explicit status; no catalogue is persisted in this unit.

A version is eligible only when metadata provides a server digest/size and a Java major version. Missing historical server/runtime fields are unavailable, not guessed. The user selects a Java executable and already downloaded server JAR. Creation verifies the JAR size and SHA-1 against official metadata, copies from its verified handle, rechecks copied bytes, and creates a new UUID-owned directory. It writes explicit accepted EULA and selected server port. It never adopts existing worlds or directories. Failed partial creation retains its owned directory for recovery.

The manager stores a versioned inventory below application-local data. Existing unknown document/record fields are preserved. A malformed inventory blocks creation rather than replacing it. Managed directories and JARs must not be symbolic links. Start revalidates the JAR digest and runtime path and executes an argument list through QProcess. Only processes created by this manager are controlled. Started means an owned process/PID exists, not game readiness. Stop sends the stop protocol line to that exact process; restart waits for exit. Closing the manager requests graceful stop; QProcess destruction ends remaining owned children. No unrelated process is discovered or adopted. Logs are discarded in this bounded unit.

## Limits and incomplete coverage

This is a first implementation unit, not a complete universal server manager. Runtime major-version compatibility, executable provenance/digest, port availability, readiness, capacity and native lifecycle acceptance remain unverified. Creation performs bounded local hashing/copying synchronously. Runtime provisioning, distribution download, transactional rollback of partial creation, version/channel filtering, searchable selectors, per-version property descriptors, Bedrock/history, container/SSH adapters, mods/plugins/packs, worlds, backups, users/permissions, schedules, management APIs, exports and full GUI contracts remain incomplete. The memory range is a manager safety limit, not a claim about every version's supported resources. Unknown inventory fields survive serialization; existing server configuration is not adopted or edited.

English/Cantonese/bilingual voice keys are supplied separately for integration. Navigation labels and form refresh on language changes remain incomplete. Guidance, advanced appearance, history, bulk actions, anchored regex filtering and server-specific status reporting need follow-up. No real server, metadata request, license acceptance or process launch was exercised during implementation.

## Focused verification

`testmaterialservermanager` covers the four-target inventory, negative omissions, name validation, allowed metadata origins and unknown-field preservation. Build and execution are pending. After the exact root build, run `ctest --test-dir build-windows --output-on-failure --timeout 180 -R "^testmaterialservermanager$"`. Real creation/lifecycle and the viewport/language/theme/scale matrix require disposable owned servers and source/hash-bound captures.
