import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { runInNewContext } from 'node:vm';

const root = new URL('../', import.meta.url);
const app = readFileSync(new URL('site/app.js', root), 'utf8');
const build = readFileSync(new URL('site/build.mjs', root), 'utf8');
const manifest = JSON.parse(readFileSync(new URL('site/content-manifest.json', root), 'utf8'));
const categoryDeclaration = build.match(/^const categories=new Set\([^\n]+/m)?.[0];
const ownKeysDeclaration = app.match(/^const ownKeys=[^\n]+/m)?.[0];
const start = app.indexOf('function loadContentManifest(data){');
const end = app.indexOf('\nconst validTime=', start);
assert(categoryDeclaration && ownKeysDeclaration && start >= 0 && end > start, 'Production validation boundaries changed');
const buildCategories = runInNewContext(`${categoryDeclaration}\n[...categories]`);
const load = data => runInNewContext(`${ownKeysDeclaration}\nlet articles=[];\n${app.slice(start, end)}\nloadContentManifest(data); articles`, { data });
let passed = 0, failed = 0;
function check(name, test) {
  try { test(); passed++; console.log(`PASS ${name}`); }
  catch (error) { failed++; console.error(`FAIL ${name}: ${error.message}`); }
}
check('build accepts every checked-in documentation category', () => {
  const actual = readdirSync(fileURLToPath(new URL('docs/features/', root)), { withFileTypes: true })
    .filter(entry => entry.isDirectory()).map(entry => entry.name).sort();
  assert.deepEqual(Array.from(buildCategories).sort(), actual);
});
check('client loads every article from the actual immutable manifest', () => {
  const articles = load(manifest);
  assert.equal(articles.length, manifest.articles.length);
  for (const path of ['interface/ui-motion.md', 'interface/vault-pane-sizing.md', 'search/context-menu-search.md']) {
    assert.equal(articles.filter(article => article[2] === path).length, 1, path);
  }
});
check('client still rejects an unknown category', () => {
  const invalid = structuredClone(manifest);
  invalid.articles[0].category = 'unknown-category';
  assert.throws(() => load(invalid), /Invalid content manifest/);
});
console.log(`${passed} checks passed; ${failed} failed.`);
process.exitCode = failed ? 1 : 0;
