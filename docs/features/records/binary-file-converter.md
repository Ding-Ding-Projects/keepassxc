# Local binary file converter foundation

The native Converter destination provides explicit source and new-destination
file pickers and four operations: lowercase hexadecimal encode/decode and standard
padded base64 encode/decode. Decode is strict and canonical: whitespace, uppercase
hex, missing padding, invalid alphabet and nonzero unused base64 bits are rejected.
Only user-selected files are read. Known KeePass database signatures are rejected.
The source picker inspects at most 4 KiB and reports a hex-compatible,
base64-compatible or arbitrary-binary prefix without trusting the extension.
Prefix compatibility is not a whole-file type verdict; strict decoding validates
the complete input before output can be published.

Conversion runs on a worker thread in 64 KiB chunks, with cancellation between
chunks. Binary input is bounded to 64 MiB and encoded input to 128 MiB. Destination
capacity is checked conservatively. The source is opened read-only. Staged output
is round-trip checked against the source SHA-256, closed, reopened and checked
again before publication. Windows publication uses a same-directory atomic rename
without replacement; existing destinations, source identity and linked/UNC/alternate
stream paths are rejected. Cancellation and validation failures remove temporary
output without publishing a destination. No file content or path is logged.

Eight categories are listed and searchable with the existing regex builder.
Documents/PDF, Images, Audio, Video, Archives, Structured Data/Spreadsheets and
Code/Text remain disabled because no bundled offline adapter and packaged proof
are present. Binary Encodings is the only implemented category. English, Cantonese
and bilingual control copy use the shared catalogue; the current level variants
retain identical factual wording.

## Verification and limits

Source fixtures cover streaming round trips across chunk boundaries, strict decode
negatives, cancellation, source preservation, existing-output refusal and absence
of output after invalid input. At `3ce199347e48d0f5497dd1646cff73965152c187`, the exact root build and stage validation passed. Vault-filter fixtures reported 11 passed, 0 failed and 0 skipped; voice fixtures reported 10 passed, 0 failed and 0 skipped. Converter fixtures reported 6 passed, 1 failed and 0 skipped: `atomicDestinationAndSource` returned `IoFailure`. This failed fixture remains unresolved by runtime evidence.

Later source repairs `e6f64a7d08ab4c4bfa783c49530cb116c3e06df7` (search visibility and live keyed copy) and `480f9ada0cc1524bb021ebdddf9c6cb5c0399f2d` (publication rename) were independently source-reviewed and preserved in merge `1180d0b25280e8ad716c66637a4eca0e9ba989c9`. They remain uncompiled and unrun. Publication now calls `QTemporaryFile::rename` after the reopened verifier closes, and disables `autoRemove` only after successful rename. The exact native cause of the earlier `IoFailure` remains unproved; source review does not clear that failed fixture. Actual native interaction/capture acceptance remains pending.

This is not universal converter completion. Complete per-format adapter inventory,
PDF operations, isolated decoder processes, durable paged queues, pause/resume,
crash recovery, progress/history/export, full per-category searches and the website
equivalent remain incomplete. Built-in encoding runs in a bounded worker thread,
not an operating-system sandbox. Point-in-time path checks do not eliminate hostile
concurrent path replacement; no adversarial filesystem isolation is claimed.
