import assert from 'node:assert/strict';
import { buildReleaseData, parseReleaseJson } from './site-release-data.mjs';
const root = 'https://github.com/Ding-Ding-Projects/keepassxc/releases/';
const base = `${root}download/v2.8.1/`;
const names = ['Setup.exe', 'RELEASES', 'KeePassXC.Material-2.8.1-full.nupkg', 'update-manifest-v1.json', 'build-provenance.json'];
const release = { isDraft: false, isPrerelease: false, tagName: 'v2.8.1', assets: names.map(name => ({ name, url: base + name, size: 50 })) };
const provenance = { schemaVersion: 1, version: '2.8.1', packageId: 'KeePassXC.Material', architecture: 'x64', sourceCommit: 'a'.repeat(40), generatedAtUtc: '2026-09-04T00:54:48Z', stagedExecutable: {sha256: 'b'.repeat(64), path: 'unpublished-build-path'} };
const manifest = { schemaVersion: 1, version: '2.8.1', packageId: 'KeePassXC.Material', architecture: 'x64', sha256: 'c'.repeat(64), executableSha256: 'b'.repeat(64), bytes: 50, packageFile: names[2], packageUrl: base + names[2], notesUrl: root + 'tag/v2.8.1' };
const result = buildReleaseData(release, provenance, manifest);
assert.equal(result.version, '2.8.1');
assert.equal(result.updatedAtUtc, provenance.generatedAtUtc);
assert.equal(result.unsigned, true);
assert.ok(!JSON.stringify(result).includes('unpublished-build-path'));
assert.deepEqual(parseReleaseJson(Buffer.from('\uFEFF{"schemaVersion":1}')), {schemaVersion: 1});
assert.throws(() => parseReleaseJson(Buffer.alloc(1024 * 1024 + 1)), /one MiB/);
let rejected = 0;
for (const mutate of [
    (r) => { r.isDraft = true; },
    (r) => { r.isPrerelease = true; },
    (r) => { r.tagName = 'v2.8.2'; },
    (r) => { r.assets.shift(); },
    (r) => { r.assets.push(r.assets[0]); },
    (r) => { r.assets[0].url = 'https://example.com/Setup.exe'; },
    (r, p) => { p.sourceCommit = ''; },
    (r, p) => { p.generatedAtUtc = 'not-a-date'; },
    (r, p) => { p.generatedAtUtc = '2026-02-30T00:54:48Z'; },
    (r, p) => { p.stagedExecutable.sha256 = 'd'.repeat(64); },
    (r, p, m) => { m.architecture = 'arm64'; },
    (r, p, m) => { m.bytes = 51; },
    (r, p, m) => { m.packageUrl = 'https://example.com/package'; },
]) {
    const [r, p, m] = structuredClone([release, provenance, manifest]);
    mutate(r, p, m);
    assert.throws(() => buildReleaseData(r, p, m));
    rejected++;
}
console.log(`PASS: release provenance, safe projection, BOM, byte limit, and ${rejected} invalid metadata cases.`);
