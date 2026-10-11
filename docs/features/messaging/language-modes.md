# Language modes and the voice catalogue

Feature id: `language-modes` · Category: Messages, language and voice

## Behaviour

`Material::Voice` (`src/gui/material/MaterialVoice.h`) resolves every user-facing message by id from `share/voice/voice.json`, which holds one line per language per playfulness level. The language is English, Cantonese or bilingual; in bilingual mode a message has an English primary line and a Cantonese secondary line so hosts can render the first prominently and the second compactly. Each catalogue entry declares the facts it must carry; a variant that drops a fact is discarded for a plainer level, so the voice can change and the facts cannot.

## Configuration

`GUI/VoiceLanguage` (English, Cantonese, Bilingual) in the roaming configuration; Appearance › Language and voice.

## Failure modes

Task revision `df94c74b36080f8902bff8edc6a482014ea7d721` includes a bounded shared, user-renamable presentation mode. While restricted, Voice getters and explicit previews return neutral English; stored language and tone preferences remain unchanged for restoration after local credential unlock. Owned settings omit language/tone and personal-vocabulary controls and the classic route. Marked palette actions are omitted, and startup photos are cancelled or suppressed. Corrupt or unwatchable shared state remains restricted and explains its unavailable state on the mode control.

This reader branch does not yet contain `MaterialSchoolMode`, so the reader build cannot establish acceptance of that task-source unit. Other products, all unmarked/direct routes, every already-rendered surface and complete personal-vocabulary suppression remain incomplete. Source implementation is not universal or native acceptance.

## Security considerations

Copy is data in a bundled resource; nothing is fetched.

## Verification

`testmaterialvoice`.

## Suggested articles

- [Funny level, English](../messaging/funny-level-english.md)
- [Funny level, Cantonese](../messaging/funny-level-cantonese.md)
- [Dim sum surprise](../messaging/dim-sum-surprise.md)
