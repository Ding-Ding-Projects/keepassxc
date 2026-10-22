# KeePassXC shortcut, history and browser confirmation repairs

## Source and requested scope

Implement installer shortcut and local history repairs, plus a persistent option
that fully suppresses website credential-use confirmation. Push to main. No subagents.

Source fix bd2cf151b08e7981f8b28528c7bebfcd5e4184a9 is already on main and independently
verified with git ls-remote. It accepts both supported installation-root names and
disables local signing/hooks for application-owned history commands.

The current addition provides a default-on "Never ask before using credentials on
websites" switch in both settings surfaces. Turning it off asks for each eligible
request. Rejection returns no credentials; approval rechecks the database and entry
eligibility after the dialog. Pairing, blocks, matching, expiration and locking remain.

## Verification

- git diff --check passed.
- Qt uic accepted the browser settings form.
- Independent real-Git fixture: missing signer exit 128; blocking hook exit 1;
  signing/hooks overrides exit 0, with one independently verified commit.
- Native lifecycle, history, browser confirmation and configuration restart tests added.
- First build.bat attempt stopped on inconsistent automatically selected MSVC state.
- A second build with installed MSVC 14.44 reached dependency compilation, then was
  canceled before changing source for the additional browser preference. No native
  test or build pass is claimed.
- Full publication-language scanning stopped on pre-existing design.zip exceeding
  its per-file limit. Changed prose was reviewed separately; full scan is incomplete.

## Remaining verification

Final source delivery is on main through 5341b3b02c87b87cff456a9512e424397de6b5db.
The fresh build attempt used build-browser-consent20261022 and was stopped during
Botan compilation without a KeePassXC build verdict. Its processes were confirmed
absent afterward. Reusing the earlier canceled build-shortcut-history directory
was rejected because it has no completed CMake ownership record. Both directories
are retained. No native test pass, installer or installed update is claimed.

Run the root build and native regression targets, inspect the new control in the
built application, and exercise the paired-browser and installer shortcut workflows.
The user's exact recurring history notification remains unavailable, so its resolution
is unconfirmed. Existing installed version 2.8.32901 does not contain these new fixes.

Preserve the existing release and documentation workflow hold. No release, deployment
or installed update is claimed. Preserve earlier feature-delivery work and unrelated
branches. Tracking and handoff: https://github.com/Ding-Ding-Projects/keepassxc/issues/23.
