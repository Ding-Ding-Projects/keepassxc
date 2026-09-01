// Download the pinned runtime and font assets the checked-in references load
// from public origins, so the reference renderer can serve them locally and a
// capture is deterministic and offline. Every file is recorded in
// design/lib/vendor/manifest.json with its SHA-256; a later run verifies the
// recorded hashes and re-downloads only what is missing or changed.
//
//   node design/parity/vendor-reference-assets.mjs          # download + verify
//   node design/parity/vendor-reference-assets.mjs --check  # verify only
import { createHash } from 'node:crypto';
import { mkdir, readFile, writeFile, access } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const vendorRoot = join(here, '..', 'lib', 'vendor');
const manifestPath = join(vendorRoot, 'manifest.json');
const checkOnly = process.argv.includes('--check');

// The exact URLs the reference files name. support.js pins the runtimes;
// lib/kpxc.css imports one Google Fonts stylesheet.
const scripts = [
  ['react.production.min.js', 'https://unpkg.com/react@18.3.1/umd/react.production.min.js'],
  ['react-dom.production.min.js', 'https://unpkg.com/react-dom@18.3.1/umd/react-dom.production.min.js'],
  ['babel.min.js', 'https://unpkg.com/@babel/standalone@7.29.0/babel.min.js']
];
const fontCss = 'https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&family=Roboto+Mono:wght@400;500&family=Noto+Sans+HK:wght@400;500;700&family=Material+Symbols+Rounded:opsz,wght,FILL,GRAD@20..48,100..700,0..1,-50..200&display=swap';
// A modern browser UA makes Google Fonts answer with woff2 and unicode-range subsets.
const userAgent = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36';

const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const exists = async path => access(path).then(() => true, () => false);

async function fetchBytes(url) {
  const response = await fetch(url, { headers: { 'user-agent': userAgent }, redirect: 'error' });
  if (!response.ok) throw new Error(`${url} answered ${response.status}`);
  return Buffer.from(await response.arrayBuffer());
}

async function loadManifest() {
  try { return JSON.parse(await readFile(manifestPath, 'utf8')); } catch { return { schemaVersion: 1, files: {} }; }
}

async function verify(manifest) {
  const failures = [];
  for (const [name, entry] of Object.entries(manifest.files)) {
    const path = join(vendorRoot, name);
    if (!(await exists(path))) { failures.push(`${name}: missing`); continue; }
    const actual = sha256(await readFile(path));
    if (actual !== entry.sha256) failures.push(`${name}: sha256 ${actual} != recorded ${entry.sha256}`);
  }
  return failures;
}

async function main() {
  const manifest = await loadManifest();
  if (checkOnly) {
    const failures = await verify(manifest);
    if (!Object.keys(manifest.files).length) failures.push('manifest is empty');
    if (failures.length) { console.error(failures.join('\n')); process.exit(1); }
    console.log(`Verified ${Object.keys(manifest.files).length} vendored reference assets.`);
    return;
  }
  await mkdir(join(vendorRoot, 'fonts'), { recursive: true });
  const files = {};
  for (const [name, url] of scripts) {
    const bytes = await fetchBytes(url);
    await writeFile(join(vendorRoot, name), bytes);
    files[name] = { url, sha256: sha256(bytes), bytes: bytes.length };
  }
  // The stylesheet is rewritten so every url(...) points at the vendored file,
  // while font-weight, font-style and unicode-range stay exactly as served.
  let css = (await fetchBytes(fontCss)).toString('utf8');
  const fontUrls = [...new Set([...css.matchAll(/url\((https:\/\/fonts\.gstatic\.com\/[^)]+)\)/g)].map(m => m[1]))];
  let index = 0;
  for (const url of fontUrls) {
    const bytes = await fetchBytes(url);
    const ext = url.split('.').pop().split('?')[0];
    const local = `fonts/${String(index++).padStart(3, '0')}-${sha256(bytes).slice(0, 12)}.${ext}`;
    await writeFile(join(vendorRoot, local), bytes);
    files[local] = { url, sha256: sha256(bytes), bytes: bytes.length };
    css = css.split(url).join(`/design/lib/vendor/${local}`);
  }
  const cssBytes = Buffer.from(css, 'utf8');
  await writeFile(join(vendorRoot, 'fonts.css'), cssBytes);
  files['fonts.css'] = { url: fontCss, sha256: sha256(cssBytes), bytes: cssBytes.length, faces: (css.match(/@font-face/g) || []).length };
  const next = { schemaVersion: 1, fetchedAt: new Date().toISOString(), userAgent, files };
  await writeFile(manifestPath, JSON.stringify(next, null, 2) + '\n');
  const failures = await verify(next);
  if (failures.length) { console.error(failures.join('\n')); process.exit(1); }
  console.log(`Vendored ${Object.keys(files).length} files; ${files['fonts.css'].faces} @font-face blocks.`);
}

main().catch(error => { console.error(error.message); process.exit(1); });
