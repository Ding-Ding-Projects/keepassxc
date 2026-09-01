// Fail-closed per-surface feature inventory check.
//
//   node scripts/check-feature-inventory.mjs            # report and exit non-zero on any red row
//   node scripts/check-feature-inventory.mjs --summary  # counts only
//
// The canonical feature list below is hand-written. It is the authority: a
// feature missing from docs/features/inventory.json is red, a row whose status
// is not "implemented" is red, and an implemented row whose implementation
// symbol, localized copy key, documentation article, focused test, built-artifact
// interaction record or capture is absent or stale is red. Symbols are matched
// with line-anchored regular expressions so a commented-out line, a descendant
// path or a renamed symbol that still contains the old name cannot satisfy them.
import { readFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
export const repoRoot = resolve(here, '..');
export const inventoryPath = join(repoRoot, 'docs', 'features', 'inventory.json');

// One id per canonical user-facing contract, per surface. Adding a contract to
// the shared instructions means adding it here by hand; nothing is discovered.
export const CANONICAL_FEATURES = [
  'language-modes', 'funny-level-english', 'funny-level-cantonese', 'dialog-emoji-toggle', 'school-mode',
  'narrator', 'narrator-voice-pickers', 'scheduled-settings', 'external-settings-sources', 'home-assistant-source',
  'dim-sum-surprise', 'release-code-name', 'personal-vocabulary-upload',
  'regex-builder', 'search-bar-every-surface', 'settings-search', 'dropdown-search', 'context-menu-search',
  'notifications', 'notification-centre', 'no-nag-policy',
  'material-3-appearance', 'per-element-appearance-editor', 'infinite-color-picker', 'rainbow-color', 'font-customization', 'app-rename',
  'tabs', 'tab-docking', 'tab-overflow', 'tab-pinning', 'tab-groups', 'tab-searches', 'bulk-close-tabs', 'move-into-group-picker',
  'toy-locks', 'support-tickets', 'unlock-ladder', 'authenticator', 'qr-pairing', 'secret-mutation-history',
  'command-palette', 'super-confirmation', 'bulk-actions', 'export-everything', 'archive-export',
  'local-history', 'history-panel-filters', 'changelog-viewer', 'changelog-commit-links', 'external-editor', 'vscode-handoff',
  'offline-docs-browser', 'landing-page', 'social-preview', 'in-app-version-provenance',
  'frameless-material-title-bar', 'overlays-paint-surface', 'resizable-panels', 'shortcut-display', 'progress-where-started', 'recovery-reauth',
  'rendered-provider-text', 'collapsible-filters', 'guided-forms', 'novice-expert-controls', 'settings-explanations', 'rich-controls', 'blank-slate-presets',
  'adhd-modes', 'app-logo-customization', 'file-converter', 'ollama-manager', 'status-hub', 'browser-extension-download-dialogs',
  'accessibility', 'responsive-sizing', 'clipping-matrix',
  'auto-updates', 'squirrel-installer', 'build-scripts', 'dependency-bundling', 'line-count-release', 'readme-captures', 'screen-recording', 'design-parity'
];

export const SURFACES = ['app', 'site'];

const requiredLinks = ['implementation', 'localizedCopy', 'article', 'test', 'interaction', 'capture'];

function anchoredMatch(file, pattern) {
  const path = resolve(repoRoot, file);
  if (!existsSync(path)) return `file missing: ${file}`;
  const text = readFileSync(path, 'utf8').replace(/\r\n/g, '\n');
  let regex;
  try { regex = new RegExp(pattern, 'm'); } catch (error) { return `invalid pattern for ${file}: ${error.message}`; }
  if (!regex.test(text)) return `anchor not found in ${file}: ${pattern}`;
  return null;
}

export function validateInventory(inventory) {
  const errors = [];
  const seen = new Set();
  const rows = Array.isArray(inventory.rows) ? inventory.rows : [];
  for (const surface of SURFACES) {
    for (const id of CANONICAL_FEATURES) {
      const key = `${surface}:${id}`;
      const row = rows.find(candidate => candidate.surface === surface && candidate.id === id);
      if (!row) { errors.push(`${key}: no inventory row`); continue; }
      if (seen.has(key)) errors.push(`${key}: duplicate row`);
      seen.add(key);
      if (typeof row.title !== 'string' || !row.title.trim()) errors.push(`${key}: title missing`);
      if (row.status !== 'implemented') {
        errors.push(`${key}: status is ${row.status || 'undefined'}${row.note ? ` (${row.note})` : ''}`);
      }
      for (const link of requiredLinks) {
        const value = row[link];
        if (!value || typeof value !== 'object') { if (row.status === 'implemented') errors.push(`${key}: ${link} link missing`); continue; }
        if (typeof value.file !== 'string' || !value.file) { errors.push(`${key}: ${link}.file missing`); continue; }
        const pattern = value.anchor;
        if (pattern) {
          const problem = anchoredMatch(value.file, pattern);
          if (problem) errors.push(`${key}: ${link} ${problem}`);
        } else if (!existsSync(resolve(repoRoot, value.file))) {
          errors.push(`${key}: ${link} file missing: ${value.file}`);
        }
      }
      if (row.limitation && (typeof row.limitation.reason !== 'string' || typeof row.limitation.equivalent !== 'string')) {
        errors.push(`${key}: limitation must carry reason and equivalent`);
      }
    }
  }
  for (const row of rows) {
    if (!CANONICAL_FEATURES.includes(row.id)) errors.push(`${row.surface}:${row.id}: not a canonical feature id`);
    if (!SURFACES.includes(row.surface)) errors.push(`${row.surface}:${row.id}: unknown surface`);
  }
  return errors;
}

export function loadInventory() {
  return JSON.parse(readFileSync(inventoryPath, 'utf8'));
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const inventory = loadInventory();
  const errors = validateInventory(inventory);
  const total = CANONICAL_FEATURES.length * SURFACES.length;
  const green = total - new Set(errors.map(e => e.split(':').slice(0, 2).join(':'))).size;
  if (!process.argv.includes('--summary')) for (const error of errors) process.stdout.write(`RED ${error}\n`);
  process.stdout.write(`${green}/${total} feature rows green, ${errors.length} finding(s).\n`);
  process.exit(errors.length ? 1 : 0);
}
