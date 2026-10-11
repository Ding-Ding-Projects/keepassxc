# Documentation language implementation reference

The reader preserves one article route while switching between the full English source, a complete Cantonese translation, and both complete texts in vertically stacked language-labelled sections. Stacking avoids squeezing two full articles into narrow side-by-side columns. The shared outline pairs section titles; identifiers are unique across both sections.

The checked-in translation inventory explicitly includes every embedded feature document and wiki page. Source hashes bind each translation decision to exact normalized source bytes. Missing and stale are distinct: missing remains visibly incomplete; stale prevents building. No title-only translation or silent English fallback counts as a translated article.

Implementation references are `site/documentation.js`, `site/documentation-languages.mjs`, `site/build-documentation.mjs`, and `site/translations/documentation-yue.json`. The behavior and acceptance limits are documented in `docs/features/delivery/documentation-reader.md`.

This is a source implementation reference, not a rendered design or visual acceptance receipt. Current source-bound browser captures, keyboard testing, accessibility review and the complete language/theme/scale/viewport matrix remain pending. Existing English-body captures belong to the earlier implementation.
