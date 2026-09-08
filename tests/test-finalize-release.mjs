import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import {
    compareVersions, finalize, GhCommandError, ghOutputLimitBytes, isNotFoundReleaseError,
    jobsSelector, latestReleaseSelector, latestSelectionIsCurrent, packageVersion,
    paginatedBase64Query, parseBase64JsonLines, parseVersion, replaceTiming, runGh,
    selectLatestRelease, timingBlock
} from '../scripts/finalize-release.mjs';

const fixture = JSON.parse(readFileSync(new URL('./fixtures/release-finalize/publication.json', import.meta.url)));
const workflow = readFileSync(new URL('../.github/workflows/release-finalize.yml', import.meta.url), 'utf8');
assert.match(
    workflow,
    /^concurrency:\r?\n  group: release-finalizer-\$\{\{ github\.repository \}\}-\$\{\{ github\.event\.workflow_run\.id \}\}-\$\{\{ github\.event\.workflow_run\.run_attempt \}\}\r?\n  cancel-in-progress: false\r?$/m,
    'each finalizer run must keep its own uncancelled workflow concurrency group'
);
assert.doesNotMatch(workflow, /^  queue:/m, 'the finalizer uses a per-run concurrency group instead of a queue extension');
assert.match(workflow, /^          WORKFLOW_RUN_ATTEMPT: \$\{\{ github\.event\.workflow_run\.run_attempt \}\}\r?$/m);
assert.equal(packageVersion(fixture.run.run_number, fixture.run.run_attempt), '2.8.19901');
assert.equal(packageVersion(Number.MAX_SAFE_INTEGER, 1), '209715202.7.65408');
assert.throws(() => packageVersion(Number.MAX_SAFE_INTEGER + 1, 1), /Invalid run number/);
assert.ok(compareVersions(parseVersion('v2.8.19901'), parseVersion('v2.8.19801')) > 0);
const publication = fixture.jobs[1].steps[0];
const block = timingBlock(fixture.jobs[0].started_at, publication.completed_at);
assert.match(block, /Workflow duration: 01:11:31/);
assert.match(timingBlock('2024-02-29T00:00:00Z', '2024-02-29T00:00:01Z'), /Workflow duration: 00:00:01/);
for (const invalidUtc of ['2025-02-29T00:00:00Z', '2024-02-30T00:00:00Z', '2024-04-31T00:00:00Z', '2024-01-01T24:00:00Z', '2024-01-01T00:60:00Z', '2024-01-01T00:00:60Z']) {
    assert.throws(() => timingBlock(invalidUtc, '2024-03-01T00:00:00Z'), /Invalid UTC timestamp/);
}
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
assert.deepEqual(parseBase64JsonLines(`${encoded}\r\n`), apiReleases);
const pageOne = apiReleases.slice(0, 2).map((release) => Buffer.from(JSON.stringify(release)).toString('base64')).join('\n');
const pageTwo = apiReleases.slice(2).map((release) => Buffer.from(JSON.stringify(release)).toString('base64')).join('\n');
assert.deepEqual(parseBase64JsonLines(`${pageOne}\n${pageTwo}`), apiReleases, 'paginated gh output must retain records from every page');
assert.throws(() => parseBase64JsonLines('gh: unknown flag: |'), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines('A==='), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines('AAAA='), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines('AAAA===='), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines('AA=A'), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines(` ${encoded}`), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines(`${encoded} `), /non-base64 release record/);
assert.throws(() => parseBase64JsonLines(`${encoded}\n\n`), /non-base64 release record/);
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
assert.equal(isNotFoundReleaseError(notFound), false);
const namedNotFound = captureThrow(() => runGh(['release', 'view', 'v1'], () => ({ status: 1, stdout: '', stderr: 'release not found' })));
assert.equal(isNotFoundReleaseError(namedNotFound), true);
const misleadingNotFound = captureThrow(() => runGh(['release', 'view', 'v1'], () => ({ status: 1, stdout: '', stderr: 'HTTP 403: release not found' })));
assert.equal(isNotFoundReleaseError(misleadingNotFound), false);
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

let spawnOptions;
assert.equal(runGh(['api', 'repos/example/project/releases'], (_command, _args, options) => {
    spawnOptions = options;
    return { status: 0, stdout: '{}', stderr: '' };
}), '{}');
assert.deepEqual(spawnOptions, { encoding: 'utf8', maxBuffer: ghOutputLimitBytes });

const base64Line = (value) => Buffer.from(JSON.stringify(value)).toString('base64');
const finalizerRun = { conclusion: 'success', run_number: 200, run_attempt: 1, id: 42, head_sha: 'abc123' };
const finalizerRelease = {
    isDraft: false, targetCommitish: 'abc123',
    body: '<!-- kpxc-release-finalization:run=42;tag=v2.8.20001;target=abc123 -->\n<!-- kpxc-workflow-timing:start -->\npending\n<!-- kpxc-workflow-timing:end -->'
};
const finalizerReleases = `${base64Line({ tag_name: 'v2.8.20001', draft: false, prerelease: false })}\n`;
const finalizerJobs = `${base64Line({
    name: 'Publish Squirrel.Windows release', started_at: '2026-09-07T06:00:00Z',
    steps: [{ name: 'Create the GitHub Release', completed_at: '2026-09-07T07:11:31Z', conclusion: 'success' }]
})}\n${base64Line({ name: 'Skipped auxiliary job', started_at: null, steps: [{ name: 'Skipped step', completed_at: null, conclusion: null }] })}\n`;
const finalizerCalls = [];
function makeFinalizerRunner(mode = '', calls = [], evidence = {}) {
    let releaseListReads = 0;
    return (args) => {
        calls.push(args);
        if (args[0] === 'release' && args[1] === 'view') {
            if (mode === 'notfound') throw new GhCommandError(args, { status: 1, stdout: '', stderr: 'release not found' });
            if (mode === 'auth') throw new GhCommandError(args, { status: 1, stdout: '', stderr: 'HTTP 403: Resource not accessible' });
            if (mode === 'network') throw new GhCommandError(args, { status: 1, stdout: '', stderr: 'network connection refused' });
            if (mode === 'malformed') return 'not-json';
            if (mode === 'metadata') return JSON.stringify({ isDraft: false, targetCommitish: 'abc123' });
            if (mode === 'missing-marker') return JSON.stringify({ ...finalizerRelease, body: finalizerRelease.body.replace('kpxc-release-finalization', 'missing-marker') });
            if (mode === 'wrong-target') return JSON.stringify({ ...finalizerRelease, targetCommitish: 'wrong-target' });
            if (mode === 'draft') return JSON.stringify({ ...finalizerRelease, isDraft: true });
            return JSON.stringify(finalizerRelease);
        }
        if (args[0] === 'release' && args[1] === 'edit' && args.includes('--notes-file')) {
            evidence.notesPath = args[args.indexOf('--notes-file') + 1];
            evidence.notes = readFileSync(evidence.notesPath, 'utf8');
            if (mode === 'notes-fail') throw new Error('notes edit failed');
            return '';
        }
        if (args[0] === 'release') return '';
        if (args[0] === 'api' && args[1] === 'repos/example/project/actions/runs/42/attempts/1') {
            if (mode === 'run-id-mismatch') return JSON.stringify({ ...finalizerRun, id: 43 });
            if (mode === 'run-attempt-mismatch') return JSON.stringify({ ...finalizerRun, run_attempt: 2 });
            if (mode === 'run-empty-sha') return JSON.stringify({ ...finalizerRun, head_sha: '' });
            return JSON.stringify(finalizerRun);
        }
        if (args[0] === 'api' && args[1] === 'repos/example/project/releases/latest') {
            if (mode === 'latest-invalid') return JSON.stringify({});
            return JSON.stringify({ tag_name: mode === 'stale' ? 'v2.8.20002' : 'v2.8.20001' });
        }
        if (args[0] === 'api' && args[2] === 'repos/example/project/actions/runs/42/attempts/1/jobs?per_page=100') {
            if (mode === 'jobs-invalid') return `${base64Line({ name: 'Publish Squirrel.Windows release', started_at: 'invalid', steps: [] })}\n`;
            return finalizerJobs;
        }
        if (args[0] === 'api' && args[2] === 'repos/example/project/releases?per_page=100') {
            releaseListReads += 1;
            if ((mode === 'stale' || mode === 'stale-exhausted') && releaseListReads % 2 === 0) {
                return `${base64Line({ tag_name: 'v2.8.20002', draft: false, prerelease: false })}\n`;
            }
            return mode === 'stale' && releaseListReads > 2
                ? `${base64Line({ tag_name: 'v2.8.20002', draft: false, prerelease: false })}\n`
                : finalizerReleases;
        }
        assert.fail(`Unexpected finalizer command: ${args.join(' ')}`);
    };
}
const finalizerEvidence = {};
finalize('example/project', '42', '1', makeFinalizerRunner('', finalizerCalls, finalizerEvidence));
assert.deepEqual(finalizerCalls[0], ['api', 'repos/example/project/actions/runs/42/attempts/1']);
assert.deepEqual(finalizerCalls[2], paginatedBase64Query('repos/example/project/actions/runs/42/attempts/1/jobs?per_page=100', jobsSelector));
assert.deepEqual(finalizerCalls[3].slice(0, 5), ['release', 'edit', 'v2.8.20001', '--repo', 'example/project']);
assert.ok(finalizerCalls[3].includes('--notes-file'), 'timing notes must be edited before latest designation');
assert.match(finalizerEvidence.notes, /Workflow duration: 01:11:31/);
assert.equal(existsSync(finalizerEvidence.notesPath), false, 'successful notes edit must remove its scratch file');
assert.deepEqual(finalizerCalls[4], paginatedBase64Query('repos/example/project/releases?per_page=100', latestReleaseSelector));
assert.deepEqual(finalizerCalls[5], ['release', 'edit', 'v2.8.20001', '--repo', 'example/project', '--latest']);
assert.deepEqual(finalizerCalls[6], paginatedBase64Query('repos/example/project/releases?per_page=100', latestReleaseSelector));
assert.deepEqual(finalizerCalls[7], ['api', 'repos/example/project/releases/latest']);

const staleCalls = [];
assert.doesNotThrow(() => finalize('example/project', '42', '1', makeFinalizerRunner('stale', staleCalls)));
assert.equal(staleCalls.filter((args) => args[0] === 'release' && args.includes('--latest')).length, 2, 'a stale first selection must retry');
assert.deepEqual(staleCalls.at(-1), ['api', 'repos/example/project/releases/latest']);
const staleExhaustedCalls = [];
assert.throws(() => finalize('example/project', '42', '1', makeFinalizerRunner('stale-exhausted', staleExhaustedCalls)), /after three attempts/);
assert.equal(staleExhaustedCalls.filter((args) => args[0] === 'release' && args.includes('--latest')).length, 3);
for (const mode of ['run-id-mismatch', 'run-attempt-mismatch', 'run-empty-sha', 'auth', 'network', 'malformed', 'metadata', 'wrong-target', 'draft', 'jobs-invalid', 'latest-invalid']) {
    assert.throws(() => finalize('example/project', '42', '1', makeFinalizerRunner(mode)));
}
for (const invalidAttempt of ['0', '01', '9007199254740992']) {
    assert.throws(() => finalize('example/project', '42', invalidAttempt, makeFinalizerRunner()));
}
const missingMarkerCalls = [];
assert.doesNotThrow(() => finalize('example/project', '42', '1', makeFinalizerRunner('missing-marker', missingMarkerCalls)));
assert.equal(missingMarkerCalls.some((args) => args[0] === 'release' && args[1] === 'edit'), false);
assert.doesNotThrow(() => finalize('example/project', '42', '1', makeFinalizerRunner('notfound')));
const notesFailureEvidence = {};
assert.throws(() => finalize('example/project', '42', '1', makeFinalizerRunner('notes-fail', [], notesFailureEvidence)), /notes edit failed/);
assert.equal(existsSync(notesFailureEvidence.notesPath), false, 'failed notes edit must remove its scratch file');
process.stdout.write('test-finalize-release: PASS\n');
