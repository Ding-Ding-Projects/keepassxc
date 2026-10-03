// Evidence integrity does not by itself authenticate pixels or prove behavior.
import { readFileSync, lstatSync, realpathSync } from 'node:fs';
import { join, resolve, relative, sep, extname } from 'node:path';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { FEATURE_CONTRACTS, CANONICAL_FEATURES, PLATFORM_IDS, SURFACE_CONTRACTS, SURFACE_IDS, CAPABILITY_IDS } from './feature-inventory-contract.mjs';
export const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const plain = v => v !== null && typeof v === 'object' && !Array.isArray(v);
const hash = v => typeof v === 'string' && /^[a-f0-9]{64}$/.test(v);
const revision = v => typeof v === 'string' && /^[a-f0-9]{40}$/.test(v);
const nonempty = v => typeof v === 'string' && v.trim().length > 0 && v.length <= 2000;
const statuses = ['missing', 'partial', 'source-present', 'unverified', 'implemented'];
const byFeature = new Map(FEATURE_CONTRACTS.map(f => [f.id, f]));
const bySurface = new Map(SURFACE_CONTRACTS.map(s => [s.id, s]));
const capabilitySet = new Set(CAPABILITY_IDS);
const referenceRoles = {
  implementation: ['src', 'site'], localizedCopy: ['share', 'src', 'site'],
  article: ['docs'], test: ['tests', 'scripts', 'site'], persistence: ['src', 'site'],
  registration: ['src', 'site'], configuration: ['src', 'site', 'share'], interaction: ['design', 'docs'], capture: ['design', 'docs'],
  testResult: ['design', 'docs'],
};
const textExtensions = new Set(['.cpp', '.h', '.ui', '.qss', '.css', '.js', '.mjs', '.ts', '.json', '.md', '.html', '.txt']);

// Validate before opening. Never traverse arbitrary user-data or administrative
// paths. Reject links, alternate streams, encoded paths and sensitive file types.
export function safeEvidenceFile(root, file, role) {
  if (typeof file !== 'string' || file.length > 240 || !/^[a-zA-Z0-9_./-]+$/.test(file)) throw new Error('unsafe evidence path');
  const parts = file.split('/');
  if (parts.some(p => !p || p === '.' || p === '..' || p.startsWith('.') || /^(con|prn|aux|nul|com[0-9]|lpt[0-9])(?:\.|$)/i.test(p))) throw new Error('unsafe evidence path');
  if (!referenceRoles[role]?.includes(parts[0])) throw new Error('path outside role roots');
  if (role === 'capture' ? extname(file) !== '.png' : !textExtensions.has(extname(file))) throw new Error('unsupported evidence file type');
  const rootReal = realpathSync(root);
  let target = rootReal;
  for (const part of parts) {
    target = join(target, part);
    if (lstatSync(target).isSymbolicLink()) throw new Error('linked evidence path');
  }
  const rel = relative(rootReal, realpathSync(target));
  if (!rel || rel === '..' || rel.startsWith(`..${sep}`) || resolve(rootReal, rel) !== realpathSync(target)) throw new Error('evidence escaped root');
  const stat = lstatSync(target);
  if (!stat.isFile() || stat.nlink !== 1 || stat.size > (role === 'capture' ? 32 : 8) * 1024 * 1024) throw new Error('evidence is not a bounded ordinary file');
  return target;
}

function readReference(root, ref, role, symbol = false) {
  if (!plain(ref) || !hash(ref.sha256)) throw new Error(`${role} requires file and SHA-256`);
  const bytes = readFileSync(safeEvidenceFile(root, ref.file, role));
  if (sha256(bytes) !== ref.sha256) throw new Error(`${role} hash mismatch`);
  if (symbol) {
    if (!Number.isInteger(ref.line) || ref.line < 1 || !nonempty(ref.text)) throw new Error(`${role} requires an exact line and literal text`);
    const line = bytes.toString('utf8').replace(/\r\n/g, '\n').split('\n')[ref.line - 1];
    if (line !== ref.text || /^\s*(\/\/|\/\*|\*|<!--)/.test(line)) throw new Error(`${role} exact source boundary missing`);
  }
  return bytes;
}
function readJsonReference(root, ref, role) {
  const result = JSON.parse(readReference(root, ref, role));
  if (!plain(result)) throw new Error(`${role} must be an object`);
  return result;
}
function sameTuple(a, b) {
  return plain(a) && plain(b) && ['screen', 'state', 'theme', 'language', 'scale', 'width', 'height', 'viewport'].every(k => a[k] === b[k]);
}
function checkTuple(t) {
  return plain(t) && nonempty(t.screen) && nonempty(t.state)
    && ['light', 'dark'].includes(t.theme) && ['english', 'cantonese', 'bilingual'].includes(t.language)
    && [1, 1.25, 1.5, 2].includes(t.scale) && Number.isInteger(t.width) && t.width >= 320 && t.width <= 16384
    && Number.isInteger(t.height) && t.height >= 200 && t.height <= 16384;
}
function checkCapture(root, ref, tuple) {
  const bytes = readReference(root, ref, 'capture');
  if (bytes.length < 33 || bytes.subarray(0, 8).toString('hex') !== '89504e470d0a1a0a' || bytes.toString('ascii', 12, 16) !== 'IHDR') throw new Error('capture is not a PNG');
  if (bytes.readUInt32BE(16) !== Math.round(tuple.width * tuple.scale) || bytes.readUInt32BE(20) !== Math.round(tuple.height * tuple.scale)) throw new Error('capture pixel dimensions differ from tuple');
}

// The receipt is externally produced by the approved capture route and reviewed.
// Checking fields cannot establish that a self-written receipt is truthful.
export function validateEvidence(row, options = {}) {
  return validateEvidenceRecord(row, options);
}

function validateEvidenceRecord(row, options, matrixMember = false) {
  const errors = [];
  const root = options.root;
  const add = message => errors.push(`${row.surface}:${row.feature}/${row.capability}: ${message}`);
  const contract = byFeature.get(row.feature);
  const platform = bySurface.get(row.surface)?.platform;
  const proof = row.evidence;
  if (!plain(proof)) return [`${row.surface}:${row.feature}/${row.capability}: current evidence absent`];
  if (!revision(options.sourceCommit)) add('expected source revision absent');
  if (!hash(options.artifactHashes?.[platform])) add('expected built artifact hash absent');
  for (const role of ['implementation', 'localizedCopy', 'article', 'test', 'registration', 'configuration', ...(contract?.persistent ? ['persistence'] : [])]) {
    try {
      const bytes = readReference(root, proof[role], role, !['article', 'localizedCopy'].includes(role));
      if (!revision(options.sourceCommit)) throw new Error(`${role} source revision unavailable`);
      const key = `${options.sourceCommit}:${proof[role].file}`;
      let committed = options.revisionCache?.get(key);
      if (!committed) {
        committed = execFileSync('git', ['show', key], { cwd: root, maxBuffer: 8 * 1024 * 1024, stdio: ['ignore', 'pipe', 'pipe'] });
        options.revisionCache?.set(key, committed);
      }
      // Worktree line endings may differ from Git blobs. Content may not.
      if (committed.toString('utf8').replace(/\r\n/g, '\n') !== bytes.toString('utf8').replace(/\r\n/g, '\n')) throw new Error(`${role} source differs from pinned revision`);
    }
    catch (e) { add(e.message); }
  }
  if (!['english', 'cantonese', 'bilingual'].every(language => proof.languages?.includes(language))) add('localized language coverage missing');
  let interaction;
  try { interaction = readJsonReference(root, proof.interaction, 'interaction'); }
  catch (e) { add(e.message); return errors; }
  if (interaction.schemaVersion !== 1 || interaction.kind !== 'built-ui-interaction') add('interaction schema or provenance kind unsupported');
  if (interaction.sourceCommit !== options.sourceCommit || !revision(interaction.sourceCommit)) add('stale interaction source revision');
  if (interaction.artifactSha256 !== options.artifactHashes?.[platform] || !hash(interaction.artifactSha256)) add('interaction artifact hash mismatch');
  const claims = interaction.claims;
  if (claims !== undefined && (!Array.isArray(claims) || !claims.length || claims.length > CAPABILITY_IDS.length
    || claims.some(claim => !plain(claim) || !capabilitySet.has(`${claim.feature}/${claim.capability}`))
    || new Set(claims.map(claim => `${claim.feature}/${claim.capability}`)).size !== claims.length)) add('interaction capability claims malformed');
  const directlyClaimed = interaction.feature === row.feature && interaction.capability === row.capability;
  const additionallyClaimed = Array.isArray(claims) && claims.some(claim => plain(claim) && claim.feature === row.feature && claim.capability === row.capability);
  if (interaction.surface !== row.surface || (!directlyClaimed && !additionallyClaimed)) add('interaction belongs to another inventory cell');
  if (!checkTuple(interaction.tuple) || interaction.tuple?.screen !== row.surface) add('interaction tuple incomplete or wrong surface');
  const tuple = interaction.tuple;
  if (row.feature === 'language-modes' || row.feature === 'clipping-matrix') {
    if (['english', 'cantonese', 'bilingual'].includes(row.capability) && tuple?.language !== row.capability) add('capability tuple language contradicts claimed mode');
  }
  if (row.feature === 'clipping-matrix') {
    if (['light', 'dark'].includes(row.capability) && tuple?.theme !== row.capability) add('capability tuple theme contradicts claimed mode');
    const expectedScale = { 'scale-100': 1, 'scale-125': 1.25, 'scale-150': 1.5, 'scale-200': 2 }[row.capability];
    if (expectedScale && tuple?.scale !== expectedScale) add('capability tuple scale contradicts claimed mode');
    if (row.capability === 'normal-minimum' && !['normal', 'minimum'].includes(tuple?.viewport)) add('capability tuple viewport must name normal or minimum');
  }
  if (interaction.method !== 'lowlevel-headless-built-artifact' || interaction.hiddenDesktop !== true || interaction.fixture !== 'isolated-nonpersonal' || interaction.synthetic !== false || interaction.injected !== false) add('genuine isolated capture provenance missing');
  if (interaction.privacy?.verdict !== 'reviewed-safe' || interaction.privacy?.privateDataPresent !== false || !nonempty(interaction.privacy?.reviewer) || !nonempty(interaction.privacy?.reviewedAt)) add('privacy review absent');
  if (interaction.inspection?.verdict !== 'reviewed' || interaction.inspection?.allFramesOpened !== true || !nonempty(interaction.inspection?.reviewer)) add('visual inspection absent');
  if (!Array.isArray(interaction.steps) || !interaction.steps.length || interaction.steps.length > 10000) { add('interaction steps absent or unbounded'); return errors; }
  for (let i = 0; i < interaction.steps.length; i++) {
    const step = interaction.steps[i];
    if (!plain(step)) { add(`step ${i + 1} malformed`); continue; }
    if (step.ordinal !== i + 1 || !['pointer', 'keyboard', 'touch', 'assistive'].includes(step.input)
      || !nonempty(step.target) || !nonempty(step.accessibleName) || !nonempty(step.action)
      || !nonempty(step.before) || !nonempty(step.expectedAfter) || step.after !== step.expectedAfter || step.verdict !== 'passed') add(`step ${i + 1} semantic interaction incomplete`);
    if (!sameTuple(step.tuple, interaction.tuple)) add(`step ${i + 1} tuple differs`);
    try { if (checkTuple(interaction.tuple)) checkCapture(root, step.capture, interaction.tuple); else add(`step ${i + 1} capture tuple unavailable`); }
    catch (e) { add(`step ${i + 1}: ${e.message}`); }
  }
  const last = interaction.steps.at(-1);
  if (!plain(proof.capture) || proof.capture.file !== last?.capture?.file || proof.capture.sha256 !== last?.capture?.sha256) add('final capture does not match final interaction');
  try {
    const result = readJsonReference(root, proof.testResult, 'testResult');
    if (result.schemaVersion !== 1 || result.kind !== 'focused-test-result' || result.status !== 'passed'
      || result.sourceCommit !== options.sourceCommit || result.artifactSha256 !== options.artifactHashes?.[platform]
      || result.testFile !== proof.test?.file || result.testFileSha256 !== proof.test?.sha256
      || !Number.isInteger(result.passed) || result.passed < 1 || result.failed !== 0 || !nonempty(result.command)
      || result.negativeRegression?.failedWhenBroken !== true || result.negativeRegression?.passedWhenRestored !== true) add('current focused red-then-green test result absent');
  } catch (e) { add(e.message); }
  if (row.feature === 'clipping-matrix' && row.capability === 'normal-minimum' && !matrixMember) {
    const members = proof.matrix ?? [];
    if (!Array.isArray(members) || members.length > 47) add('layout matrix must contain at most 47 additional interactions');
    const tuples = [interaction.tuple];
    if (Array.isArray(members) && members.length <= 47) for (let i = 0; i < members.length; i++) {
      const member = members[i];
      if (!plain(member)) { add(`layout matrix member ${i + 1} malformed`); continue; }
      // Reuse every provenance, source, test, privacy and per-click check. A
      // matrix entry never gets a lighter acceptance path than the primary one.
      const memberRow = { ...row, evidence: { ...proof, interaction: member.interaction, capture: member.capture } };
      errors.push(...validateEvidenceRecord(memberRow, options, true));
      try { tuples.push(readJsonReference(root, member.interaction, 'interaction').tuple); }
      catch (e) { add(`layout matrix member ${i + 1}: ${e.message}`); }
    }
    const covered = new Set(), sizes = new Map();
    for (const item of tuples) {
      if (!checkTuple(item) || !['normal', 'minimum'].includes(item.viewport)) continue;
      const key = `${item.language}/${item.theme}/${item.scale}/${item.viewport}`;
      if (covered.has(key)) add(`duplicate layout tuple: ${key}`);
      covered.add(key);
      const size = `${item.width}x${item.height}`;
      if (sizes.has(item.viewport) && sizes.get(item.viewport) !== size) add(`layout viewport dimensions differ across matrix: ${item.viewport}`);
      sizes.set(item.viewport, size);
    }
    for (const language of ['english', 'cantonese', 'bilingual']) for (const theme of ['light', 'dark'])
      for (const scale of [1, 1.25, 1.5, 2]) for (const viewport of ['normal', 'minimum']) {
        const key = `${language}/${theme}/${scale}/${viewport}`;
        if (!covered.has(key)) add(`required layout tuple missing: ${key}`);
      }
    const normal = tuples.find(item => item?.viewport === 'normal');
    const minimum = tuples.find(item => item?.viewport === 'minimum');
    if (normal && minimum && (minimum.width > normal.width || minimum.height > normal.height
      || (minimum.width === normal.width && minimum.height === normal.height))) add('minimum viewport must be smaller than normal viewport');
  }
  return errors;
}

// Load legacy content for historical inspection without silently upgrading proof.
export function migrateInventory(input) {
  if (input?.schemaVersion !== 1) return structuredClone(input);
  return {
    ...structuredClone(input), schemaVersion: 2, migrationRequired: true, migratedFrom: 1,
    rows: (Array.isArray(input.rows) ? input.rows : []).map(row => plain(row) ? {
      ...row, priorStatus: row.status, status: row.status === 'implemented' ? 'partial' : row.status,
      evidenceState: 'historical-unverified',
    } : row),
  };
}
function indexed(rows, key, problems, label) {
  const map = new Map();
  if (!Array.isArray(rows)) { problems.push(`${label}: rows must be an array`); return map; }
  for (const row of rows) {
    if (!plain(row)) { problems.push(`${label}: row must be an object`); continue; }
    const id = key(row);
    if (map.has(id)) problems.push(`${label}: duplicate ${id}`);
    else map.set(id, row);
  }
  return map;
}
export function validateSchema(bundle) {
  const errors = [];
  if (!plain(bundle)) return ['bundle must be an object'];
  const { inventory, surfaces, capabilities } = bundle;
  if (inventory?.schemaVersion !== 2 || inventory?.migrationRequired) errors.push('inventory: schema 2 required; legacy migration needs explicit review');
  if (surfaces?.schemaVersion !== 1) errors.push('surfaces: schema 1 required');
  if (capabilities?.schemaVersion !== 1) errors.push('capabilities: schema 1 required');
  const features = indexed(inventory?.rows, r => `${r.surface}:${r.id}`, errors, 'features');
  for (const row of features.values()) {
    if (!PLATFORM_IDS.includes(row.surface) || !CANONICAL_FEATURES.includes(row.id)) errors.push(`features: unknown ${row.surface}:${row.id}`);
    if (!statuses.includes(row.status) || !nonempty(row.title)) errors.push(`features: invalid status/title ${row.surface}:${row.id}`);
  }
  for (const platform of PLATFORM_IDS) for (const id of CANONICAL_FEATURES) if (!features.has(`${platform}:${id}`)) errors.push(`features: required row absent ${platform}:${id}`);
  const registered = indexed(surfaces?.rows, r => r.id, errors, 'surfaces');
  for (const row of registered.values()) {
    const expected = bySurface.get(row.id);
    if (!expected || row.platform !== expected.platform) errors.push(`surfaces: unknown identity ${row.id}`);
    if (!nonempty(row.label) || !statuses.includes(row.status)) errors.push(`surfaces: invalid label/status ${row.id}`);
    if (row.platform === 'site' && row.ownership !== 'visitor-site-state') errors.push(`surfaces: website ownership boundary missing ${row.id}`);
    if (row.platform === 'app' && row.ownership !== 'native-product') errors.push(`surfaces: native ownership boundary missing ${row.id}`);
  }
  for (const id of SURFACE_IDS) if (!registered.has(id)) errors.push(`surfaces: required row absent ${id}`);
  const contracts = indexed(capabilities?.contracts, r => r.id, errors, 'capabilities');
  for (const row of contracts.values()) {
    const spec = byFeature.get(row.feature);
    if (!capabilitySet.has(row.id) || !spec || row.id.split('/')[0] !== row.feature) errors.push(`capabilities: unknown identity ${row.id}`);
    if (row.persistenceRequired !== spec?.persistent) errors.push(`capabilities: persistence requirement differs ${row.id}`);
  }
  for (const id of CAPABILITY_IDS) if (!contracts.has(id)) errors.push(`capabilities: required contract absent ${id}`);
  const proofs = indexed(capabilities?.rows, r => `${r.surface}:${r.feature}/${r.capability}`, errors, 'proofs');
  for (const row of proofs.values()) {
    if (!bySurface.has(row.surface) || !capabilitySet.has(`${row.feature}/${row.capability}`)) errors.push(`proofs: unknown cell ${row.surface}:${row.feature}/${row.capability}`);
    if (!statuses.includes(row.status)) errors.push(`proofs: invalid status ${row.surface}:${row.feature}/${row.capability}`);
  }
  return errors;
}
export function evaluateBundle(bundle, options = {}) {
  options = { ...options, revisionCache: new Map() };
  const schemaErrors = validateSchema(bundle);
  const statusErrors = [];
  for (const row of Array.isArray(bundle?.inventory?.rows) ? bundle.inventory.rows.filter(plain) : []) {
    if (row.status !== 'implemented') statusErrors.push(`${row.surface}:${row.id}: summary remains ${row.status}`);
  }
  for (const row of Array.isArray(bundle?.surfaces?.rows) ? bundle.surfaces.rows.filter(plain) : []) {
    if (row.status !== 'implemented') statusErrors.push(`${row.id}: surface remains ${row.status}`);
  }
  const evidenceErrors = [];
  const proofRows = Array.isArray(bundle?.capabilities?.rows) ? bundle.capabilities.rows.filter(plain) : [];
  const verified = new Set();
  for (const row of proofRows) {
    const errors = validateEvidence(row, options);
    evidenceErrors.push(...errors);
    if (row.status === 'implemented' && !errors.length) verified.add(`${row.surface}:${row.feature}/${row.capability}`);
  }
  const incomplete = [];
  for (const surface of SURFACE_CONTRACTS) for (const feature of FEATURE_CONTRACTS) {
    const missing = feature.capabilities.filter(capability => !verified.has(`${surface.id}:${feature.id}/${capability}`));
    if (missing.length) incomplete.push({ surface: surface.id, feature: feature.id, missing });
  }
  return {
    schemaErrors, statusErrors, evidenceErrors, incomplete, schemaValid: schemaErrors.length === 0,
    productComplete: schemaErrors.length === 0 && statusErrors.length === 0 && evidenceErrors.length === 0 && incomplete.length === 0,
    verifiedCells: verified.size, requiredCells: SURFACE_IDS.length * CAPABILITY_IDS.length,
  };
}
