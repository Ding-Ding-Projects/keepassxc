# Safe external packaging paths and compiler runtime staging

`scripts/build-squirrel.ps1` accepts checkout-relative or absolute local paths for
`-StageDirectory`, `-ArtifactDirectory`, and `-BuildDirectory`. Defaults remain
`stage/app`, `dist/squirrel-windows`, and `build-windows`. Paths are resolved before
building or changing output. Filesystem roots, checkout ancestors, overlapping
directories, device and network paths, alternate streams, and linked path components
are rejected. An existing build cache must name this exact source checkout.

Use an empty stage for a new source commit or version. A stage can be reused only
when its complete build receipt still matches the same source commit, version,
directory, executable, runtime files, and all staged file hashes. Reusing a warm
build directory does not authorize relabelling an older executable.

## Candidate provenance

The native build writes `stage-provenance.json` in the build directory, outside the
application payload. It binds the full source commit, generated compiled revision,
requested version, x64 executable header, executable version resources, installed
executable hash, and complete staged file inventory. The installed executable must
match the current build output. Source changes during a build or packaging run are
rejected.

`-UseExistingStage` skips compilation only after validating this receipt. The
optional `-StageProvenancePath` selects a receipt explicitly. Missing provenance,
an extra staged file, modified bytes, another commit, or another requested version
stops packaging. A receipt provides build consistency evidence; it is not a code
signature or an authenticity guarantee.

## App-local Microsoft compiler runtime

The selected MSVC x64 compiler determines the permitted Visual Studio installation.
`VCToolsRedistDir`, exported by its `vcvars64.bat`, must remain below that installation's
`VC/Redist/MSVC` directory. The build copies the x64 `Microsoft.VC143.CRT` DLL set
beside `KeePassXC.exe`, including the MSVCP helpers, VCRUNTIME helpers, and concurrency
runtime. Each DLL must have an x64 PE32+ header, match the compiler's major/minor
runtime family, and retain its source hash after copying. DLLs are never collected
from `System32` or an arbitrary `PATH` directory.

This supplies the application's runtime files without installing or changing the
host runtime. Successful staging is not proof of installation in a fresh guest;
the separate disposable installer verification still applies.

## Release output preservation

Packaging and verification run in a unique directory below `stage/squirrel`.
The final output directory must be empty, or carry this checkout's matching
`.keepassxc-output-owner.json` and unchanged hashes for every existing file.
Unexpected files or directories stop publication before deletion.

Publication prepares and hash-checks the replacement files first. Existing verified
outputs are backed up in the unique packaging scratch directory before replacement.
An interrupted replacement restores those prior outputs. Only individually verified
files are removed; the scripts never recursively clear a caller-selected directory.
Scratch candidates and previous-output backups remain available for diagnosis.

## Verification

`tests/TestPackagingSafety.ps1` checks external and default paths, overlap and ancestor
refusal, junction containment, output ownership, modified-file preservation, injected
copy and move failures, silent copy corruption, warm-cache ownership, and PE headers.
Supplying `-CompilerPath` and `-RedistDirectory` additionally verifies the real pinned
runtime copy. `-StageExePath` and `-StageCommit` exercise executable provenance with
a copied test fixture, including version, commit, extra-file, and path-traversal
rejection. These fixture receipts never serve as production build receipts.
