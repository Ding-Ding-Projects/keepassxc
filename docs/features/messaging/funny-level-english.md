# Funny level, English

Feature id: `funny-level-english` · Category: Messages, language and voice

## Behaviour

An independent 1 to 5 slider styles every English message from fully serious to maximum playfulness. Level 1 is the upstream wording and the floor every fallback chain ends at. The level applies to every category, including destructive, security and error copy; the facts a message carries are enforced at resolve time.

## Configuration

`GUI/FunnyLevelEnglish`, persisted; Appearance › Voice, with a live sample beneath the slider.

## Failure modes

The canonical default is level 5; the shipped default is 3, an open inventory finding. The first-run disclosure exists as `GUI/VoiceDisclosureShown`.

## Security considerations

None beyond the fact enforcement.

## Verification

`testmaterialvoice` resolves catalogue entries at every level and rejects a variant that drops a fact.

## Suggested articles

- [Funny level, Cantonese](../messaging/funny-level-cantonese.md)
- [Language modes and the voice catalogue](../messaging/language-modes.md)
