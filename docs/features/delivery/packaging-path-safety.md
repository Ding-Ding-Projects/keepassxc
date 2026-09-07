# Safe external packaging paths and compiler runtime staging

`scripts/build-squirrel.ps1` accepts checkout-relative or absolute local paths for
`-StageDirectory`, `-ArtifactDirectory`, and `-BuildDirectory`. Defaults remain
`stage/app`, `dist/squirrel-windows`, and `build-windows`. Paths are resolved before
building or changing output. Filesystem roots, checkout ancestors, overlapping
directories, device and network paths, alternate streams, and linked path components
are rejected. An existing build cache must name this exact source checkout.

An existing stage must carry a valid ownership and complete-file receipt. A new
commit or version is installed into a fresh same-volume candidate directory,
including runtime files and its new receipt. Only a completely validated candidate
replaces the existing stage. Reusing a warm build directory does not authorize
relabelling an older executable.

## Candidate provenance

The native build writes `.keepassxc-stage-provenance.json` inside the stage so the
application and its receipt move as one directory generation. NuGet explicitly
excludes this private receipt, and the package verifier rejects its presence. The
receipt binds the full source commit, generated compiled revision,
requested version, x64 executable header, executable version resources, installed
executable hash, and complete staged file inventory. The installed executable must
match the current build output. Source changes during a build or packaging run are
rejected.

`-UseExistingStage` skips compilation only after validating this receipt against the
current commit and requested version. The
optional `-StageProvenancePath` selects a receipt explicitly. Missing provenance,
an extra staged file, modified bytes, another commit, or another requested version
stops packaging. A receipt provides build consistency evidence; it is not a code
signature or an authenticity guarantee.

The package verifier also hashes the executable and every declared compiler-runtime
entry inside the full package. Matching version resources alone cannot accept older
or different executable bytes, and a package missing a runtime DLL is rejected.

## App-local Microsoft compiler runtime

The selected MSVC x64 compiler determines the permitted Visual Studio installation.
The builder always activates that exact toolset's `vcvars64.bat` in a fresh child
process before configuration and rechecks the selected compiler. Inherited compiler
initialization markers are reset only inside that child, and its resulting environment
is imported into the current build process. Validation includes the selected Windows
SDK identity, its required include and x64 library paths, and essential headers and
libraries. This activation does not install tools or change user or machine settings.
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

Stage and release publication use the same directory transaction. Candidates and
previous generations are named siblings of the destination, ensuring same-volume
renames. A complete, flushed journal is atomically published before the first
rename. A directory-scoped mutex serializes cooperating publishers. The old
generation is renamed to its backup, then the verified candidate becomes the
destination. Application files and the canonical stage receipt are never updated
independently.

A reported operation failure restores the old generation. After abrupt process
termination, the next invocation recovers the journal before ownership validation:
it completes a verified candidate or restores the verified previous generation if
the candidate is unavailable. Unexpected or modified contents fail closed and are
preserved. Successful recovery retires the journal automatically. Previous
generations and failed candidates remain available for diagnosis; no directory is
recursively cleared.

## Verification

`tests/TestPackagingSafety.ps1` checks external and default paths, overlap and ancestor
refusal, junction containment, output ownership, modified-file preservation, injected
copy and rename failures, silent copy corruption, warm-cache ownership, and PE headers.
Real subprocess interruption tests exit after the first destructive rename and prove
that a fresh invocation recovers release output and a stage together with its receipt.
Injected install, runtime, and receipt failures prove the previous stage is unchanged.
Supplying `-CompilerPath` and `-RedistDirectory` additionally verifies the real pinned
runtime copy. `-StageExePath` and `-StageCommit` exercise executable provenance with
a copied test fixture, including version, commit, extra-file, and path-traversal
rejection. These fixture receipts never serve as production build receipts.
