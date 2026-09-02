# Social preview embed graphic

Feature id: `social-preview` · Category: Build, install and update

## Behaviour

`social-preview.png` (1280 × 640) lives at the repository root, generated from the real application mark with measured, wrapped text. The documentation site serves a byte-identical copy at `https://ding-ding-projects.github.io/keepassxc/social-preview.png` and its head carries `og:type`, `og:site_name`, `og:title`, `og:description`, `og:url`, an absolute https `og:image` with width, height and alt, `twitter:card` set to `summary_large_image`, and a theme colour, all in the served HTML rather than injected by script.

## Configuration

The Pages workflow (`.github/workflows/pages.yml`) copies the root master into the published site and refuses to deploy unless the two files compare byte-identical.

## Failure modes

GitHub's repository social preview is a settings-page upload with no API; until it is uploaded by hand (Settings → General → Social preview → `social-preview.png` at the root) a shared repository link shows GitHub's generated metadata card instead of the product graphic. Discord caches an image URL aggressively: change the filename when the graphic changes materially.

## Security considerations

Only public branding is embedded; the crawler fetches the image anonymously.

## Verification

Fetch the deployed page and read the tags back; fetch the image URL without credentials and confirm a 200 with `image/png`. Verified 2026-09-01 for `d7e8adba`.

## Suggested articles

- [Line count in every release](line-count-release.md)
- [One-click build and installer scripts](build-scripts.md)
