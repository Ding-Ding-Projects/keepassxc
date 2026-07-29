# Build and Install KeePassXC

This fork targets **Windows only**. The Linux and BSD backends have been removed — there is no
X11 auto-type, no Wayland portal, no D-Bus, no freedesktop.org Secret Service, and no Snap,
Flatpak or AppImage packaging. If you need those, use
[upstream KeePassXC](https://github.com/keepassxreboot/keepassxc).

macOS sources are still present but are not built or tested here.

## Toolchain and build dependencies

These must be on your `PATH`:

* **Visual Studio 2022 or newer**, with the "Desktop development with C++" workload (MSVC toolset
  and the Windows SDK)
* **CMake** >= 3.16
* **Ninja** >= 1.10
* **Qt** 6.8 or newer, `msvc2022_64` — this fork is developed against 6.8.3
* **asciidoctor** >= 2.0, only if you build the offline documentation
  (`-DKPXC_FEATURE_DOCS=ON`, the default). Install with `gem install asciidoctor`.

Everything else — Botan, zlib, minizip, PCSC, zxcvbn — is resolved through **vcpkg** in manifest
mode. Set `VCPKG_ROOT` and the toolchain file below will pull them in on first configure.

## Build steps

Open the **x64 Native Tools Command Prompt for VS 2022** (or any shell where `cl.exe` is on the
path) and run:

```
git clone https://github.com/Ding-Ding-Projects/keepassxc.git
cd keepassxc
cmake -B build -G Ninja ^
      -DCMAKE_BUILD_TYPE=Debug ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 ^
      -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

The binary lands in `build/src/KeePassXC.exe`.

To run it from the build tree, Qt's DLLs must be reachable — either put
`C:\Qt\6.8.3\msvc2022_64\bin` on your `PATH` or run `windeployqt` against the output directory.
The same applies to the test executables in `build/tests`; without it they pop a system-modal
loader error.

### Screenshots and screen capture

KeePassXC calls `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` on its top-level windows, so
every screen-capture API returns black — or worse, the windows *behind* it — even though the app
is plainly visible on your monitor. Launch with `--allow-screencapture` when you need to record
or screenshot it. This is upstream behaviour and it is easy to mistake for a rendering bug.

## Additional CMake parameters

```
-DKPXC_MINIMAL=[ON|OFF]           Minimal feature set required for basic usage (default: OFF)
-DKPXC_FEATURE_BROWSER=[ON|OFF]   Browser integration and passkeys support (default: ON)
-DKPXC_FEATURE_SSHAGENT=[ON|OFF]  SSH Agent integration (default: ON)

-DKPXC_FEATURE_NETWORK=[ON|OFF]   Code that reaches external networks, e.g. icon download (default: ON)
-DKPXC_FEATURE_UPDATES=[ON|OFF]   Automatic update checks; requires networking (default: ON)
-DKPXC_FEATURE_DOCS=[ON|OFF]      Offline documentation; requires asciidoctor (default: ON)

-DWITH_TESTS=[ON|OFF]             Build unit tests (default: ON)
-DWITH_GUI_TESTS=[ON|OFF]         Build GUI tests (default: OFF)
-DWITH_WARN_DEPRECATED=[ON|OFF]   Development only: warn about deprecated methods (default: OFF)
-DWITH_ASAN=[ON|OFF]              Address sanitizer checks (default: OFF)
-DWITH_CCACHE=[ON|OFF]            Use ccache (default: OFF)

-DKEEPASSXC_BUILD_TYPE=[Snapshot|Release]  Show or hide stability warnings (default: "Snapshot")
-DOVERRIDE_VERSION=[X.X.X]        Version number for snapshot builds (default: "")
-DGIT_HEAD_OVERRIDE=[XXXXXXX]     7-digit commit ref, for builds outside a git checkout (default: "")
```

Note: even with all TCP/IP networking disabled, Qt6's network library is still required and
linked — it provides the local named pipes used to talk to the browser extension.

## Packaging

```
cpack -G "ZIP;WIX"
```

## Testing

```
ctest --test-dir build --output-on-failure
```

Useful flags:

```
ctest --test-dir build -j8                    # parallel; note testcli flakes on clipboard timing
ctest --test-dir build -E testgui             # skip the GUI tests
ctest --test-dir build -R testpasskeys -V     # one suite, verbose
```

Kill any running `KeePassXC.exe` or `test*.exe` before rebuilding, or the link step fails with
`LNK1168`.
