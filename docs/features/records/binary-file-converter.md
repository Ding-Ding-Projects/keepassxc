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
again before publication. Windows publication uses a same-directory atomic move
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
of output after invalid input. Compilation, fixture execution and actual native
interaction/capture acceptance remain pending.

This is not universal converter completion. Complete per-format adapter inventory,
PDF operations, isolated decoder processes, durable paged queues, pause/resume,
crash recovery, progress/history/export, full per-category searches and the website
equivalent remain incomplete. Built-in encoding runs in a bounded worker thread,
not an operating-system sandbox. Point-in-time path checks do not eliminate hostile
concurrent path replacement; no adversarial filesystem isolation is claimed.
