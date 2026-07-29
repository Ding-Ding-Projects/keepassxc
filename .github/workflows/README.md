# GitHub Actions workflows

What lives in this directory and what each file is responsible for.

> **This fork is Windows-only.** The Material Design 3 interface targets Windows and nothing
> else, so CI builds, tests and releases Windows alone. There is no Linux or macOS job and none
> is planned. The Linux backends — fdosecrets, the X11 and Wayland auto-type plugins, nixutils,
> PolKit, the D-Bus adaptor and the Snap/Flatpak/AppImage paths — have been **removed from the
> tree**, so there is nothing left for a Linux job to build.

| Workflow | File | Origin | Triggers | What it does |
| --- | --- | --- | --- | --- |
| **Material CI and Release** | [`material-release.yml`](material-release.yml) | This fork | Push to any branch, manual dispatch | Builds and tests on Windows, then publishes one uniquely tagged GitHub Release per run |
| **CodeQL** | [`codeql.yml`](codeql.yml) | Upstream KeePassXC | Push to `develop` and `release/**`, every pull request, weekly cron | Static security and quality analysis of the C++ sources, reported to the repository's code scanning alerts |
| **Copilot Setup Steps** | [`copilot-setup-steps.yml`](copilot-setup-steps.yml) | Upstream KeePassXC | Manual dispatch, and push/PR that touches the file itself | Declares the toolchain that GitHub Copilot coding agents get pre-installed in their sandbox; it only installs packages, it does not build the project |

Only `material-release.yml` was added by this fork. The other two are upstream files kept as
they are so the `pull.yml` rebase from `keepassxreboot/keepassxc` stays conflict-free. They still
mention Linux runners; that is upstream's business and is deliberately not edited here.

## Material CI and Release

### Shape

```
push to any branch  ─┐
workflow_dispatch   ─┴─▶  test-windows  ──▶  release
```

`release` declares `needs: [test-windows]` and an explicit `success()` in its condition, so a
failing, cancelled, or skipped test job publishes nothing at all. It runs only on the repository's
default branch or on a manual dispatch; pushes to feature branches build and test but never
release.

### What the test job does

`test-windows` installs Qt 6.8.3 with `jurplel/install-qt-action`, CMake and Ninja with
`lukka/get-cmake`, configures with `-DCMAKE_BUILD_TYPE=Release -DKPXC_FEATURE_DOCS=OFF
-DWITH_TESTS=ON`, builds, runs the full `ctest` suite, and uploads the Windows package.

`KPXC_FEATURE_DOCS` is off because the offline documentation needs asciidoctor, a Ruby toolchain
that is not worth installing on a runner just to render help pages. Everything else, including the
browser integration, stays at its default.

The build uses MSVC and Ninja. The MSVC environment is exported by locating the toolchain with
`vswhere` and dumping `vcvars64.bat` into `$GITHUB_ENV`, so no extra action is needed for it.
Native dependencies come from vcpkg, exactly as `README.md` documents for a local build. Test
executables are GUI-subsystem binaries on Windows, so their stdout is detached and QTest output
does not reach the log; the job relies on ctest exit codes and passes `--output-on-failure`.

### The vcpkg baseline trap

`vcpkg.json` pins `builtin-baseline` to an exact vcpkg commit. While resolving the manifest, vcpkg
runs `git show <builtin-baseline>:versions/baseline.json` inside its own checkout. A
`git clone --depth 1` only fetches the tip of `master`, so that commit object is not present and
the run fails with:

```
error: failed to git show versions/baseline.json
```

The workflow avoids this by reading the baseline out of `vcpkg.json` and fetching that exact SHA
(`git fetch --depth 1 origin <baseline>`), falling back to a full unshallowed fetch if the server
refuses a by-SHA fetch. It then asserts the object is readable before configuring. The vcpkg
checkout and its binary cache are cached with `actions/cache` under a key that includes
`hashFiles('vcpkg.json')`.

vcpkg is cloned into `.ci-vcpkg/`, **not** into `vcpkg/`. The repository already tracks a `vcpkg/`
directory holding the overlay triplets that `vcpkg-configuration.json` points at; cloning the tool
over it would destroy them.

### Action versions

GitHub's hosted runners warn about, and will eventually refuse to run, actions built on Node 20.
The first-party actions are therefore pinned to their current majors, all of which run on Node 24:
`actions/checkout@v7`, `actions/cache@v6`, `actions/upload-artifact@v7` and
`actions/download-artifact@v8`. `jurplel/install-qt-action@v4` is a composite action and brings no
Node runtime of its own, and `lukka/get-cmake@latest` tracks its own newest release.

### Release

Each run publishes a non-draft, non-prerelease release tagged `v0.0.<run_number>.<run_attempt>`,
targeted at the pushed commit. `run_number` increases with every run of this workflow and
`run_attempt` with every re-run, so the tag is unique and monotonic and no earlier release is ever
recycled or overwritten. The release carries the one package the test job actually built:

- `KeePassXC-Material-windows-x64.zip` — the `cmake --install` tree, which the project's own
  windeployqt install rule populates with the Qt runtime, plugins and CA bundle.

The release is created with `gh release create` using the built-in `GITHUB_TOKEN`, granted through
a job-scoped `permissions: contents: write` block. No organization or repository secret is
referenced by name and the token is never echoed.

The `release` job itself runs on `ubuntu-24.04`. That is not leftover Linux support: the job never
compiles anything, it downloads an artifact and shells out to `gh`, and a Linux runner bills at a
fifth of a Windows one. Nothing Linux is built, tested or shipped.

### No automation loop

Nothing in the workflow pushes back to the repository. The only ref it creates is the release tag,
and the `push` trigger is filtered to `branches: ['**']`, which tag pushes do not match — so a
release cannot re-trigger the workflow.
