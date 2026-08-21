#!/usr/bin/env node
import fs from 'node:fs';
import { execFileSync } from 'node:child_process';

const checkOnly = process.argv.includes('--check');
const changelog = fs.readFileSync('CHANGELOG.md', 'utf8');
const outputPath = 'share/changelog/provenance.json';
const origin = execFileSync('git', ['remote', 'get-url', 'origin'], { encoding: 'utf8' }).trim()
  .replace(/^git@github[.]com:/, 'https://github.com/').replace(/[.]git$/, '');
const records = [];
for (const match of changelog.matchAll(/^##\s+(.+?)\s+\(([^()]*)\)\s*$/gm)) {
  const version = match[1].trim();
  const date = match[2].trim();
  if (date.toLowerCase() === 'pending') {
    records.push({ version, released: false, reason: 'Pending entry has no immutable release tag.' });
    continue;
  }
  const prereleaseTag = version.replace(/\s+(Alpha|Beta|RC)\s*(\d+)$/i, (_, phase, number) => `-${phase.toLowerCase()}${number}`);
  const candidates = [version, prereleaseTag !== version ? prereleaseTag : null, /^\d+[.]\d+$/.test(version) ? `${version}.0` : null].filter(Boolean);
  let tag;
  let sha;
  for (const candidate of candidates) {
    try {
      const resolved = execFileSync('git', ['rev-list', '-n', '1', `refs/tags/${candidate}`], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim();
      if (resolved) { tag = candidate; sha = resolved; break; }
    } catch { /* try the normalized x.y.0 tag */ }
  }
  if (!tag) throw new Error(`Released changelog version ${version} has no resolvable exact or x.y.0 tag`);
  if (!/^[0-9a-f]{40}$/.test(sha)) throw new Error(`Tag ${tag} did not resolve to a full commit SHA`);
  execFileSync('git', ['cat-file', '-e', `${sha}^{commit}`]);
  records.push({ version, released: true, tag, date, sha, url: `${origin}/commit/${sha}` });
}
const rendered = `${JSON.stringify({ schemaVersion: 1, origin, records }, null, 2)}\n`;
if (checkOnly) {
  if (!fs.existsSync(outputPath) || fs.readFileSync(outputPath, 'utf8') !== rendered) {
    throw new Error(`${outputPath} is missing or stale; run node scripts/generate-changelog-provenance.mjs`);
  }
  process.stdout.write(`PASS: ${records.length} changelog provenance records are exact.\n`);
} else {
  fs.writeFileSync(outputPath, rendered);
  process.stdout.write(`Wrote ${records.length} records to ${outputPath}.\n`);
}
