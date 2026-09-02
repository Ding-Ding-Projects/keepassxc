// Clipping matrix: drive every shell destination across the supported width
// classes, language modes, themes and display scales on an off-screen desktop,
// keep a capture and the application's own widget probe for each tuple, and
// report every squeezed, overflowing or off-screen widget as a finding.
//
//   node design/parity/clipping-matrix.mjs --app <KeePassXC.exe> [--quick]
//
// Output: design/parity/evidence/clipping/<tuple>.png, <tuple>.json and a
// summary at design/parity/evidence/clipping/matrix.json. A finding is a widget
// whose text is wider than its box (and not word-wrapped), a widget narrower
// than its own minimum size hint, or a widget partly outside the window.
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdir, readFile, rm, writeFile, access } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, '..', '..');
const outRoot = join(here, 'evidence', 'clipping');

const args = new Map();
for (let i = 2; i < process.argv.length; i += 1) {
  const key = process.argv[i];
  if (!key.startsWith('--')) continue;
  const next = process.argv[i + 1];
  args.set(key.slice(2), next && !next.startsWith('--') ? process.argv[++i] : 'true');
}
const appPath = resolve(args.get('app') || '');
const quick = args.get('quick') === 'true';
const lowlevelDir = args.get('lowlevel') || 'C:\\Users\\cntow\\Documents\\GitHub\\lowlevel-computer-use-mcp';
const desktopName = args.get('desktop') || 'KpxcClip';
const scratch = args.get('scratch') || join(tmpdir(), 'kpxc-clipping');

// The five window-size classes by their smallest width plus the documented
// minimum client area; heights follow a laptop-ish aspect.
// --widths, --languages, --themes, --scales and --screens accept comma lists to run a
// bounded subset of the full matrix; --quick is the smallest useful one.
const allWidths = [{ id: 'minimum', w: 480, h: 640 }, { id: 'compact', w: 600, h: 720 }, { id: 'medium', w: 840, h: 760 }, { id: 'expanded', w: 1200, h: 800 }, { id: 'large', w: 1600, h: 900 }, { id: 'extra-large', w: 1920, h: 1000 }];
function allWidthsFor(ids) { return ids.map(id => allWidths.find(w => w.id === id)).filter(Boolean); }
const widths = (args.get('widths') ? allWidthsFor(args.get('widths').split(',')) : (quick ? [{ id: 'compact', w: 600, h: 720 }, { id: 'expanded', w: 1200, h: 800 }] : null));
const pick = (key, fallback) => args.get(key) ? args.get(key).split(',') : fallback;
const languages = pick('languages', quick ? ['bilingual'] : ['english', 'cantonese', 'bilingual']);
const themes = pick('themes', quick ? ['light'] : ['light', 'dark']);
const scales = pick('scales', quick ? ['1'] : ['1', '1.25', '1.5', '2']);
const widthSet = widths || allWidths;
const screens = pick('screens', ['vault', 'reports', 'editor', 'database', 'tools', 'history', 'changelog', 'settings', 'appearance', 'help']);

const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const exists = async path => access(path).then(() => true, () => false);
const sleep = ms => new Promise(done => setTimeout(done, ms));

function cheap(tool, params) {
  const cli = spawnSync('uv', ['run', '--directory', lowlevelDir, 'lowlevel-computer-use-cheap', tool, '--json', JSON.stringify(params)], { encoding: 'utf8', windowsHide: true, timeout: 120000 });
  if (cli.status !== 0) throw new Error(`cheap ${tool} exited ${cli.status}: ${cli.stderr || cli.stdout}`);
  const result = JSON.parse(cli.stdout.slice(cli.stdout.indexOf('{')));
  if (!result.ok) throw new Error(`cheap ${tool}: ${result.error}`);
  return result;
}

function gitHead() {
  return spawnSync('git', ['-C', repoRoot, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).stdout.trim();
}

async function writeProfile(dir, theme, language) {
  await mkdir(dir, { recursive: true });
  const ini = join(dir, 'keepassxc.ini');
  const voice = language === 'bilingual' ? 'Bilingual' : language === 'cantonese' ? 'Cantonese' : 'English';
  await writeFile(ini, ['[General]', 'ConfigVersion=2', '', '[GUI]', `ApplicationTheme=${theme}`, 'MaterialSeed=keepass', 'MaterialDensity=comfortable', 'MaterialBackdrop=false', `VoiceLanguage=${voice}`, 'FunnyLevelEnglish=5', 'FunnyLevelCantonese=5', 'VoiceDisclosureShown=true', 'DimSumSurprise=false', 'CheckForUpdates=false', 'ShowTrayIcon=false', ''].join('\r\n'));
  const local = join(dir, 'keepassxc-local.ini');
  await writeFile(local, '[General]\r\nConfigVersion=2\r\n');
  return { ini, local };
}

function findings(widgets, tupleId) {
  const out = [];
  for (const w of widgets) {
    if (w.textOverflows) out.push({ tuple: tupleId, kind: 'text-overflows', widget: `${w.class}#${w.name}`, rect: w.rect, text: w.text, textWidth: w.textWidth });
    if (w.squeezed) out.push({ tuple: tupleId, kind: 'squeezed-below-minimum', widget: `${w.class}#${w.name}`, rect: w.rect, minimumHint: w.minimumHint });
    if (w.offscreen) out.push({ tuple: tupleId, kind: 'offscreen', widget: `${w.class}#${w.name}`, rect: w.rect });
  }
  return out;
}

async function runTuple(tuple) {
  const id = `${tuple.screen}_${tuple.width.id}_${tuple.language}_${tuple.theme}_x${tuple.scale.replace('.', '-')}`;
  const profileDir = join(scratch, id);
  await rm(profileDir, { recursive: true, force: true });
  const { ini, local } = await writeProfile(profileDir, tuple.theme, tuple.language);
  const receiptPath = join(profileDir, 'receipt.json');
  const fixture = join(here, 'fixtures', 'parity.kdbx');
  const keyFile = join(here, 'fixtures', 'parity.keyx');
  const route = `kpxc://capture/${tuple.screen}?state=default&width=${tuple.width.w}&height=${tuple.width.h}&theme=${tuple.theme}&lang=${tuple.language}&probe=1`;
  const command = `"${appPath}" --config "${ini}" --localconfig "${local}" --allow-screencapture --capture-scale ${tuple.scale} --keyfile "${keyFile}" --capture-route "${route}" --capture-receipt "${receiptPath}" "${fixture}"`;
  const launch = cheap('launch_on_headless_desktop', { name: desktopName, command });
  let receipt = null;
  const started = Date.now();
  while (Date.now() - started < 45000) {
    if (await exists(receiptPath)) { receipt = JSON.parse(await readFile(receiptPath, 'utf8')); break; }
    await sleep(500);
  }
  const record = { id, tuple, pid: launch.pid };
  try {
    if (!receipt) throw new Error('no receipt within 45 s');
    record.receipt = { outcome: receipt.outcome, shellViewport: receipt.shellViewport, windowGeometry: receipt.windowGeometry, devicePixelRatio: receipt.devicePixelRatio };
    const windows = cheap('list_headless_windows', { name: desktopName }).windows.filter(w => w.process_id === launch.pid && w.width > 0 && w.title);
    const target = windows.find(w => String(w.handle) === String(receipt.hwnd)) || windows[0];
    if (!target) throw new Error('no window');
    const png = join(outRoot, `${id}.png`);
    await sleep(500);
    cheap('screenshot', { hwnd: target.handle, output_path: png, client_only: true });
    record.png = `${id}.png`;
    record.pngSha256 = sha256(await readFile(png));
    record.findings = findings(receipt.widgets || [], id);
    record.widgetCount = (receipt.widgets || []).length;
    await writeFile(join(outRoot, `${id}.json`), JSON.stringify({ ...record, widgets: receipt.widgets || [] }, null, 2) + '\n');
  } catch (error) {
    record.error = error.message;
  } finally {
    try { cheap('kill_process', { pid: launch.pid, force: true }); } catch {}
    await sleep(200);
  }
  return record;
}

async function main() {
  if (!appPath || !(await exists(appPath))) throw new Error('--app <KeePassXC.exe> is required');
  await mkdir(outRoot, { recursive: true });
  await mkdir(scratch, { recursive: true });
  cheap('create_headless_desktop', { name: desktopName });
  const records = [];
  try {
    for (const scale of scales) for (const theme of themes) for (const language of languages) for (const width of widthSet) for (const screen of screens) {
      const record = await runTuple({ screen, width, language, theme, scale });
      records.push(record);
      const count = record.findings ? record.findings.length : 'ERR';
      console.log(`${record.id}: ${record.error ? record.error : `${count} finding(s), ${record.widgetCount} widgets`}`);
    }
  } finally {
    try { cheap('close_headless_desktop', { name: desktopName }); } catch {}
  }
  const summary = {
    schemaVersion: 1, sourceCommit: gitHead(), app: appPath, generatedAt: new Date().toISOString(), quick,
    tuples: records.length, errors: records.filter(r => r.error).length,
    findings: records.flatMap(r => r.findings || []),
    records: records.map(({ id, tuple, receipt, png, pngSha256, error, widgetCount, findings: f }) => ({ id, tuple, receipt, png, pngSha256, error, widgetCount, findingCount: f ? f.length : null }))
  };
  await writeFile(join(outRoot, 'matrix.json'), JSON.stringify(summary, null, 2) + '\n');
  console.log(`${summary.tuples} tuples, ${summary.errors} errors, ${summary.findings.length} findings -> ${join(outRoot, 'matrix.json')}`);
}

main().catch(error => { console.error(error.message); process.exit(1); });
