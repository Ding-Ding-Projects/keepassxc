// Contract tests use a disposable Git repository and simulated receipts only.
// An existing historical PNG is copied without modification for byte validation.
// These fixtures are never promoted as real interaction or current UI evidence.
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync, realpathSync, symlinkSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, dirname, relative } from 'node:path';
import { execFileSync, spawnSync } from 'node:child_process';
import { loadBundle, repoRoot, evaluateBundle, validateSchema, validateEvidence, migrateInventory } from './check-feature-inventory.mjs';
import { sha256 } from './feature-evidence.mjs';
import { FEATURE_CONTRACTS, SURFACE_CONTRACTS, CAPABILITY_IDS } from './feature-inventory-contract.mjs';

let passed = 0, failed = 0;
function check(name, body) {
  try { body(); passed++; process.stdout.write(`PASS ${name}\n`); }
  catch (error) { failed++; process.stderr.write(`FAIL ${name}: ${error.message}\n`); }
}
const baseline = loadBundle();
check('complete registries have valid schema but incomplete product', () => {
  const result = evaluateBundle(baseline, { root: repoRoot });
  assert.deepEqual(result.schemaErrors, []);
  assert.equal(result.productComplete, false);
  assert.equal(result.verifiedCells, 0);
  assert.equal(result.requiredCells, SURFACE_CONTRACTS.length * CAPABILITY_IDS.length);
});

// Independent examples pin the required behaviors, not only registry length.
const requiredCapabilities = [
  'dim-sum-surprise/one-percent-per-launch-draw', 'dim-sum-surprise/persisted-off-switch',
  'dim-sum-surprise/at-most-once-per-launch', 'dim-sum-surprise/no-mid-task-flow',
  'school-mode/live-propagation', 'school-mode/rename-everywhere',
  'narrator-voice-pickers/stable-identity', 'scheduled-settings/dst',
  'regex-builder/bounded-trace', 'regex-builder/conditionals-subroutines',
  'search-bar-every-surface/isolated-history-snippets',
  'context-menu-search/states-pseudo-states', 'context-menu-search/no-short-menu-exemption',
  'per-element-appearance-editor/channels', 'per-element-appearance-editor/adjustment-layers',
  'per-element-appearance-editor/embedded-smart-content', 'per-element-appearance-editor/warp',
  'per-element-appearance-editor/state-overrides', 'per-element-appearance-editor/self-editing',
  'toy-locks/pin', 'toy-locks/password', 'toy-locks/pin-password', 'toy-locks/password-totp',
  'toy-locks/pin-totp', 'toy-locks/password-pin-totp', 'toy-locks/disabled-wrapper',
  'toy-locks/keypad', 'toy-locks/manual-pin', 'toy-locks/shared-validator', 'toy-locks/shared-attempt-budget',
  'toy-locks/independent-credential-set', 'toy-locks/shortcut-block',
  'authenticator/rfc6238-vectors', 'qr-pairing/confirm-current-code',
  'secret-mutation-history/encrypted-snapshot', 'secret-mutation-history/password-protected-manager',
  'file-converter/documents-pdf', 'file-converter/images', 'file-converter/audio', 'file-converter/video',
  'file-converter/archives', 'file-converter/structured-data-spreadsheets', 'file-converter/code-text',
  'file-converter/binary-encodings', 'file-converter/unlimited-paged-queue', 'file-converter/bundled-offline-proof',
  'file-converter/pdf-postwrite-reopen', 'ollama-manager/exhaustive-catalog', 'ollama-manager/all-tags',
  'ollama-manager/all-pages', 'ollama-manager/unknown-not-zero', 'ollama-manager/streaming-chat',
  'ollama-manager/failed-launch-rollback', 'ollama-manager/no-arbitrary-shell',
  'tab-searches/current-strip', 'tab-searches/each-group', 'tab-searches/group-names',
  'tab-searches/all-owned-windows', 'in-app-version-provenance/front-screen-before-navigation',
  'status-hub/verified-reply-route', 'element-motion/every-rendered-element',
];
// Independently transcribed from concrete addPage/addSettingsPage registrations.
// This list must not be generated from the production registry being tested.
check('every registered settings and editor destination has its own obligation', () => {
  const expected = [
    'app.settings.general', 'app.settings.security', 'app.settings.browser',
    'app.settings.shortcuts', 'app.settings.ssh-agent', 'app.settings.keeshare',
    'app.settings.appearance', 'app.database.general', 'app.database.security',
    'app.database.credentials', 'app.database.encryption', 'app.database.remote-sync',
    'app.database.browser', 'app.database.keeshare', 'app.database.maintenance',
    'app.entry.main', 'app.entry.attributes', 'app.entry.icons', 'app.entry.autotype',
    'app.entry.browser', 'app.entry.ssh', 'app.entry.properties', 'app.entry.history',
    'app.group.main', 'app.group.icon', 'app.group.properties', 'app.group.browser', 'app.group.keeshare',
  ];
  for (const id of expected) {
    assert(SURFACE_CONTRACTS.some(surface => surface.id === id), `missing independently registered destination ${id}`);
    assert(baseline.surfaces.rows.some(surface => surface.id === id), `missing inventory destination ${id}`);
  }
});
check('handwritten acceptance examples remain registered', () => {
  for (const id of requiredCapabilities) assert(CAPABILITY_IDS.includes(id), `missing required example ${id}`);
  for (const id of ['app.database.credentials', 'app.entry.attributes', 'app.menus.text-editing',
    'app.passkey.clipboard', 'app.attachments.preview', 'app.extension.start', 'app.extension.progress',
    'app.extension.complete', 'site.overview', 'site.downloads', 'site.docs', 'site.changelog',
    'site.settings', 'site.tabs', 'site.dialogs']) assert(SURFACE_CONTRACTS.some(s => s.id === id), id);
});
for (const [property, collection] of [['inventory', 'rows'], ['surfaces', 'rows'], ['capabilities', 'contracts']]) {
  check(`each ${property} registration fails when removed and passes when restored`, () => {
    const list = baseline[property][collection];
    for (let i = 0; i < list.length; i++) {
      const candidate = { ...baseline, [property]: { ...baseline[property], [collection]: list.filter((_, index) => index !== i) } };
      assert(validateSchema(candidate).some(e => e.includes('required') && e.includes('absent')), `removal ${property} ${i}`);
    }
    assert.deepEqual(validateSchema(baseline), []);
  });
}
function schemaMutation(name, mutate, expected) {
  check(name, () => {
    const candidate = structuredClone(baseline); mutate(candidate);
    assert(validateSchema(candidate).some(e => e.includes(expected)), expected);
    assert.deepEqual(validateSchema(baseline), []);
  });
}
schemaMutation('duplicate feature', b => b.inventory.rows.push(b.inventory.rows[0]), 'duplicate');
schemaMutation('duplicate surface', b => b.surfaces.rows.push(b.surfaces.rows[0]), 'duplicate');
schemaMutation('duplicate capability', b => b.capabilities.contracts.push(b.capabilities.contracts[0]), 'duplicate');
schemaMutation('unknown feature', b => b.inventory.rows[0].id = 'unknown-feature', 'unknown');
schemaMutation('unknown surface', b => b.surfaces.rows[0].id = 'app.unknown', 'unknown');
schemaMutation('unknown capability', b => b.capabilities.contracts[0].id = 'language-modes/unknown', 'unknown');
schemaMutation('malformed row', b => b.inventory.rows.push(null), 'object');
schemaMutation('missing persistence contract', b => b.capabilities.contracts[0].persistenceRequired = false, 'persistence');
schemaMutation('website cannot claim native product ownership', b => b.surfaces.rows.find(s => s.platform === 'site').ownership = 'native-product', 'ownership');
schemaMutation('legacy version remains red', b => b.inventory.schemaVersion = 1, 'schema 2');
check('legacy loader retains every row and link without upgrading proof', () => {
  const row = { id: 'language-modes', surface: 'app', title: 'Historical', status: 'implemented', capture: { file: 'design/old.png' } };
  const migrated = migrateInventory({ schemaVersion: 1, rows: [row] });
  assert.equal(migrated.rows.length, 1);
  assert.deepEqual(migrated.rows[0].capture, row.capture);
  assert.equal(migrated.rows[0].priorStatus, 'implemented');
  assert.equal(migrated.rows[0].status, 'partial');
  assert.equal(migrated.migrationRequired, true);
  assert.equal(row.status, 'implemented');
});

const root = mkdtempSync(join(tmpdir(), 'keepassxc-inventory-'));
const expectedRoot = realpathSync(root);
try {
  const sourceFiles = {
    'src/feature.cpp': 'void configureLanguage() {}\n',
    'src/storage.cpp': 'void persistLanguage() {}\n',
    'src/registration.cpp': 'registerMaterialComponent("LanguagePicker");\n',
    'share/copy.json': '{"en":"Language","zh":"語言","both":"Language · 語言"}\n',
    'docs/feature.md': '# Language control\n',
    'tests/feature.cpp': 'void testLanguagePersistence() {}\n',
  };
  for (const [file, content] of Object.entries(sourceFiles)) {
    mkdirSync(dirname(join(root, file)), { recursive: true }); writeFileSync(join(root, file), content);
  }
  const git = (...args) => execFileSync('git', args, { cwd: root, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
  git('init', '--quiet'); git('config', 'user.name', 'Claude Fable 5.1'); git('config', 'user.email', 'noreply@anthropic.com');
  git('config', 'core.autocrlf', 'false'); git('add', 'src', 'share', 'docs', 'tests');
  git('commit', '--quiet', '-m', 'Create disposable evidence fixtures\n\nEvidence receipts need a place to trip before they learn to pass.\n證據收據先喺臨時場地跌一跌，先知檢查真係識捉錯。\n\nCo-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>');
  const sourceCommit = git('rev-parse', 'HEAD');
  const artifactSha256 = 'b'.repeat(64);
  const options = { root, sourceCommit, artifactHashes: { app: artifactSha256 }, revisionCache: new Map() };
  const ref = (file, line = undefined) => ({ file, sha256: sha256(readFileSync(join(root, file))),
    ...(line ? { line, text: readFileSync(join(root, file), 'utf8').split('\n')[line - 1] } : {}) });
  mkdirSync(join(root, 'design/evidence'), { recursive: true });
  const captureBytes = readFileSync(join(repoRoot, 'design/parity/evidence/appearance-default/built.png'));
  writeFileSync(join(root, 'design/evidence/capture.png'), captureBytes);
  const capture = ref('design/evidence/capture.png');
  const tuple = { screen: 'app.shell', state: 'language-selection', language: 'english', theme: 'light',
    width: captureBytes.readUInt32BE(16), height: captureBytes.readUInt32BE(20), scale: 1 };
  const receipt = {
    schemaVersion: 1, kind: 'built-ui-interaction', sourceCommit, artifactSha256,
    surface: 'app.shell', feature: 'language-modes', capability: 'english', tuple,
    method: 'lowlevel-headless-built-artifact', hiddenDesktop: true, fixture: 'isolated-nonpersonal', synthetic: false, injected: false,
    privacy: { verdict: 'reviewed-safe', privateDataPresent: false, reviewer: 'fixture-only', reviewedAt: '2026-10-03T00:00:00Z' },
    inspection: { verdict: 'reviewed', allFramesOpened: true, reviewer: 'fixture-only' },
    steps: [{ ordinal: 1, input: 'keyboard', target: 'language-control', accessibleName: 'Language', action: 'select English',
      before: 'control focused', expectedAfter: 'English selected', after: 'English selected', verdict: 'passed', tuple: { ...tuple }, capture }],
  };
  const testResult = { schemaVersion: 1, kind: 'focused-test-result', sourceCommit, artifactSha256,
    testFile: 'tests/feature.cpp', testFileSha256: ref('tests/feature.cpp').sha256, status: 'passed', passed: 1, failed: 0,
    command: 'fixture-only', negativeRegression: { failedWhenBroken: true, passedWhenRestored: true } };
  const writeReceipt = value => { writeFileSync(join(root, 'design/evidence/interaction.json'), JSON.stringify(value)); return ref('design/evidence/interaction.json'); };
  const writeResult = value => { writeFileSync(join(root, 'design/evidence/test-result.json'), JSON.stringify(value)); return ref('design/evidence/test-result.json'); };
  const row = { surface: 'app.shell', feature: 'language-modes', capability: 'english', status: 'implemented', evidence: {
    implementation: ref('src/feature.cpp', 1), localizedCopy: ref('share/copy.json'), article: ref('docs/feature.md'),
    test: ref('tests/feature.cpp', 1), persistence: ref('src/storage.cpp', 1), registration: ref('src/registration.cpp', 1), configuration: ref('src/feature.cpp', 1),
    languages: ['english', 'cantonese', 'bilingual'], interaction: writeReceipt(receipt), capture, testResult: writeResult(testResult),
  } };
  check('simulated complete receipt passes structural validation only', () => assert.deepEqual(validateEvidence(row, options), []));
  function evidenceMutation(name, mutate, expected) {
    check(name, () => {
      const candidate = structuredClone(row), copy = structuredClone(receipt), result = structuredClone(testResult);
      mutate(candidate, copy, result);
      if (candidate.evidence?.interaction) candidate.evidence.interaction = writeReceipt(copy);
      if (candidate.evidence?.testResult) candidate.evidence.testResult = writeResult(result);
      assert(validateEvidence(candidate, options).some(e => e.includes(expected)), `${name}: expected ${expected}`);
      row.evidence.interaction = writeReceipt(receipt); row.evidence.testResult = writeResult(testResult);
      assert.deepEqual(validateEvidence(row, options), []);
    });
  }
  for (const role of ['implementation', 'localizedCopy', 'article', 'test', 'persistence', 'registration', 'configuration', 'interaction', 'testResult']) {
    evidenceMutation(`missing ${role}`, r => delete r.evidence[role], role);
  }
  evidenceMutation('source hash mismatch', r => r.evidence.implementation.sha256 = '0'.repeat(64), 'hash mismatch');
  evidenceMutation('literal source line renamed', r => r.evidence.implementation.text = 'void configureLanguageRenamed() {}', 'exact source boundary');
  evidenceMutation('missing source line', r => delete r.evidence.implementation.line, 'exact line');
  evidenceMutation('missing localized mode', r => r.evidence.languages.pop(), 'language coverage');
  evidenceMutation('stale receipt revision', (_r, x) => x.sourceCommit = 'a'.repeat(40), 'stale');
  evidenceMutation('wrong binary hash', (_r, x) => x.artifactSha256 = 'c'.repeat(64), 'artifact hash');
  evidenceMutation('receipt reused across surface', (_r, x) => x.surface = 'site.overview', 'another inventory cell');
  evidenceMutation('receipt reused across capability', (_r, x) => x.capability = 'bilingual', 'another inventory cell');
  const claim = (r, x, feature, capability) => { r.feature = x.feature = feature; r.capability = x.capability = capability; };
  evidenceMutation('dark capability rejects a light tuple', (r, x) => claim(r, x, 'clipping-matrix', 'dark'), 'capability tuple');
  evidenceMutation('200 percent capability rejects scale one', (r, x) => claim(r, x, 'clipping-matrix', 'scale-200'), 'capability tuple');
  evidenceMutation('Cantonese capability rejects English tuple', (r, x) => claim(r, x, 'clipping-matrix', 'cantonese'), 'capability tuple');
  evidenceMutation('language mode capability rejects a different rendered language', (r, x) => claim(r, x, 'language-modes', 'cantonese'), 'capability tuple');
  evidenceMutation('separate axis evidence cannot replace the combined layout matrix', (r, x) => {
    claim(r, x, 'clipping-matrix', 'normal-minimum'); x.tuple.viewport = 'normal'; x.steps[0].tuple.viewport = 'normal';
  }, 'required layout tuple missing: bilingual/dark/2/minimum');
  evidenceMutation('historical ledger schema', (_r, x) => x.schemaVersion = 0, 'schema');
  evidenceMutation('source preview is not built provenance', (_r, x) => x.method = 'source-preview', 'genuine');
  evidenceMutation('synthetic capture receipt', (_r, x) => x.synthetic = true, 'genuine');
  evidenceMutation('injected UI receipt', (_r, x) => x.injected = true, 'genuine');
  evidenceMutation('visible desktop receipt', (_r, x) => x.hiddenDesktop = false, 'genuine');
  evidenceMutation('missing privacy review', (_r, x) => delete x.privacy, 'privacy');
  evidenceMutation('private-data capture', (_r, x) => x.privacy.privateDataPresent = true, 'privacy');
  evidenceMutation('unopened capture', (_r, x) => x.inspection.allFramesOpened = false, 'inspection');
  evidenceMutation('empty interaction sequence', (_r, x) => x.steps = [], 'steps absent');
  evidenceMutation('missing input method', (_r, x) => delete x.steps[0].input, 'semantic');
  evidenceMutation('wrong semantic outcome', (_r, x) => x.steps[0].after = 'unchanged', 'semantic');
  evidenceMutation('missing capture', (_r, x) => delete x.steps[0].capture, 'capture requires');
  evidenceMutation('wrong capture hash', (_r, x) => x.steps[0].capture.sha256 = '0'.repeat(64), 'hash mismatch');
  evidenceMutation('different per-click tuple', (_r, x) => x.steps[0].tuple.scale = 2, 'tuple differs');
  evidenceMutation('missing tuple field', (_r, x) => delete x.tuple.theme, 'tuple incomplete');
  evidenceMutation('wrong pixel dimensions', (_r, x) => x.tuple.width += 1, 'pixel dimensions');
  evidenceMutation('missing final capture', r => delete r.evidence.capture, 'final capture');
  evidenceMutation('failed focused execution', (_r, _x, t) => t.status = 'failed', 'test result');
  evidenceMutation('stale focused execution', (_r, _x, t) => t.sourceCommit = 'a'.repeat(40), 'test result');
  evidenceMutation('missing deliberate red evidence', (_r, _x, t) => t.negativeRegression.failedWhenBroken = false, 'test result');
  for (const path of ['../outside.txt', '/outside.txt', 'C:/private.txt', 'src/../private.txt', 'src/%2e%2e/private.txt', 'src/file.cpp:stream', '.git/config', 'src/vault.kdbx']) {
    evidenceMutation(`unsafe path ${path}`, r => r.evidence.implementation.file = path, path.endsWith('.kdbx') ? 'unsupported' : 'unsafe');
  }
  check('linked evidence directory rejected before content read', () => {
    mkdirSync(join(root, 'outside'), { recursive: true }); writeFileSync(join(root, 'outside/linked.cpp'), 'void linked() {}');
    symlinkSync(join(root, 'outside'), join(root, 'src/linked'), process.platform === 'win32' ? 'junction' : 'dir');
    const candidate = structuredClone(row);
    candidate.evidence.implementation = { file: 'src/linked/linked.cpp', sha256: sha256('void linked() {}'), line: 1, text: 'void linked() {}' };
    assert(validateEvidence(candidate, options).some(e => e.includes('linked evidence path')));
  });
  check('current file cannot impersonate source at the pinned commit', () => {
    writeFileSync(join(root, 'src/feature.cpp'), 'void replacement() {}\n');
    const candidate = structuredClone(row); candidate.evidence.implementation = ref('src/feature.cpp', 1);
    assert(validateEvidence(candidate, options).some(e => e.includes('source differs from pinned')));
    writeFileSync(join(root, 'src/feature.cpp'), sourceFiles['src/feature.cpp']);
    assert.deepEqual(validateEvidence(row, options), []);
  });
  check('missing expected artifact identity stays red', () => {
    assert(validateEvidence(row, { ...options, artifactHashes: {} }).some(e => e.includes('expected built artifact')));
  });
  check('one valid fixture cell cannot make the product complete', () => {
    const candidate = structuredClone(baseline); candidate.capabilities.rows.push(row);
    const result = evaluateBundle(candidate, options);
    assert.equal(result.verifiedCells, 1); assert.equal(result.productComplete, false);
    assert.equal(result.incomplete.length, SURFACE_CONTRACTS.length * FEATURE_CONTRACTS.length);
  });
  check('duplicate proof rows fail schema', () => {
    const candidate = structuredClone(baseline); candidate.capabilities.rows.push(row, row);
    assert(validateSchema(candidate).some(e => e.includes('duplicate')));
  });
} finally {
  // Resolve the exact task-created root before the only recursive deletion.
  assert.equal(realpathSync(root), expectedRoot);
  assert(!relative(realpathSync(tmpdir()), expectedRoot).startsWith('..'));
  assert(expectedRoot.includes('keepassxc-inventory-'));
  rmSync(root, { recursive: true, force: true });
}
check('CLI schema mode passes without declaring product complete', () => {
  const result = spawnSync(process.execPath, ['scripts/check-feature-inventory.mjs', '--schema-only', '--summary'], { cwd: repoRoot, encoding: 'utf8' });
  assert.equal(result.status, 0); assert.match(result.stdout, /Product: INCOMPLETE/);
});
check('CLI product mode remains red', () => {
  const result = spawnSync(process.execPath, ['scripts/check-feature-inventory.mjs', '--summary'], { cwd: repoRoot, encoding: 'utf8' });
  assert.equal(result.status, 1); assert.match(result.stdout, /0\/82668 current capability cells verified/);
});
process.stdout.write(`${passed} focused contract checks passed; ${failed} failed. Registry inventory: ${baseline.inventory.rows.length} feature rows, ${baseline.surfaces.rows.length} surfaces, ${baseline.capabilities.contracts.length} capabilities. Product completeness remains incomplete.\n`);
process.exitCode = failed ? 1 : 0;
