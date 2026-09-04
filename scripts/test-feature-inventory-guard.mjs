// Negative regression for the feature inventory check. Each probe removes or
// corrupts exactly one item in a copy of the inventory or a temporary source
// fixture and must turn the check red; the baseline must stay as it is.
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, relative } from 'node:path';
import { validateInventory, loadInventory, CANONICAL_FEATURES, repoRoot } from './check-feature-inventory.mjs';

const original = loadInventory();
const baseline = validateInventory(original);
const baselineKeys = new Set(baseline.map(e => e));
const clone = () => JSON.parse(JSON.stringify(original));

function expectNewRed(name, mutate) {
  const candidate = clone();
  mutate(candidate);
  const errors = validateInventory(candidate).filter(e => !baselineKeys.has(e));
  if (!errors.length) throw new Error(`${name}: guard stayed green`);
  process.stdout.write(`RED: ${name} — ${errors[0]}\n`);
}

// Any row with an anchored implementation will do; a partial row is red for its
// status already, so each probe must add a NEW finding to count.
const implemented = original.rows.find(r => r.implementation && r.implementation.anchor && !baseline.some(e => e.includes('implementation anchor not found') && e.startsWith(`${r.surface}:${r.id}:`)));
if (!implemented) throw new Error('guard needs at least one row with a resolving anchored implementation');

expectNewRed('canonical row removed', inv => { inv.rows = inv.rows.filter(r => r !== inv.rows.find(x => x.id === implemented.id && x.surface === implemented.surface)); });
expectNewRed('row status downgraded', inv => { const r = inv.rows.find(x => x.id === implemented.id && x.surface === implemented.surface); r.status = 'missing'; r.note = 'probe'; });
expectNewRed('implementation anchor renamed', inv => { const r = inv.rows.find(x => x.id === implemented.id && x.surface === implemented.surface); r.implementation.anchor = r.implementation.anchor + 'Renamed'; });
expectNewRed('implementation file missing', inv => { const r = inv.rows.find(x => x.id === implemented.id && x.surface === implemented.surface); r.implementation.file = r.implementation.file + '.gone'; });
expectNewRed('canonical row duplicated', inv => { inv.rows.push(JSON.parse(JSON.stringify(implemented))); });
expectNewRed('non-canonical id added', inv => { inv.rows.push({ id: 'not-a-feature', surface: 'app', title: 'x', status: 'implemented' }); });
expectNewRed('malformed row added', inv => { inv.rows.push(null); });

// A commented-out symbol must not satisfy a line-anchored implementation anchor.
const scratch = mkdtempSync(join(tmpdir(), 'kpxc-inventory-guard-'));
try {
  const fixture = join(scratch, 'fixture.h');
  const relFixture = relative(repoRoot, fixture).split('\\').join('/');
  writeFileSync(fixture, '// class RegexBuilder : public Overlay\n');
  expectNewRed('commented-out symbol', inv => {
    const r = inv.rows.find(x => x.id === implemented.id && x.surface === implemented.surface);
    r.implementation = { file: relFixture, anchor: '^\\s*class RegexBuilder : public Overlay' };
  });
  writeFileSync(fixture, '    class RegexBuilderRenamed : public Overlay\n');
  expectNewRed('renamed symbol containing the old name', inv => {
    const r = inv.rows.find(x => x.id === implemented.id && x.surface === implemented.surface);
    r.implementation = { file: relFixture, anchor: '^\\s*class RegexBuilder : public Overlay' };
  });
} finally {
  rmSync(scratch, { recursive: true, force: true });
}

// The canonical list itself must be hand-written and non-empty.
if (CANONICAL_FEATURES.length < 60) throw new Error('canonical feature list is shorter than the shared contract');
process.stdout.write(`GREEN: ${CANONICAL_FEATURES.length} canonical features, baseline unchanged (${baseline.length} open finding(s) remain red until implemented)\n`);
