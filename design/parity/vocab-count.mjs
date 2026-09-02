// Count how many visible labels on a screen carry a replacement value from the
// private vocabulary once its validated cache is present. Prints counts only;
// the receipt and profile are deleted. Usage:
//   node vocab-count.mjs <exe> <repo> <scratchDir> <jsonPath> <screen>
import { spawnSync } from 'node:child_process';
import { mkdirSync, writeFileSync, readFileSync, existsSync, rmSync } from 'node:fs';
import path from 'node:path';

// The cheap Lowlevel checkout: LOWLEVEL_DIR overrides the default sibling path.
const cheapDir = process.env.LOWLEVEL_DIR || 'C:/Users/cntow/Documents/GitHub/lowlevel-computer-use-mcp';
function cheap(tool, args) {
  const r = spawnSync('uv', ['run', '--directory', cheapDir, 'lowlevel-computer-use-cheap', tool, '--json', JSON.stringify(args)], { encoding: 'utf8' });
  const out = r.stdout || '';
  const at = out.indexOf('{');
  if (at < 0) throw new Error(tool + ': ' + out + r.stderr);
  return JSON.parse(out.slice(at));
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const [exe, repo, scratch, jsonPath, screen] = process.argv.slice(2);
const desk = 'kpxc-vocab-count';
const profile = path.join(scratch, 'profile-count');
rmSync(profile, { recursive: true, force: true });
mkdirSync(profile, { recursive: true });
const source = JSON.parse(readFileSync(jsonPath, 'utf8'));
const entries = source.entries || source.replacements || {};
const values = Object.values(entries).filter(Boolean);
const canonical = JSON.stringify({ schemaVersion: 1, entries });
const escaped = canonical.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
writeFileSync(path.join(profile, 'keepassxc.ini'), ['[General]', 'ConfigVersion=2', '', '[GUI]', 'ApplicationTheme=light', 'MaterialSeed=keepass', 'MaterialDensity=comfortable', 'MaterialBackdrop=false', 'VoiceLanguage=Bilingual', 'FunnyLevelEnglish=3', 'FunnyLevelCantonese=3', 'VoiceDisclosureShown=true', 'DimSumSurprise=false', 'CheckForUpdates=false', 'ShowTrayIcon=false', 'MinimizeOnStartup=false', 'ShowExpiredEntriesOnDatabaseUnlock=false', ''].join(String.fromCharCode(13, 10)));
const localLines = ['[General]', 'ConfigVersion=2', '', '[GUI]', `PersonalVocabularyCache="${escaped}"`, ''];
async function run(withCache) {
  writeFileSync(path.join(profile, 'keepassxc-local.ini'), (withCache ? localLines : localLines.slice(0, 3)).join(String.fromCharCode(13, 10)));
  const receipt = path.join(profile, `receipt-${withCache ? 'cache' : 'plain'}.json`);
  rmSync(receipt, { force: true });
  const route = `kpxc://capture/${screen}?state=default&width=1200&height=860&theme=light&lang=bilingual&target=page&probe=1`;
  const cmd = `"${exe}" --config "${profile}/keepassxc.ini" --localconfig "${profile}/keepassxc-local.ini" --allow-screencapture --capture-route "${route}" --capture-receipt "${receipt}" --keyfile "${repo}/design/parity/fixtures/parity.keyx" "${repo}/design/parity/fixtures/parity.kdbx"`;
  const launch = cheap('launch_on_headless_desktop', { name: desk, command: cmd });
  if (!launch.ok) throw new Error(JSON.stringify(launch));
  for (let i = 0; i < 60 && !existsSync(receipt); i++) await sleep(500);
  const texts = existsSync(receipt) ? JSON.parse(readFileSync(receipt, 'utf8')).widgets.map((w) => w.text || '').filter(Boolean) : null;
  cheap('kill_process', { pid: launch.pid, force: true });
  await sleep(1000);
  return texts;
}
try {
  cheap('create_headless_desktop', { name: desk });
  const plain = await run(false);
  const withCache = await run(true);
  if (!plain || !withCache) throw new Error('no receipt');
  const hits = withCache.filter((t) => values.some((v) => t.includes(v))).length;
  const plainHits = plain.filter((t) => values.some((v) => t.includes(v))).length;
  const changed = withCache.filter((t) => !plain.includes(t)).length;
  console.log(JSON.stringify({ screen, entries: values.length, labels: withCache.length, labelsCarryingAReplacement: hits, beforeCache: plainHits, labelsChanged: changed }));
} finally {
  try { cheap('close_headless_desktop', { name: desk }); } catch {}
  rmSync(profile, { recursive: true, force: true });
}
