# Dim sum release code names

Feature id: `release-code-name` · Category: Build, install and update

## Behaviour

`scripts/select-dim-sum.mjs` picks the next unused dish from the public catalog at `Ding-Ding-Projects/dim-sum-photos` in catalog order, proves the photo is a published `catalog-v1*` release asset, and the publish job attaches that photo and writes the dish id, bilingual name and public photo link into the release title and notes. Used ids are read from prior release bodies so no dish repeats.

## Configuration

None; the catalog and photo repository are public.

## Failure modes

If no unused dish with a published photo can be resolved the selector exits non-zero and the release ships with its version alone, saying so.

## Security considerations

No image is generated or stored in this repository.

## Verification

Run the selector locally with `GH_REPO=Ding-Ding-Projects/keepassxc`.

## Suggested articles

- [Dim sum surprise](../messaging/dim-sum-surprise.md)
- [Line count in every release](../delivery/line-count-release.md)
