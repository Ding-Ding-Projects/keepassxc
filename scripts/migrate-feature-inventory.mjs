// Explicit one-time migration. It preserves every original row and evidence link
// as history, never turns an existing source file into a current delivery claim.
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { join } from 'node:path';
import { execFileSync } from 'node:child_process';
import { FEATURE_CONTRACTS, PLATFORM_IDS, SURFACE_CONTRACTS } from './feature-inventory-contract.mjs';
import { repoRoot } from './check-feature-inventory.mjs';

const file = join(repoRoot, 'docs/features/inventory.json');
const inventory = JSON.parse(readFileSync(file, 'utf8'));
if (inventory.schemaVersion !== 1) throw new Error('Migration requires an untouched version 1 inventory; existing version 2 content is never overwritten.');
const destinations = ['surface-inventory.json', 'capability-inventory.json'].map(name => join(repoRoot, 'docs/features', name));
if (destinations.some(existsSync)) throw new Error('Refusing to overwrite an existing surface or capability inventory.');
const sourceCommit = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: repoRoot, encoding: 'utf8' }).trim();
const oldRows = structuredClone(inventory.rows);
inventory.schemaVersion = 2;
inventory.migratedFrom = 1;
inventory.assessmentSourceCommit = sourceCommit;
inventory.note = 'Platform summaries preserve historical source assessments, not current built-product proof. Named surfaces and capability evidence are checked independently. A schema pass is never product completion.';
inventory.rows = oldRows.map(row => ({
  ...row,
  status: row.status === 'implemented' ? 'partial' : row.status,
  priorStatus: row.status,
  evidenceState: 'historical-unverified',
}));
for (const platform of PLATFORM_IDS) for (const contract of FEATURE_CONTRACTS) {
  if (!inventory.rows.some(row => row.surface === platform && row.id === contract.id)) inventory.rows.push({
    id: contract.id, surface: platform, title: contract.id.replaceAll('-', ' '),
    status: 'unverified', evidenceState: 'unverified',
    note: 'Explicitly registered obligation. No current built-artifact interaction or capture accepted.',
  });
}
const surfaces = {
  schemaVersion: 1, assessmentSourceCommit: sourceCommit,
  note: 'Hand-maintained surface registry. Source presence is not implementation or visual verification. Every registered capability is required on each surface, with a documented site-owned equivalent when needed.',
  rows: SURFACE_CONTRACTS.map(surface => ({
    ...surface, ownership: surface.platform === 'site' ? 'visitor-site-state' : 'native-product',
    status: surface.source && existsSync(join(repoRoot, surface.source)) ? 'source-present' : 'unverified',
    evidenceState: 'unverified',
  })),
};
const capabilities = {
  schemaVersion: 1, assessmentSourceCommit: sourceCommit,
  note: 'Explicit canonical capability contracts. Evidence rows are sparse: an omitted surface/capability cell is incomplete, never exempt or inherited from another surface. No current built interaction evidence has been accepted.',
  contracts: FEATURE_CONTRACTS.flatMap(feature => feature.capabilities.map(capability => ({
    id: `${feature.id}/${capability}`, feature: feature.id, section: feature.section,
    persistenceRequired: feature.persistent,
  }))),
  rows: [],
};
writeFileSync(file, `${JSON.stringify(inventory, null, 2)}\n`);
writeFileSync(destinations[0], `${JSON.stringify(surfaces, null, 2)}\n`);
writeFileSync(destinations[1], `${JSON.stringify(capabilities, null, 2)}\n`);
process.stdout.write(`Migrated ${oldRows.length} historical rows; ${surfaces.rows.length} surfaces; ${capabilities.contracts.length} capability contracts. No current evidence claimed.\n`);
