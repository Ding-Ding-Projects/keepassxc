import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { validateStructure, validateEvidence } from './check-parity.mjs';
import { createHash } from 'node:crypto';
import { writeFileSync, renameSync, existsSync } from 'node:fs';

const here = dirname(fileURLToPath(import.meta.url));
const original = JSON.parse(readFileSync(join(here, 'inventory.json'), 'utf8'));

function clone() {
  return JSON.parse(JSON.stringify(original));
}

function expectRed(name, mutate) {
  const candidate = clone();
  mutate(candidate);
  const errors = validateStructure(candidate);
  if (!errors.length) throw new Error(`${name}: guard stayed green`);
  process.stdout.write(`RED: ${name} — ${errors[0]}\n`);
}

const green = validateStructure(original);
if (green.length) throw new Error(`baseline is red: ${green.join('; ')}`);
process.stdout.write('GREEN: baseline structure and reference hashes\n');

expectRed('reference omitted', inventory => inventory.rows.pop());
expectRed('duplicate row id', inventory => { inventory.rows[1].id = inventory.rows[0].id; });
expectRed('missing reference route', inventory => { inventory.rows[0].referenceRoute = ''; });
expectRed('missing built route', inventory => { inventory.rows[0].builtRoute = ''; });
expectRed('tuple screen mismatch', inventory => { inventory.rows[0].tuple.screen = 'wrong-screen'; });
expectRed('tuple viewport field missing', inventory => { delete inventory.rows[0].tuple.viewport.width; });
expectRed('deterministic input missing', inventory => { delete inventory.rows[0].determinism.random; });
expectRed('audit missing', inventory => { delete inventory.rows[0].audit; });
expectRed('evidence field missing', inventory => { delete inventory.rows[0].evidence.diff; });
expectRed('deviation reason missing', inventory => { inventory.rows[0].deviations = [{ approval: 'review-1' }]; });
expectRed('deviation approval missing', inventory => { inventory.rows[0].deviations = [{ reason: 'Reviewed difference' }]; });
expectRed('stale reference hash', inventory => { inventory.rows[0].referenceSha256 = '0'.repeat(64); });

// Evidence probes: each removes exactly one real evidence item and must turn
// the --require-evidence check red, then green once restored. These run only
// when the baseline evidence is complete, so a pending row never masks a probe.
const repoRoot = join(here, '..', '..');
const evidenceBaseline = validateEvidence(original, repoRoot);
if (evidenceBaseline.length) {
  process.stdout.write(`SKIP: evidence probes (baseline evidence is not complete: ${evidenceBaseline[0]})\n`);
} else {
  function expectEvidenceRed(name, mutate) {
    const candidate = clone();
    mutate(candidate);
    const errors = validateEvidence(candidate, repoRoot);
    if (!errors.length) throw new Error(`${name}: evidence guard stayed green`);
    process.stdout.write(`RED: ${name} — ${errors[0]}\n`);
  }
  expectEvidenceRed('audit not complete', inventory => { inventory.rows[0].audit.status = 'pending'; inventory.rows[0].audit.reason = 'probe'; });
  expectEvidenceRed('evidence not complete', inventory => { inventory.rows[0].evidence.status = 'pending'; });
  expectEvidenceRed('source commit pending', inventory => { inventory.rows[0].sourceCommit = 'pending'; });
  expectEvidenceRed('capture tool pending', inventory => { inventory.rows[0].captureTool = 'pending'; });
  expectEvidenceRed('built capture hash stale', inventory => { inventory.rows[0].evidence.builtRawSha256 = createHash('sha256').update('probe').digest('hex'); });
  expectEvidenceRed('comparison hash stale', inventory => { inventory.rows[0].evidence.comparisonSha256 = '0'.repeat(64); });
  // A missing artifact on disk, restored afterwards.
  const diffPath = join(repoRoot, original.rows[0].evidence.diff);
  const parked = diffPath + '.probe';
  renameSync(diffPath, parked);
  try {
    const errors = validateEvidence(original, repoRoot);
    if (!errors.length) throw new Error('diff file missing: evidence guard stayed green');
    process.stdout.write(`RED: diff file missing — ${errors[0]}\n`);
  } finally {
    renameSync(parked, diffPath);
  }
  const auditPath = join(repoRoot, original.rows[0].audit.path);
  const parkedAudit = auditPath + '.probe';
  renameSync(auditPath, parkedAudit);
  try {
    const errors = validateEvidence(original, repoRoot);
    if (!errors.length) throw new Error('audit file missing: evidence guard stayed green');
    process.stdout.write(`RED: audit file missing — ${errors[0]}\n`);
  } finally {
    renameSync(parkedAudit, auditPath);
  }
  const restoredEvidence = validateEvidence(original, repoRoot);
  if (restoredEvidence.length) throw new Error(`restored evidence baseline is red: ${restoredEvidence.join('; ')}`);
  process.stdout.write('GREEN: restored evidence baseline after every negative probe\n');
}

const restored = validateStructure(original);
if (restored.length) throw new Error(`restored baseline is red: ${restored.join('; ')}`);
process.stdout.write('GREEN: restored baseline after every negative probe\n');
