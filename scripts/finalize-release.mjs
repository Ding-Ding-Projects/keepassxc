#!/usr/bin/env node
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const timingStart = '<!-- kpxc-workflow-timing:start -->';
const timingEnd = '<!-- kpxc-workflow-timing:end -->';
const identityPrefix = '<!-- kpxc-release-finalization:';

function fail(message) { throw new Error(message); }
function runGh(args) {
    const result = spawnSync('gh', args, { encoding: 'utf8' });
    if (result.error) fail(`gh ${args.join(' ')} could not start: ${result.error.message}`);
    if (result.status !== 0) fail(`gh ${args.join(' ')} failed: ${(result.stderr || result.stdout).trim()}`);
    return result.stdout;
}
function jsonGh(args) { return JSON.parse(runGh(args)); }
function utcSeconds(value) {
    const seconds = Date.parse(value) / 1000;
    if (!Number.isFinite(seconds)) fail(`Invalid UTC timestamp: ${value}`);
    return seconds;
}
function duration(seconds) {
    return [Math.floor(seconds / 3600), Math.floor(seconds % 3600 / 60), seconds % 60]
        .map((part) => String(part).padStart(2, '0')).join(':');
}
export function packageVersion(runNumber, runAttempt) {
    if (!Number.isInteger(runNumber) || runNumber < 1 || !Number.isInteger(runAttempt) || runAttempt < 1) fail('Invalid run number or attempt.');
    const ordinal = runNumber * 100 + runAttempt;
    const patch = ordinal % 65536;
    const minorOrdinal = 8 + Math.floor(ordinal / 65536);
    return `${2 + Math.floor(minorOrdinal / 65536)}.${minorOrdinal % 65536}.${patch}`;
}
export function parseVersion(tag) {
    const match = /^v(\d+)\.(\d+)\.(\d+)$/.exec(tag);
    if (!match) return null;
    return match.slice(1).map((part) => Number(part));
}
export function compareVersions(left, right) {
    for (let index = 0; index < 3; ++index) {
        if (left[index] !== right[index]) return left[index] - right[index];
    }
    return 0;
}
export function timingBlock(startedAt, publishedAt) {
    const elapsed = utcSeconds(publishedAt) - utcSeconds(startedAt);
    if (elapsed < 0) fail('Publication precedes the first job start.');
    return `${timingStart}\nWorkflow started: ${startedAt}\nWorkflow completed: ${publishedAt}\nWorkflow duration: ${duration(elapsed)}\n${timingEnd}`;
}
export function replaceTiming(body, block) {
    const start = body.indexOf(timingStart);
    const end = body.indexOf(timingEnd);
    if (start < 0 || end < start) fail('Release body has no owned timing block.');
    return `${body.slice(0, start)}${block}${body.slice(end + timingEnd.length)}`;
}
export function paginatedBase64Query(endpoint, selector = '.[]') {
    if (typeof endpoint !== 'string' || !endpoint) fail('A GitHub API endpoint is required.');
    if (typeof selector !== 'string' || !selector) fail('A jq selector is required.');
    // Keep the complete jq program in one argv element. Passing `|` and
    // `@base64` as separate arguments makes gh treat them as extra operands.
    return ['api', '--paginate', endpoint, '--jq', `${selector} | @base64`];
}
export const latestReleaseSelector = 'map({tag_name, draft, prerelease})[]';
export function parseBase64JsonLines(output) {
    if (typeof output !== 'string') fail('GitHub API output must be text.');
    return output.trim().split(/\r?\n/).filter(Boolean).map((line) => {
        const decoded = Buffer.from(line, 'base64').toString('utf8');
        if (!decoded || Buffer.from(decoded, 'utf8').toString('base64').replace(/=+$/, '') !== line.replace(/=+$/, '')) {
            fail('GitHub API returned a non-base64 release record.');
        }
        try { return JSON.parse(decoded); }
        catch { fail('GitHub API returned base64 data that is not JSON.'); }
    });
}
function jsonLinesFromGh(endpoint, selector = '.[]') {
    return parseBase64JsonLines(runGh(paginatedBase64Query(endpoint, selector)));
}
function ensureReleaseIdentity(release, runId, tag, target) {
    const marker = `${identityPrefix}run=${runId};tag=${tag};target=${target} -->`;
    if (!release.body.includes(marker)) {
        return false;
    }
    if (release.isDraft || release.targetCommitish !== target) {
        fail('Release does not match its workflow run, tag, and target commit.');
    }
    return true;
}
export function selectLatestRelease(releases) {
    if (!Array.isArray(releases)) fail('Release records must be an array.');
    const stable = releases
        .filter((release) => !release.draft && !release.prerelease)
        .map((release) => ({ release, version: parseVersion(release.tag_name) }))
        .filter(({ version }) => version);
    if (!stable.length) fail('No stable numeric release exists.');
    return stable.reduce((best, current) => compareVersions(current.version, best.version) > 0 ? current : best).release;
}
function selectLatest(repository) {
    return selectLatestRelease(jsonLinesFromGh(`repos/${repository}/releases?per_page=100`, latestReleaseSelector));
}
function finalize(repository, runId) {
    const run = jsonGh(['api', `repos/${repository}/actions/runs/${runId}`]);
    if (run.conclusion !== 'success') fail('Only successful workflow runs can finalize a release.');
    const tag = `v${packageVersion(run.run_number, run.run_attempt)}`;
    let release;
    try {
        release = jsonGh(['release', 'view', tag, '--repo', repository, '--json', 'body,targetCommitish,isDraft']);
    } catch {
        console.log(`No release ${tag} exists for workflow run ${run.id}; skipping finalization.`);
        return;
    }
    if (!ensureReleaseIdentity(release, run.id, tag, run.head_sha)) {
        console.log(`Release ${tag} has no finalizer marker for workflow run ${run.id}; skipping finalization.`);
        return;
    }
    const jobs = jsonLinesFromGh(`repos/${repository}/actions/runs/${runId}/jobs?per_page=100`, '.jobs[]');
    const starts = jobs.map((job) => job.started_at).filter(Boolean).sort();
    const releaseJob = jobs.find((job) => job.name === 'Publish Squirrel.Windows release');
    const publication = releaseJob?.steps?.find((step) => step.name === 'Create the GitHub Release');
    if (!starts.length || !publication?.completed_at || publication.conclusion !== 'success') {
        fail('Workflow timing is unavailable because the release publication step lacks a successful completion timestamp.');
    }
    const updatedBody = replaceTiming(release.body, timingBlock(starts[0], publication.completed_at));
    const scratch = mkdtempSync(join(tmpdir(), 'keepassxc-release-notes-'));
    try {
        const notes = join(scratch, 'notes.md');
        writeFileSync(notes, updatedBody, 'utf8');
        runGh(['release', 'edit', tag, '--repo', repository, '--notes-file', notes]);
    } finally { rmSync(scratch, { recursive: true, force: true }); }
    for (let attempt = 0; attempt < 3; ++attempt) {
        const latest = selectLatest(repository);
        runGh(['release', 'edit', latest.tag_name, '--repo', repository, '--latest']);
        const verified = jsonGh(['api', `repos/${repository}/releases/latest`]);
        if (verified.tag_name === latest.tag_name) return;
    }
    fail('Could not verify the highest numeric stable release as latest after three attempts.');
}
function selfTest() {
    if (packageVersion(199, 1) !== '2.8.19901') fail('Version formula drifted.');
    if (compareVersions(parseVersion('v2.8.19901'), parseVersion('v2.8.19801')) <= 0) fail('Numeric release ordering drifted.');
    const body = `before\n${timingStart}\nWorkflow timing finalization pending.\n${timingEnd}\nafter`;
    const replacement = timingBlock('2026-09-07T06:00:00Z', '2026-09-07T07:11:31Z');
    const result = replaceTiming(body, replacement);
    if (!result.includes('Workflow duration: 01:11:31') || result.includes('pending')) fail('Timing replacement drifted.');
    let rejected = false;
    try { replaceTiming('no marker', replacement); } catch { rejected = true; }
    if (!rejected) fail('Missing owned timing block was accepted.');
    process.stdout.write('finalize-release self-test: PASS\n');
}
if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
    if (process.argv.includes('--self-test')) selfTest();
    else if (process.argv.includes('--finalize')) {
        const repository = process.env.GITHUB_REPOSITORY;
        const runId = process.env.WORKFLOW_RUN_ID;
        if (!repository || !runId || !/^\d+$/.test(runId)) fail('GITHUB_REPOSITORY and numeric WORKFLOW_RUN_ID are required.');
        finalize(repository, runId);
    } else fail('Use --self-test or --finalize.');
}
