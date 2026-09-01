#!/usr/bin/env node
// Select the next unused dim sum code name for a release from the public
// catalog at Ding-Ding-Projects/dim-sum-photos.
//
//   node scripts/select-dim-sum.mjs [--used-from-releases] [--out dim-sum.json]
//
// Dishes are taken in catalog order. A dish is used once per project: the ids
// already taken are read from this repository's existing release bodies, which
// each record "Code name: ... (`hk-dish-NNNN`". Only a dish whose photo is
// published as a `catalog-v1*` release asset is eligible, proven with a HEAD
// request. Nothing is generated or stored locally; the photo stays public.
import { spawnSync } from 'node:child_process';
import { writeFileSync } from 'node:fs';

const CATALOG_URL = 'https://raw.githubusercontent.com/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json';
const PHOTO_REPO = 'Ding-Ding-Projects/dim-sum-photos';

const args = new Map();
for (let i = 2; i < process.argv.length; i += 1) {
  const key = process.argv[i];
  if (!key.startsWith('--')) continue;
  const next = process.argv[i + 1];
  args.set(key.slice(2), next && !next.startsWith('--') ? process.argv[++i] : 'true');
}

function gh(argv) {
  const result = spawnSync('gh', argv, { encoding: 'utf8', maxBuffer: 1 << 26 });
  if (result.status !== 0) throw new Error(`gh ${argv.join(' ')} failed: ${result.stderr}`);
  return result.stdout;
}

async function usedIds() {
  if (args.get('used-from-releases') !== 'true') return new Set();
  const repo = process.env.GH_REPO;
  const raw = gh(['release', 'list', '--limit', '1000', '--json', 'tagName', ...(repo ? ['--repo', repo] : [])]);
  const tags = JSON.parse(raw).map(r => r.tagName);
  const used = new Set();
  for (const tag of tags) {
    const body = gh(['release', 'view', tag, '--json', 'body', '--jq', '.body', ...(repo ? ['--repo', repo] : [])]);
    for (const match of body.matchAll(/hk-dish-(\d{4})/g)) used.add(`hk-dish-${match[1]}`);
  }
  return used;
}

async function photoAssets() {
  // Every catalog-v1* release's asset names, so eligibility is proven from the
  // published inventory rather than guessed from a filename pattern.
  const releases = JSON.parse(gh(['release', 'list', '--repo', PHOTO_REPO, '--limit', '200', '--json', 'tagName']));
  const assets = new Map();
  for (const { tagName } of releases) {
    if (!tagName.startsWith('catalog-v1')) continue;
    const list = JSON.parse(gh(['release', 'view', tagName, '--repo', PHOTO_REPO, '--json', 'assets', '--jq', '[.assets[] | {name, url}]']));
    for (const asset of list) if (!assets.has(asset.name)) assets.set(asset.name, { tag: tagName, url: asset.url });
  }
  return assets;
}

const response = await fetch(CATALOG_URL);
if (!response.ok) throw new Error(`catalog fetch answered ${response.status}`);
const catalog = await response.json();
const used = await usedIds();
const assets = await photoAssets();
let chosen = null;
for (const dish of catalog.dishes) {
  if (used.has(dish.id)) continue;
  const assetName = `${dish.id}-${dish.slug}.png`;
  const asset = assets.get(assetName);
  if (!asset) continue;
  const head = await fetch(asset.url, { method: 'HEAD', redirect: 'follow' });
  if (!head.ok) continue;
  chosen = { id: dish.id, slug: dish.slug, nameEn: dish.name.en, nameZh: dish.name.zhHant, jyutping: dish.jyutping, assetName, assetUrl: asset.url, catalogRelease: asset.tag, catalogUrl: CATALOG_URL, usedBefore: used.size };
  break;
}
if (!chosen) {
  console.error('No unused dish with a published photo could be resolved; ship the release with its version alone and say so.');
  process.exit(3);
}
const out = args.get('out');
if (out) writeFileSync(out, JSON.stringify(chosen, null, 2) + '\n');
console.log(JSON.stringify(chosen, null, 2));
