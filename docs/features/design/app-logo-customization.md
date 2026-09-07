# Application logo customization

The Material Settings screen includes an **Application logo** card. It keeps the shipped KeePassXC mark by default and lets a person choose a local PNG or JPEG as a presentation-only replacement. The change applies immediately to the application and its open top-level windows. It does not alter the executable, installer, application identifier, update feed, data location, signing state, or ordinary action icons.

## Local-only conversion boundary

The picker accepts only actual PNG and JPEG bytes. Its validator checks the decoded format instead of the filename, rejects unreadable files, images larger than 5 MiB, dimensions larger than 4096 pixels, images above 16 megapixels, and animated input. The accepted pixels are normalized to PNG under the application's private app-data `logos/` cache. The cache refuses symbolic links and Windows reparse-point directories, verifies canonical containment before writing or removing files, and never follows a linked `logos/` directory. The selected source pathname is never stored in configuration, history, exports, telemetry, logs, screenshots, prompts, or project records.

The card stores only three local presentation settings: whether a custom mark is active, `fit` or `crop`, and the background colour. A normalized private source image permits a fit, crop, or background change to regenerate the display derivative without reopening the original. The source and display files are staged separately, then activated with checked rollback before the enabled state or presentation settings are committed. A conversion failure leaves the existing valid mark and settings in place. Reset confirms both private files are gone before declaring success. If the second removal fails after the first completed, it disables the custom presentation, reports a failed partial reset, retains the residual private file under a cleanup-only name, and allows a later reset retry to remove it.

## Accessible controls

The card supplies an accessible live preview and status text, a searchable fit-mode selector with the adjacent regex-builder integration provided by `Material::Select`, a background-colour control, a keyboard-operable local chooser, and a reset action. The picker truthfully limits its filter to PNG and JPEG. SVG, animated formats, malformed data, files with false extensions, and oversized inputs are refused rather than silently converted.

## Verification

`TestApplicationLogo` creates neutral generated fixtures only. It covers validated import, stored-path privacy, malformed and oversized rejection, second-write fault injection with prior-state retention, staged presentation-setting rollback, reset failure retention, linked-cache refusal without external-target writes, fit/background regeneration, persistence settings, reset, and cache removal. The focused target requires this registration in `tests/CMakeLists.txt`:

```cmake
add_unit_test(NAME testapplicationlogo SOURCES TestApplicationLogo.cpp
        LIBS ${TEST_LIBRARIES})
```

It should use the existing offscreen Qt setup before a built-interface capture confirms the selected mark at supported display scales.
