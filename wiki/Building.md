# Building

## Requirements

| Component | Version used |
| --- | --- |
| Qt | 6.8.3 (6.6+ supported) |
| CMake | 4.3.4 (3.16+ supported) |
| Ninja | 1.12.0 |
| Compiler | MSVC (Visual Studio 2022 or newer), GCC, or Clang with C++17 |
| asciidoctor | 2.0.26 — only for the offline documentation target |

Native dependencies are declared in [`vcpkg.json`](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/vcpkg.json): Botan 3, minizip, libqrencode, zlib, readline.

## Dependencies

```bash
git clone --depth 1 https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
```

> [!NOTE]
> `vcpkg.json` pins a `builtin-baseline` commit. A `--depth 1` clone does not contain it, and manifest resolution fails with *"failed to `git show` versions/baseline.json"*. Fetch that one commit:
> ```bash
> git -C "$HOME/vcpkg" fetch --depth 1 origin <builtin-baseline>
> ```

## Configure and build

```bash
cmake -B build -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --parallel
```

On Windows, run from a Visual Studio developer shell and point CMake at Qt:

```bat
cmake -B build -S . -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE=%USERPROFILE%/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
```

## Useful options

| Option | Default | Effect |
| --- | --- | --- |
| `KPXC_FEATURE_DOCS` | `ON` | Offline documentation; needs asciidoctor |
| `KPXC_FEATURE_BROWSER` | `ON` | Browser integration **and passkeys** |
| `WITH_TESTS` | `ON` | Build the test suite |

Turn documentation off when asciidoctor is unavailable:

```
-DKPXC_FEATURE_DOCS=OFF
```

## Tests

```bash
cmake --build build --target testpasskeys
./build/tests/testpasskeys -o results.txt,txt
```

On Windows the test executables are built for the GUI subsystem, so stdout is detached — the exit code is the failure count, and `-o <file>,txt` gives the readable report.

See [Passkeys](./Passkeys.md) for what the passkey suite covers.
