// Drive the built app's personal-vocabulary upload on a hidden desktop.
// Usage: node vocab-proof.mjs <exe> <repo> <outDir> <jsonPath> <mode>
//   mode = neutral  : retain captures (the JSON is a neutral test file)
//   mode = private  : retain only counts; every receipt and image is deleted
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
const [exe, repo, out, jsonPath, mode] = process.argv.slice(2);
const retain = mode === 'neutral';
mkdirSync(out, { recursive: true });
const profile = path.join(out, 'profile');
mkdirSync(profile, { recursive: true });
// Same seeded profile as the parity harness: disclosure acknowledged, no
// update check, no tray, no dim sum; the local ini persists across restarts.
writeFileSync(path.join(profile, 'keepassxc.ini'), ['[General]', 'ConfigVersion=2', '', '[GUI]', 'ApplicationTheme=light', 'MaterialSeed=keepass', 'MaterialDensity=comfortable', 'MaterialBackdrop=false', 'VoiceLanguage=Bilingual', 'FunnyLevelEnglish=3', 'FunnyLevelCantonese=3', 'VoiceDisclosureShown=true', 'DimSumSurprise=false', 'CheckForUpdates=false', 'ShowTrayIcon=false', 'MinimizeOnStartup=false', 'ShowExpiredEntriesOnDatabaseUnlock=false', ''].join(String.fromCharCode(13, 10)));
if (!existsSync(path.join(profile, 'keepassxc-local.ini'))) writeFileSync(path.join(profile, 'keepassxc-local.ini'), '[General]' + String.fromCharCode(13, 10) + 'ConfigVersion=2' + String.fromCharCode(13, 10));
const desk = 'kpxc-vocab-proof';
const ledger = [];
let step = 0;
function record(entry) { ledger.push({ step: ++step, at: new Date().toISOString(), ...entry }); console.log(JSON.stringify(entry)); }
function rectOf(s) { const m = s.match(/(-?\d+),(-?\d+) (\d+)x(\d+)/); return { x: +m[1], y: +m[2], w: +m[3], h: +m[4] }; }
function cacheLine() {
  const ini = path.join(profile, 'keepassxc-local.ini');
  if (!existsSync(ini)) return '';
  const line = readFileSync(ini, 'utf8').split(/\r?\n/).find((l) => l.startsWith('PersonalVocabularyCache='));
  return line || '';
}
function entryCount() {
  const line = cacheLine();
  if (!line) return 0;
  try {
    const value = line.slice('PersonalVocabularyCache='.length).replace(/^"|"$/g, '').replace(/\\"/g, '"');
    return Object.keys(JSON.parse(value).entries || {}).length;
  } catch { return -1; }
}

let launch = null;
let target = null;
async function start(screen, state, receipt) {
  const route = `kpxc://capture/${screen}?state=${state}&width=1200&height=860&theme=light&lang=bilingual&target=page&probe=1`;
  const cmd = `"${exe}" --config "${profile}/keepassxc.ini" --localconfig "${profile}/keepassxc-local.ini" --allow-screencapture --capture-route "${route}" --capture-receipt "${receipt}" --keyfile "${repo}/design/parity/fixtures/parity.keyx" "${repo}/design/parity/fixtures/parity.kdbx"`;
  launch = cheap('launch_on_headless_desktop', { name: desk, command: cmd });
  if (!launch.ok) throw new Error('launch failed: ' + JSON.stringify(launch));
  for (let i = 0; i < 40 && !existsSync(receipt); i++) await sleep(500);
  await sleep(1500);
  const listed = cheap('list_headless_windows', { name: desk });
  if (!listed.windows) throw new Error('no windows: ' + JSON.stringify(listed) + ' launch=' + JSON.stringify(launch));
  const wins = listed.windows.filter((w) => w.process_id === launch.pid && w.width > 0 && w.height > 0 && w.title);
  target = wins.sort((a, b) => b.width * b.height - a.width * a.height)[0];
  if (!target) throw new Error('no application window');
  return JSON.parse(readFileSync(receipt, 'utf8'));
}
function stop() { if (launch) cheap('kill_process', { pid: launch.pid, force: true }); launch = null; target = null; }
function shot(name) {
  const file = path.join(out, name + '.png');
  cheap('screenshot', { hwnd: target.handle, output_path: file, client_only: true });
  return file;
}
function clickRow(receipt, name) {
  const w = receipt.widgets.find((x) => x.name === name);
  if (!w) throw new Error('row not in receipt: ' + name);
  const r = rectOf(w.rect);
  // Left third of the row: clear of the notification column at the right edge.
  cheap('mouse_click', { hwnd: target.handle, x: r.x + 200, y: r.y + Math.floor(r.h / 2) });
  return r;
}
function findControls(hwnd, depth = 0) {
  const res = cheap('list_child_windows', { hwnd });
  let list = res.children || [];
  if (depth < 2) for (const c of [...list]) if (c.visible !== false) list = list.concat(findControls(c.handle, depth + 1));
  return list;
}
async function feedFileDialog(file, receipt, rowName) {
  let dialog = null;
  for (let attempt = 0; attempt < 3 && !dialog; attempt++) {
    if (attempt > 0) { await sleep(1500); clickRow(receipt, rowName); }
    for (let i = 0; i < 16 && !dialog; i++) {
      await sleep(500);
      const all = cheap('list_headless_windows', { name: desk }).windows.filter((w) => w.process_id === launch.pid);
      dialog = all.find((w) => w['class'] === '#32770' && w.width > 200);
    }
  }
  if (!dialog) throw new Error('file dialog did not open');
  const controls = findControls(dialog.handle);
  const edit = controls.find((c) => c['class'] === 'Edit' && c.visible);
  const open = controls.find((c) => c['class'] === 'Button' && /^&?Open$/i.test(c.text || ''));
  if (!edit || !open) throw new Error('dialog controls not found: ' + JSON.stringify(controls.map((c) => [c['class'], c.text]).slice(0, 30)));
  cheap('win_set_control_text', { hwnd: edit.handle, text: file.replace(/\//g, '\\') });
  await sleep(300);
  cheap('mouse_click', { hwnd: open.handle, x: 5, y: 5 });
  await sleep(2500);
  return dialog;
}
function textsOf(receipt) { return receipt.widgets.map((w) => w.text || '').filter(Boolean); }

try {
  cheap('create_headless_desktop', { name: desk });
  // 1. Baseline: no cache, original wording.
  let receipt = await start('settings', 'personal-vocabulary', path.join(out, 'r1.json'));
  const baselineTexts = textsOf(receipt);
  record({ event: 'baseline', cache: cacheLine() ? 'present' : 'absent', capture: retain ? shot('01-settings-before') : 'not retained' });
  // 2. Upload the file through the real control and the native dialog.
  clickRow(receipt, 'specRow_upload_file');
  await feedFileDialog(jsonPath, receipt, 'specRow_upload_file');
  const loadedCount = entryCount();
  record({ event: 'upload', entriesInCache: loadedCount, capture: retain ? shot('02-vocabulary-loaded') : 'not retained' });
  stop();
  // 3. Restart: cache persists and wording applies.
  receipt = await start('settings', 'personal-vocabulary', path.join(out, 'r2.json'));
  const afterTexts = textsOf(receipt);
  const changed = afterTexts.filter((t) => !baselineTexts.includes(t)).length;
  record({ event: 'restart', entriesInCache: entryCount(), textsChanged: changed, textsTotal: afterTexts.length, capture: retain ? shot('03-after-restart') : 'not retained' });
  // 4. Replace with a second file (one entry).
  const second = path.join(out, 'second.json');
  writeFileSync(second, JSON.stringify({ schemaVersion: 1, entries: { Interface: 'Look and feel' } }));
  await sleep(8000); // let the success toast auto-dismiss
  clickRow(receipt, 'specRow_upload_file');
  await feedFileDialog(second, receipt, 'specRow_upload_file');
  const replacedCount = entryCount();
  record({ event: 'replace', entriesInCache: replacedCount, capture: retain ? shot('04-replaced') : 'not retained' });
  // 5. Invalid file is refused and the cache is kept.
  const invalid = path.join(out, 'invalid.json');
  writeFileSync(invalid, '{"schemaVersion": 2, "entries": {"a": "b"}}');
  await sleep(8000);
  clickRow(receipt, 'specRow_upload_file');
  await feedFileDialog(invalid, receipt, 'specRow_upload_file');
  record({ event: 'invalid-file', entriesInCache: entryCount(), refused: entryCount() === replacedCount, capture: retain ? shot('05-invalid-refused') : 'not retained' });
  // 6. Clear restores original wording.
  clickRow(receipt, 'specRow_delete_sweep');
  await sleep(2000);
  record({ event: 'clear', cache: cacheLine() ? 'present' : 'absent', capture: retain ? shot('06-cleared') : 'not retained' });
  stop();
  receipt = await start('settings', 'personal-vocabulary', path.join(out, 'r3.json'));
  const restored = textsOf(receipt).filter((t) => !baselineTexts.includes(t)).length;
  record({ event: 'restart-after-clear', textsDifferingFromBaseline: restored, capture: retain ? shot('07-original-wording') : 'not retained' });
  stop();
} finally {
  stop();
  try { cheap('close_headless_desktop', { name: desk }); } catch {}
  if (!retain) {
    for (const f of ['r1.json', 'r2.json', 'r3.json', 'invalid.json', 'second.json']) rmSync(path.join(out, f), { force: true });
    rmSync(profile, { recursive: true, force: true });
  }
  writeFileSync(path.join(out, 'ledger.json'), JSON.stringify({ mode, exe, ledger }, null, 2));
}
