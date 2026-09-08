import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import {
    compareVersions, GhCommandError, ghOutputLimitBytes, isNotFoundReleaseError,
    jobsSelector, latestReleaseSelector, latestSelectionIsCurrent, packageVersion,
    paginatedBase64Query, parseBase64JsonLines, parseVersion, replaceTiming, runGh,
    selectLatestRelease, timingBlock
} from '../scripts/finalize-release.mjs';

const fixture = JSON.parse(readFileSync(new URL('./fixtures/release-finalize/publication.json', import.meta.url)));
const workflow = readFileSync(new URL('../.github/workflows/release-finalize.yml', import.meta.url), 'utf8');
assert.match(
    workflow,
    /^concurrency:\r?\n  group: release-finalizer-\$\{\{ github\.repository \}\}\r?\n  cancel-in-progress: false\r?\n  queue: max\r?$/m,
    'finalizer runs must serialize without cancelling an in-flight publication finalizer'
);
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
assert.deepEqual(
    paginatedBase64Query('repos/example/project/actions/runs/1/jobs?per_page=100', jobsSelector),
    ['api', '--paginate', 'repos/example/project/actions/runs/1/jobs?per_page=100', '--jq', '.jobs[] | {name, started_at, steps: [.steps[] | {name, completed_at, conclusion}]} | @base64']
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
const pageOne = apiReleases.slice(0, 2).map((release) => Buffer.from(JSON.stringify(release)).toString('base64')).join('\n');
const pageTwo = apiReleases.slice(2).map((release) => Buffer.from(JSON.stringify(release)).toString('base64')).join('\n');
assert.deepEqual(parseBase64JsonLines(`${pageOne}\n${pageTwo}`), apiReleases, 'paginated gh output must retain records from every page');
assert.throws(() => parseBase64JsonLines('gh: unknown flag: |'), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines('A==='), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines('AAAA='), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines('AAAA===='), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines('AA=A'), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines('AB=='), /noncanonical base64 release data/);
assert.throws(() => parseBase64JsonLines('AAB='), /noncanonical base64 release data/);
assert.throws(() => parseBase64JsonLines(Buffer.from('not json').toString('base64')), /not JSON/);
assert.equal(selectLatestRelease(parseBase64JsonLines(encoded)).tag_name, 'v2.8.20001');
assert.throws(() => selectLatestRelease([{ tag_name: 'v3.0.0', draft: true, prerelease: false }]), /No stable numeric release/);
assert.throws(() => selectLatestRelease([{ tag_name: 'v3.0.0', draft: null, prerelease: false }]), /invalid selection schema/);
assert.throws(() => selectLatestRelease([{ tag_name: 'v3.0.0', draft: false }]), /invalid selection schema/);
assert.ok(compareVersions(parseVersion('v9007199254740993.0.0'), parseVersion('v9007199254740992.999999999999999999.999999999999999999')) > 0);

const oversizedRelease = {
    tag_name: 'v99.0.0', draft: false, prerelease: false,
    body: 'x'.repeat(2 * 1024 * 1024), assets: Array.from({ length: 128 }, () => ({ name: 'package.nupkg', size: 4096 }))
};
const projectedRelease = (({ tag_name, draft, prerelease }) => ({ tag_name, draft, prerelease }))(oversizedRelease);
assert.equal(Buffer.byteLength(JSON.stringify(projectedRelease)) < 100, true, 'latest selection must not buffer release bodies or assets');
assert.equal(selectLatestRelease([projectedRelease, ...apiReleases]).tag_name, 'v99.0.0');
assert.equal(latestSelectionIsCurrent({ tag_name: 'v2.8.20001' }, apiReleases), true);
assert.equal(latestSelectionIsCurrent({ tag_name: 'v2.8.20001' }, [
    ...apiReleases, { tag_name: 'v2.8.20002', draft: false, prerelease: false }
]), false, 'a newer release after the edit must trigger a bounded retry');

function captureThrow(action) {
    try { action(); }
    catch (error) { return error; }
    assert.fail('Expected action to throw.');
}
const notFound = captureThrow(() => runGh(['release', 'view', 'v1'], () => ({ status: 1, stdout: '', stderr: 'HTTP 404: Not Found' })));
assert.ok(notFound instanceof GhCommandError);
assert.equal(isNotFoundReleaseError(notFound), true);
const namedNotFound = captureThrow(() => runGh(['release', 'view', 'v1'], () => ({ status: 1, stdout: '', stderr: 'release not found' })));
assert.equal(isNotFoundReleaseError(namedNotFound), true);
const denied = captureThrow(() => runGh(['release', 'view', 'v1'], () => ({ status: 1, stdout: '', stderr: 'HTTP 403: Resource not accessible' })));
assert.equal(isNotFoundReleaseError(denied), false);
const unavailable = captureThrow(() => runGh(['release', 'view', 'v1'], () => ({ status: 1, stdout: '', stderr: 'network connection refused' })));
assert.equal(isNotFoundReleaseError(unavailable), false);
assert.throws(() => runGh(['api', 'repos/example/project/releases'], () => ({
    status: null, stdout: '', stderr: '', error: Object.assign(new Error('spawn ENOBUFS'), { code: 'ENOBUFS' })
})), /output limit/);
assert.throws(() => runGh(['api', 'repos/example/project/releases'], () => ({
    status: 0, stdout: 'x'.repeat(ghOutputLimitBytes + 1), stderr: ''
})), /output limit/);
process.stdout.write('test-finalize-release: PASS\n');
