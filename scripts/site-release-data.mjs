import { openSync, readSync, closeSync, writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const repository = 'Ding-Ding-Projects/keepassxc';
const releaseRoot = `https://github.com/${repository}/releases/`;
const sha256 = /^[a-f0-9]{64}$/i;
const commit = /^[a-f0-9]{40}$/i;
function requireValue(condition, message) {
    if (!condition) throw new Error(message);
}

export function buildReleaseData(release, provenance, manifest) {
    requireValue(release && release.isDraft === false && release.isPrerelease === false, 'A published stable release is required.');
    requireValue(/^v\d+\.\d+\.\d+$/.test(release.tagName), 'Invalid release version.');
    const version = release.tagName.slice(1);
    requireValue(provenance?.schemaVersion === 1 && manifest?.schemaVersion === 1, 'Unsupported provenance or manifest schema.');
    requireValue(provenance.version === version && manifest.version === version, 'Release and package versions differ.');
    for (const record of [provenance, manifest]) {
        requireValue(record.packageId === 'KeePassXC.Material' && record.architecture === 'x64', 'Unexpected package identity or architecture.');
    }
    requireValue(commit.test(provenance.sourceCommit), 'Missing source commit provenance.');
    const timestamp = provenance.generatedAtUtc;
    requireValue(typeof timestamp === 'string' && /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?Z$/.test(timestamp) && Number.isFinite(Date.parse(timestamp)) && new Date(timestamp).toISOString().slice(0,19) === timestamp.slice(0,19), 'Missing or invalid build timestamp provenance.');
    requireValue(sha256.test(manifest.sha256) && sha256.test(manifest.executableSha256), 'Invalid package or executable digest.');
    requireValue(provenance.stagedExecutable?.sha256?.toLowerCase() === manifest.executableSha256.toLowerCase(), 'Executable provenance differs from update manifest.');
    const base = `${releaseRoot}download/${release.tagName}/`;
    const expectedPackage = `KeePassXC.Material-${version}-full.nupkg`;
    requireValue(manifest.packageFile === expectedPackage && manifest.packageUrl === base + expectedPackage, 'Unexpected package URL.');
    requireValue(manifest.notesUrl === `${releaseRoot}tag/${release.tagName}`, 'Unexpected release notes URL.');
    requireValue(Number.isSafeInteger(manifest.bytes) && manifest.bytes > 0, 'Invalid package size.');
    requireValue(Array.isArray(release.assets), 'Release assets are missing.');
    const assets = {};
    for (const name of ['Setup.exe', 'RELEASES', expectedPackage, 'update-manifest-v1.json', 'build-provenance.json']) {
        const matches = release.assets.filter(asset => asset.name === name);
        requireValue(matches.length === 1, `Expected exactly one ${name} asset.`);
        const asset = matches[0];
        requireValue(asset.url === base + name && Number.isSafeInteger(asset.size) && asset.size > 0, `Invalid ${name} asset metadata.`);
        assets[name] = { url: asset.url, bytes: asset.size };
    }
    requireValue(assets[expectedPackage].bytes === manifest.bytes, 'Package byte count differs from release metadata.');
    return {
        schemaVersion: 1,
        version,
        tag: release.tagName,
        sourceCommit: provenance.sourceCommit,
        updatedAtUtc: timestamp,
        updatedAtSource: 'build-provenance.generatedAtUtc',
        notesUrl: manifest.notesUrl,
        installer: assets['Setup.exe'],
        package: { ...assets[expectedPackage], sha256: manifest.sha256 },
        unsigned: true,
    };
}

export function parseReleaseJson(bytes) {
    requireValue(bytes.length <= 1024 * 1024, 'Release metadata exceeds the one MiB limit.');
    return JSON.parse(bytes.toString('utf8').replace(/^\uFEFF/, ''));
}

function readBoundedJson(filename) {
    const descriptor = openSync(filename, 'r');
    try {
        const bytes = Buffer.alloc(1024 * 1024 + 1);
        let length = 0;
        while (length < bytes.length) {
            const count = readSync(descriptor, bytes, length, bytes.length - length, null);
            if (!count) break;
            length += count;
        }
        return parseReleaseJson(bytes.subarray(0, length));
    } finally {
        closeSync(descriptor);
    }
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
    const [release, provenance, manifest, destination] = process.argv.slice(2);
    requireValue(release && provenance && manifest && destination, 'Usage: node scripts/site-release-data.mjs RELEASE_JSON PROVENANCE_JSON MANIFEST_JSON OUTPUT_JSON');
    const data = buildReleaseData(readBoundedJson(release), readBoundedJson(provenance), readBoundedJson(manifest));
    writeFileSync(destination, JSON.stringify(data, null, 2) + '\n');
    console.log(`Validated website release metadata for ${data.version}.`);
}
