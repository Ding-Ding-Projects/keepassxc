# Window-chrome capture promotion gap

The existing diagnostic capture from source
`2b2a683ba16d9a5dc116808100c2e8418f4fa681` was reviewed for promotion. It was
**not promoted**, and no image was copied into this directory or embedded as
accepted current evidence. The [assessment record](promotion-assessment.json)
contains the verified image identity and explicit missing evidence.

The raw PNG is 1800 by 1200 pixels, 85,009 bytes, with SHA-256
`0b7b4161aaa6e1b419d326ccb2959365e2c88987531ddabd73464ac5b0c8bc07`.
It was captured from the isolated English/light welcome state at 150% scale.
The executable SHA-256 is
`f3f8fdc487e2ee43b9d5697ca7486f8b54ecde13a2f9d90b2769ae17da51b491`.

## Why it remains private diagnostic evidence

- The existing capture record has no version-1 or version-2 promotion schema.
  Validation against the unchanged producing source exits 1 with
  `UI evidence rejected: version must be 1 or 2`.
- It lacks producer-recorded capture start/completion timestamps. The launch
  timestamp is not the capture timestamp; filenames and filesystem times cannot
  supply missing provenance.
- The recorded route is `direct-cheap-runtime-binding`. It must retain its actual
  identity rather than being relabelled to satisfy a different receipt arm.
- No hashed privacy-scan receipt or exact inventory-row binding accompanies the
  capture. The existing `shell-default` parity row is a different 1440 by 920,
  100% tuple and cannot be reused for this image.
- Visual inspection shows a black strip along the lower boundary. The title also
  contains a synthetic verification-profile identifier. A complete publication
  review and a fully rendered replacement capture are still required.

The image bytes were not edited, recompressed, renamed into accepted evidence or
added to version control. No large binary transfer was attempted. This follow-up
ran no build and launched no application. The intermittent dragging report remains
unresolved, independently of this promotion gap.

## Next capture requirements

An authorized future capture run must record the real source-start/source-end
binding, build receipt hash, exact capture timestamps, actual supported route,
surface tuple, inspection/privacy evidence and owned teardown. It must preserve
the raw image unchanged, show the complete surface, and receive an exact new
inventory row for this tuple. Only after those checks pass may documentation embed
the image, explicitly as diagnostic evidence rather than proof that dragging is
fixed.
