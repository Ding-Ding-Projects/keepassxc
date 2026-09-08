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
export const ghOutputLimitBytes = 512 * 1024;

function fail(message) { throw new Error(message); }
export class GhCommandError extends Error {
    constructor(args, result) {
        super(`gh ${args.join(' ')} failed: ${(result.stderr || result.stdout || result.error?.message || 'unknown error').trim()}`);
        this.args = args;
        this.status = result.status;
        this.stderr = result.stderr || '';
        this.code = result.error?.code;
    }
}
export function runGh(args, spawn = spawnSync) {
    const result = spawn('gh', args, { encoding: 'utf8', maxBuffer: ghOutputLimitBytes });
    if (result.error?.code === 'ENOBUFS') fail(`gh ${args.join(' ')} exceeded the ${ghOutputLimitBytes}-byte output limit.`);
    if (result.error) throw new GhCommandError(args, result);
    if (Buffer.byteLength(result.stdout || '', 'utf8') > ghOutputLimitBytes) {
        fail(`gh ${args.join(' ')} exceeded the ${ghOutputLimitBytes}-byte output limit.`);
    }
    if (result.status !== 0) throw new GhCommandError(args, result);
    return result.stdout;
}
function jsonGh(args, runner = runGh) { return JSON.parse(runner(args)); }
function positiveSafeInteger(value) {
    if (typeof value === 'number') return Number.isSafeInteger(value) && value > 0 ? value : null;
    if (typeof value !== 'string' || !/^[1-9]\d*$/.test(value)) return null;
    const number = Number(value);
    return Number.isSafeInteger(number) ? number : null;
}
function utcSeconds(value) {
    if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/.test(value)) fail(`Invalid UTC timestamp: ${value}`);
    const seconds = Date.parse(value) / 1000;
    if (!Number.isFinite(seconds) || new Date(seconds * 1000).toISOString() !== value.replace('Z', '.000Z')) fail(`Invalid UTC timestamp: ${value}`);
    return seconds;
}
function duration(seconds) {
    return [Math.floor(seconds / 3600), Math.floor(seconds % 3600 / 60), seconds % 60]
        .map((part) => String(part).padStart(2, '0')).join(':');
}
export function packageVersion(runNumber, runAttempt) {
    if (!Number.isSafeInteger(runNumber) || runNumber < 1 || !Number.isSafeInteger(runAttempt) || runAttempt < 1) fail('Invalid run number or attempt.');
    const ordinal = runNumber * 100 + runAttempt;
    const patch = ordinal % 65536;
    const minorOrdinal = 8 + Math.floor(ordinal / 65536);
    return `${2 + Math.floor(minorOrdinal / 65536)}.${minorOrdinal % 65536}.${patch}`;
}
export function parseVersion(tag) {
    const match = /^v(\d+)\.(\d+)\.(\d+)$/.exec(tag);
    if (!match) return null;
    return match.slice(1).map((part) => BigInt(part));
}
export function compareVersions(left, right) {
    for (let index = 0; index < 3; ++index) {
        if (left[index] !== right[index]) return left[index] > right[index] ? 1 : -1;
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
export const jobsSelector = '.jobs[] | {name, started_at, steps: [.steps[] | {name, completed_at, conclusion}]}';
export function parseBase64JsonLines(output) {
    if (typeof output !== 'string') fail('GitHub API output must be text.');
    if (output === '') return [];
    const lines = output.split(/\r?\n/);
    if (lines.at(-1) === '') lines.pop();
    return lines.map((line) => {
        if (!line || !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(line)) {
            fail('GitHub API returned a non-base64 release record.');
        }
        const bytes = Buffer.from(line, 'base64');
        if (bytes.toString('base64') !== line) fail('GitHub API returned noncanonical base64 release data.');
        const decoded = bytes.toString('utf8');
        try { return JSON.parse(decoded); }
        catch { fail('GitHub API returned base64 data that is not JSON.'); }
    });
}
function jsonLinesFromGh(endpoint, selector = '.[]', runner = runGh) {
    return parseBase64JsonLines(runner(paginatedBase64Query(endpoint, selector)));
}
function ensureReleaseIdentity(release, runId, tag, target) {
    if (!release || typeof release.body !== 'string' || typeof release.isDraft !== 'boolean' || typeof release.targetCommitish !== 'string') {
        fail('Release metadata has an invalid finalization schema.');
    }
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
    const candidates = releases.map((release) => {
        if (!release || typeof release !== 'object' || Array.isArray(release)
            || typeof release.tag_name !== 'string' || typeof release.draft !== 'boolean' || typeof release.prerelease !== 'boolean') {
            fail('Release record has an invalid selection schema.');
        }
        return { release, version: parseVersion(release.tag_name) };
    });
    const stable = candidates
        .filter(({ release }) => !release.draft && !release.prerelease)
        .filter(({ version }) => version);
    if (!stable.length) fail('No stable numeric release exists.');
    return stable.reduce((best, current) => compareVersions(current.version, best.version) > 0 ? current : best).release;
}
function selectLatest(repository, runner = runGh) {
    return selectLatestRelease(jsonLinesFromGh(`repos/${repository}/releases?per_page=100`, latestReleaseSelector, runner));
}
export function latestSelectionIsCurrent(selected, releases) {
    return Boolean(selected) && selectLatestRelease(releases).tag_name === selected.tag_name;
}
export function isNotFoundReleaseError(error) {
    const diagnostic = error instanceof GhCommandError ? error.stderr.trim() : '';
    return error instanceof GhCommandError && error.status !== 0 && diagnostic === 'release not found';
}
export function finalize(repository, runId, expectedAttempt, runner = runGh) {
    const numericRunId = positiveSafeInteger(runId);
    const numericAttempt = positiveSafeInteger(expectedAttempt);
    if (!numericRunId || !numericAttempt) {
        fail('Numeric WORKFLOW_RUN_ID and positive WORKFLOW_RUN_ATTEMPT are required.');
    }
    const run = jsonGh(['api', `repos/${repository}/actions/runs/${runId}/attempts/${expectedAttempt}`], runner);
    if (!run || typeof run !== 'object' || !Number.isSafeInteger(run.id) || run.id !== numericRunId
        || !Number.isSafeInteger(run.run_attempt) || run.run_attempt !== numericAttempt
        || !Number.isSafeInteger(run.run_number) || run.run_number < 1 || run.conclusion !== 'success'
        || typeof run.head_sha !== 'string' || !run.head_sha.trim()) {
        fail('Workflow run metadata does not match the requested successful attempt.');
    }
    const tag = `v${packageVersion(run.run_number, run.run_attempt)}`;
    let release;
    try {
        release = jsonGh(['release', 'view', tag, '--repo', repository, '--json', 'body,targetCommitish,isDraft'], runner);
    } catch (error) {
        if (!isNotFoundReleaseError(error)) throw error;
        console.log(`No release ${tag} exists for workflow run ${run.id}; skipping finalization.`);
        return;
    }
    if (!ensureReleaseIdentity(release, run.id, tag, run.head_sha)) {
        console.log(`Release ${tag} has no finalizer marker for workflow run ${run.id}; skipping finalization.`);
        return;
    }
    const jobs = jsonLinesFromGh(`repos/${repository}/actions/runs/${runId}/attempts/${expectedAttempt}/jobs?per_page=100`, jobsSelector, runner);
    for (const job of jobs) {
        if (!job || typeof job !== 'object' || typeof job.name !== 'string'
            || (job.started_at !== null && typeof job.started_at !== 'string') || !Array.isArray(job.steps)) {
            fail('Workflow job metadata has an invalid timing schema.');
        }
        if (job.started_at !== null) utcSeconds(job.started_at);
        for (const step of job.steps) {
            if (!step || typeof step !== 'object' || typeof step.name !== 'string'
                || (step.completed_at !== null && typeof step.completed_at !== 'string')
                || (step.conclusion !== null && typeof step.conclusion !== 'string')) {
                fail('Workflow step metadata has an invalid timing schema.');
            }
            if (step.completed_at !== null) utcSeconds(step.completed_at);
        }
    }
    const starts = jobs.map((job) => job.started_at).filter(Boolean).sort();
    const releaseJob = jobs.find((job) => job.name === 'Publish Squirrel.Windows release');
    const publication = releaseJob?.steps?.find((step) => step.name === 'Create the GitHub Release');
    if (!starts.length || typeof releaseJob?.started_at !== 'string' || typeof publication?.completed_at !== 'string' || publication.conclusion !== 'success') {
        fail('Workflow timing is unavailable because the release publication step lacks a successful completion timestamp.');
    }
    const updatedBody = replaceTiming(release.body, timingBlock(starts[0], publication.completed_at));
    const scratch = mkdtempSync(join(tmpdir(), 'keepassxc-release-notes-'));
    try {
        const notes = join(scratch, 'notes.md');
        writeFileSync(notes, updatedBody, 'utf8');
        runner(['release', 'edit', tag, '--repo', repository, '--notes-file', notes]);
    } finally { rmSync(scratch, { recursive: true, force: true }); }
    for (let attempt = 0; attempt < 3; ++attempt) {
        const latest = selectLatest(repository, runner);
        runner(['release', 'edit', latest.tag_name, '--repo', repository, '--latest']);
        const afterEdit = jsonLinesFromGh(`repos/${repository}/releases?per_page=100`, latestReleaseSelector, runner);
        if (!latestSelectionIsCurrent(latest, afterEdit)) continue;
        const verified = jsonGh(['api', `repos/${repository}/releases/latest`], runner);
        if (!verified || typeof verified !== 'object' || typeof verified.tag_name !== 'string') {
            fail('Latest release verification has an invalid schema.');
        }
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
        const runAttempt = process.env.WORKFLOW_RUN_ATTEMPT;
        if (!repository || !runId || !runAttempt) fail('GITHUB_REPOSITORY, WORKFLOW_RUN_ID, and WORKFLOW_RUN_ATTEMPT are required.');
        finalize(repository, runId, runAttempt);
    } else fail('Use --self-test or --finalize.');
}
