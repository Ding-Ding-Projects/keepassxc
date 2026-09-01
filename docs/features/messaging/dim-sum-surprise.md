# Dim sum surprise

Feature id: `dim-sum-surprise` · Category: Messages, language and voice

## Behaviour

At startup the application may show a randomly chosen dim sum dish, named in English and Cantonese, as a non-blocking auto-dismissing card that never gates startup or steals focus (`Material::DimSum`, `src/gui/material/MaterialDimSum.h`). It is suppressed on first run, on error paths, during updates and whenever a capture route is active.

## Configuration

Today `GUI/DimSumSurprise` can turn it off and the draw is one percent.

## Failure modes

The canonical contract is a ten percent draw with no opt-out and dish photos resolved from the public catalog rather than bundled images; both are open inventory findings.

## Security considerations

Bundled assets only; no network.

## Verification

Parity captures suppress the card explicitly; behaviour tests are pending.

## Suggested articles

- [Language modes and the voice catalogue](../messaging/language-modes.md)
- [Dim sum release code names](../delivery/release-code-name.md)
