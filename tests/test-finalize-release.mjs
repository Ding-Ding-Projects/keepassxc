import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import {
    compareVersions, latestReleaseSelector, packageVersion, paginatedBase64Query,
    parseBase64JsonLines, parseVersion, replaceTiming, selectLatestRelease, timingBlock
} from '../scripts/finalize-release.mjs';

const fixture = JSON.parse(readFileSync(new URL('./fixtures/release-finalize/publication.json', import.meta.url)));
assert.equal(packageVersion(fixture.run.run_number, fixture.run.run_attempt), '2.8.19901');
assert.ok(compareVersions(parseVersion('v2.8.19901'), parseVersion('v2.8.19801')) > 0);
const publication = fixture.jobs[1].steps[0];
const block = timingBlock(fixture.jobs[0].started_at, publication.completed_at);
assert.match(block, /Workflow duration: 01:11:31/);
const marker = `<!-- kpxc-release-finalization:run=${fixture.run.id};tag=v2.8.19901;target=${fixture.run.head_sha} -->`;
const body = `${marker}\n<!-- kpxc-workflow-timing:start -->\nWorkflow timing finalization pending the successful Create GitHub Release step.\n<!-- kpxc-workflow-timing:end -->`;
assert.match(replaceTiming(body, block), /Workflow completed: 2026-09-07T07:11:31Z/);
assert.throws(() => replaceTiming(marker, block), /owned timing block/);

const query = paginatedBase64Query('repos/example/project/releases?per_page=100');
assert.deepEqual(query, [
    'api', '--paginate', 'repos/example/project/releases?per_page=100', '--jq', '.[] | @base64'
]);
assert.equal(query.length, 5, 'the jq pipe must stay inside the --jq argument boundary');
assert.deepEqual(
    paginatedBase64Query('repos/example/project/releases?per_page=100', latestReleaseSelector),
    ['api', '--paginate', 'repos/example/project/releases?per_page=100', '--jq', 'map({tag_name, draft, prerelease})[] | @base64']
);
const apiReleases = [
    { tag_name: 'v2.8.19901', draft: false, prerelease: false },
    { tag_name: 'v2.9.1', draft: true, prerelease: false },
    { tag_name: 'v9.0.0', draft: false, prerelease: true },
    { tag_name: 'not-a-version', draft: false, prerelease: false },
    { tag_name: 'v2.8.20001', draft: false, prerelease: false }
];
const encoded = apiReleases.map((release) => Buffer.from(JSON.stringify(release)).toString('base64')).join('\n');
assert.deepEqual(parseBase64JsonLines(encoded), apiReleases);
assert.throws(() => parseBase64JsonLines('gh: unknown flag: |'), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines(Buffer.from('not json').toString('base64')), /not JSON/);
assert.equal(selectLatestRelease(parseBase64JsonLines(encoded)).tag_name, 'v2.8.20001');
assert.throws(() => selectLatestRelease([{ tag_name: 'v3.0.0', draft: true, prerelease: false }]), /No stable numeric release/);

const oversizedRelease = {
    tag_name: 'v99.0.0', draft: false, prerelease: false,
    body: 'x'.repeat(2 * 1024 * 1024), assets: Array.from({ length: 128 }, () => ({ name: 'package.nupkg', size: 4096 }))
};
const projectedRelease = (({ tag_name, draft, prerelease }) => ({ tag_name, draft, prerelease }))(oversizedRelease);
assert.equal(Buffer.byteLength(JSON.stringify(projectedRelease)) < 100, true, 'latest selection must not buffer release bodies or assets');
assert.equal(selectLatestRelease([projectedRelease, ...apiReleases]).tag_name, 'v99.0.0');
process.stdout.write('test-finalize-release: PASS\n');
