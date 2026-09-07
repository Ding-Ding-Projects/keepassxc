import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { compareVersions, packageVersion, parseVersion, replaceTiming, timingBlock } from '../scripts/finalize-release.mjs';

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
process.stdout.write('test-finalize-release: PASS\n');
