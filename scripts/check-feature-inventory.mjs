// Schema success and complete product delivery are deliberately separate verdicts.
import { readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';
import { CANONICAL_FEATURES, PLATFORM_IDS, SURFACE_IDS, CAPABILITY_IDS } from './feature-inventory-contract.mjs';
import { migrateInventory, evaluateBundle, validateSchema, validateEvidence } from './feature-evidence.mjs';
export { CANONICAL_FEATURES, SURFACE_IDS, CAPABILITY_IDS, evaluateBundle, validateSchema, validateEvidence, migrateInventory };
export const SURFACES = PLATFORM_IDS;
export const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
export const inventoryPath = join(repoRoot, 'docs/features/inventory.json');
export function loadInventory(root = repoRoot) {
  return migrateInventory(JSON.parse(readFileSync(join(root, 'docs/features/inventory.json'), 'utf8')));
}
export function loadBundle(root = repoRoot) {
  return {
    inventory: loadInventory(root),
    surfaces: JSON.parse(readFileSync(join(root, 'docs/features/surface-inventory.json'), 'utf8')),
    capabilities: JSON.parse(readFileSync(join(root, 'docs/features/capability-inventory.json'), 'utf8')),
  };
}
// Compatibility is fail-closed: the old two-platform file cannot establish the
// complete named-surface product. Call evaluateBundle for structured findings.
export function validateInventory(inventory, options = {}) {
  const bundle = options.bundle ?? {
    inventory, surfaces: { schemaVersion: 1, rows: [] },
    capabilities: { schemaVersion: 1, contracts: [], rows: [] },
  };
  const result = evaluateBundle(bundle, { root: repoRoot, ...options });
  return [...result.schemaErrors, ...result.statusErrors, ...result.evidenceErrors,
    ...result.incomplete.map(r => `${r.surface}:${r.feature}: ${r.missing.length} current capability proofs absent`)];
}
if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const args = process.argv.slice(2);
    const sourceCommit = args.find(a => a.startsWith('--source='))?.slice(9)
      ?? execFileSync('git', ['rev-parse', 'HEAD'], { cwd: repoRoot, encoding: 'utf8' }).trim();
    const artifactHashes = Object.fromEntries(PLATFORM_IDS.map(p => [p, args.find(a => a.startsWith(`--${p}-artifact=`))?.split('=')[1]]));
    const result = evaluateBundle(loadBundle(), { root: repoRoot, sourceCommit, artifactHashes });
    const schemaOnly = args.includes('--schema-only');
    if (!args.includes('--summary')) {
      const findings = [...result.schemaErrors, ...(schemaOnly ? [] : result.statusErrors), ...(schemaOnly ? [] : result.evidenceErrors),
        ...(schemaOnly ? [] : result.incomplete.map(r => `${r.surface}:${r.feature}: ${r.missing.length} current capability proofs absent`))];
      for (const problem of findings.slice(0, 30)) process.stdout.write(`RED ${problem}\n`);
      if (findings.length > 30) process.stdout.write(`${findings.length - 30} additional findings; use the exported evaluator for the complete structured result.\n`);
    }
    process.stdout.write(`Schema: ${result.schemaValid ? 'PASS' : 'FAIL'} (${result.schemaErrors.length} findings). Product: ${result.productComplete ? 'PASS' : 'INCOMPLETE'}; ${result.verifiedCells}/${result.requiredCells} current capability cells verified; ${result.incomplete.length} incomplete surface-feature rows; ${result.evidenceErrors.length} invalid evidence findings.\n`);
    process.exit(schemaOnly ? (result.schemaValid ? 0 : 1) : (result.productComplete ? 0 : 1));
  } catch (e) { process.stderr.write(`RED inventory load/validation failed: ${e.message}\n`); process.exit(1); }
}
