import { createHash } from 'node:crypto';
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { dirname, extname, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const designRoot = resolve(here, '..');
const inventoryPath = join(here, 'inventory.json');
const expectedReferences = [
  'Appearance.dc.html', 'Changelog.dc.html', 'History.dc.html', 'KeePassXC Material.dc.html',
  'RegexBuilder.dc.html', 'Reports.dc.html', 'Settings.dc.html', 'Sheet.dc.html', 'Vault.dc.html'
];
const requiredTupleFields = ['screen', 'state', 'theme', 'viewport', 'scale', 'locale'];
const requiredDeterminismFields = ['fixture', 'time', 'motion', 'random', 'font', 'network', 'languageMode'];
const requiredEvidenceFields = ['referenceRaw', 'referenceRawSha256', 'builtRaw', 'builtRawSha256', 'comparison', 'comparisonSha256', 'diff', 'diffSha256'];

function digest(path) {
  return createHash('sha256').update(readFileSync(path)).digest('hex');
}

function requireText(errors, value, label) {
  if (typeof value !== 'string' || value.trim() === '') errors.push(`${label} must be a non-empty string`);
}

export function validateStructure(inventory, root = designRoot) {
  const errors = [];
  if (inventory.schemaVersion !== 1) errors.push('schemaVersion must equal 1');
  if (!Array.isArray(inventory.rows)) return ['rows must be an array'];

  const ids = new Set();
  const referenceCounts = new Map(expectedReferences.map(file => [file, 0]));
  for (const [index, row] of inventory.rows.entries()) {
    const at = `rows[${index}]`;
    requireText(errors, row.id, `${at}.id`);
    if (ids.has(row.id)) errors.push(`${at}.id duplicates ${row.id}`);
    ids.add(row.id);
    requireText(errors, row.referenceFile, `${at}.referenceFile`);
    if (!referenceCounts.has(row.referenceFile)) errors.push(`${at}.referenceFile is not a canonical reference`);
    else referenceCounts.set(row.referenceFile, referenceCounts.get(row.referenceFile) + 1);
    requireText(errors, row.referenceSha256, `${at}.referenceSha256`);
    requireText(errors, row.referenceRoute, `${at}.referenceRoute`);
    requireText(errors, row.builtRoute, `${at}.builtRoute`);
    requireText(errors, row.screen, `${at}.screen`);
    requireText(errors, row.state, `${at}.state`);
    if (!row.tuple || typeof row.tuple !== 'object') errors.push(`${at}.tuple must be an object`);
    else {
      for (const field of requiredTupleFields) if (!(field in row.tuple)) errors.push(`${at}.tuple.${field} is missing`);
      if (row.tuple.screen !== row.screen) errors.push(`${at}.tuple.screen must equal screen`);
      if (row.tuple.state !== row.state) errors.push(`${at}.tuple.state must equal state`);
      if (!Number.isInteger(row.tuple.viewport?.width) || row.tuple.viewport.width <= 0) errors.push(`${at}.tuple.viewport.width must be a positive integer`);
      if (!Number.isInteger(row.tuple.viewport?.height) || row.tuple.viewport.height <= 0) errors.push(`${at}.tuple.viewport.height must be a positive integer`);
      if (typeof row.tuple.scale !== 'number' || row.tuple.scale <= 0) errors.push(`${at}.tuple.scale must be positive`);
    }
    if (!row.determinism || typeof row.determinism !== 'object') errors.push(`${at}.determinism must be an object`);
    else for (const field of requiredDeterminismFields) requireText(errors, row.determinism[field], `${at}.determinism.${field}`);
    if (!row.audit || typeof row.audit !== 'object') errors.push(`${at}.audit must be an object`);
    else {
      requireText(errors, row.audit.status, `${at}.audit.status`);
      requireText(errors, row.audit.path, `${at}.audit.path`);
      if (row.audit.status === 'pending') requireText(errors, row.audit.reason, `${at}.audit.reason`);
    }
    if (!row.evidence || typeof row.evidence !== 'object') errors.push(`${at}.evidence must be an object`);
    else {
      requireText(errors, row.evidence.status, `${at}.evidence.status`);
      for (const field of requiredEvidenceFields) requireText(errors, row.evidence[field], `${at}.evidence.${field}`);
    }
    if (!Array.isArray(row.deviations)) errors.push(`${at}.deviations must be an array`);
    else for (const [deviationIndex, deviation] of row.deviations.entries()) {
      requireText(errors, deviation.reason, `${at}.deviations[${deviationIndex}].reason`);
      requireText(errors, deviation.approval, `${at}.deviations[${deviationIndex}].approval`);
    }
    requireText(errors, row.sourceCommit, `${at}.sourceCommit`);
    requireText(errors, row.captureTool, `${at}.captureTool`);

    const source = resolve(root, row.referenceFile || '');
    if (!existsSync(source)) errors.push(`${at}.referenceFile does not exist`);
    else if (digest(source) !== row.referenceSha256) errors.push(`${at}.referenceSha256 is stale`);
    const expectedRoute = `/reference/${row.id}`;
    if (row.referenceRoute !== expectedRoute) errors.push(`${at}.referenceRoute must equal ${expectedRoute}`);
  }
  for (const [file, count] of referenceCounts) if (count !== 1) errors.push(`${file} must appear exactly once; found ${count}`);

  const discovered = readdirSync(root, { withFileTypes: true })
    .filter(entry => entry.isFile() && entry.name.endsWith('.dc.html'))
    .map(entry => entry.name)
    .sort();
  const expected = [...expectedReferences].sort();
  if (JSON.stringify(discovered) !== JSON.stringify(expected)) {
    errors.push(`checked-in reference set differs from the hand-written source set: ${discovered.join(', ')}`);
  }
  return errors;
}

export function validateEvidence(inventory, repositoryRoot) {
  const errors = [];
  for (const row of inventory.rows) {
    const at = row.id;
    if (row.audit.status !== 'complete') errors.push(`${at}: Material Design 3 audit is not complete`);
    if (row.evidence.status !== 'complete') errors.push(`${at}: capture evidence is not complete`);
    if (row.sourceCommit === 'pending') errors.push(`${at}: source commit is pending`);
    if (row.captureTool === 'pending') errors.push(`${at}: capture tool provenance is pending`);
    for (const pathField of ['referenceRaw', 'builtRaw', 'comparison', 'diff']) {
      const hashField = `${pathField}Sha256`;
      const path = resolve(repositoryRoot, row.evidence[pathField]);
      if (!existsSync(path)) errors.push(`${at}: ${pathField} is missing`);
      else if (row.evidence[hashField] === 'pending' || digest(path) !== row.evidence[hashField]) errors.push(`${at}: ${hashField} is missing or stale`);
    }
    const auditPath = resolve(repositoryRoot, row.audit.path);
    if (!existsSync(auditPath)) errors.push(`${at}: audit file is missing`);
  }
  return errors;
}

function main() {
  const inventory = JSON.parse(readFileSync(inventoryPath, 'utf8'));
  const repositoryRoot = resolve(designRoot, '..');
  const structureErrors = validateStructure(inventory);
  const evidenceErrors = process.argv.includes('--require-evidence') ? validateEvidence(inventory, repositoryRoot) : [];
  const errors = [...structureErrors, ...evidenceErrors];
  if (errors.length) {
    for (const error of errors) process.stderr.write(`ERROR: ${error}\n`);
    process.exitCode = 1;
  } else {
    process.stdout.write(`PASS: ${inventory.rows.length} design reference rows are structurally complete and source-current${evidenceErrors.length ? '' : process.argv.includes('--require-evidence') ? ' with evidence' : ''}.\n`);
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
