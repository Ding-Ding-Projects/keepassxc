# Language modes and the voice catalogue

Feature id: `language-modes` · Category: Messages, language and voice

## Behaviour

`Material::Voice` (`src/gui/material/MaterialVoice.h`) resolves every user-facing message by id from `share/voice/voice.json`, which holds one line per language per playfulness level. The language is English, Cantonese or bilingual; in bilingual mode a message has an English primary line and a Cantonese secondary line so hosts can render the first prominently and the second compactly. Each catalogue entry declares the facts it must carry; a variant that drops a fact is discarded for a plainer level, so the voice can change and the facts cannot.

## Configuration

`GUI/VoiceLanguage` (English, Cantonese, Bilingual) in the roaming configuration; Appearance › Language and voice.

## Failure modes

School mode, which must force English and hide every Cantonese and dim-sum capability, is not implemented yet and is an open inventory row.

## Security considerations

Copy is data in a bundled resource; nothing is fetched.

## Verification

`testmaterialvoice`.

## Suggested articles

- [Funny level, English](../messaging/funny-level-english.md)
- [Funny level, Cantonese](../messaging/funny-level-cantonese.md)
- [Dim sum surprise](../messaging/dim-sum-surprise.md)
