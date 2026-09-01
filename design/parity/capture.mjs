// Capture the reference and the built application for every inventory row at
// the identical tuple, through the approved headless route, and write the raw
// PNGs plus a per-row capture receipt under design/parity/evidence/<row>/.
//
//   node design/parity/capture.mjs --app <KeePassXC.exe> [--rows a,b] [--side both|reference|built]
//
// Reference side: the local reference server (serve-reference.mjs, vendored
// assets) rendered by Edge in new headless mode with a fixed window size, an
// isolated throwaway profile, extensions and sync off, and a virtual time
// budget so React/Babel and fonts settle before the screenshot.
//
// Built side: KeePassXC launched on a named off-screen Windows desktop through
// the Cheap Version CLI with --capture-route, a throwaway configuration and the
// key-file-only fixture database; the harness polls the JSON receipt the route
// writes, then captures the window by HWND from that same off-screen desktop.
import { spawn, spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdir, readFile, rm, writeFile, access } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { PNG } from 'pngjs';

const here = dirname(fileURLToPath(import.meta.url));
const designRoot = resolve(here, '..');
const repoRoot = resolve(designRoot, '..');
const inventoryPath = join(here, 'inventory.json');
const evidenceRoot = join(here, 'evidence');

const args = new Map();
for (let i = 2; i < process.argv.length; i += 1) {
  const key = process.argv[i];
  if (!key.startsWith('--')) continue;
  const next = process.argv[i + 1];
  const value = next && !next.startsWith('--') ? process.argv[++i] : 'true';
  args.set(key.slice(2), value);
}
const side = args.get('side') || 'both';
const appPath = args.get('app') ? resolve(args.get('app')) : '';
const rowFilter = args.get('rows') ? new Set(args.get('rows').split(',')) : null;
const edgePath = args.get('edge') || 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const lowlevelDir = args.get('lowlevel') || 'C:\\Users\\cntow\\Documents\\GitHub\\lowlevel-computer-use-mcp';
const desktopName = args.get('desktop') || 'KpxcParity';
const referencePort = Number.parseInt(args.get('port') || '43110', 10);
const scratch = args.get('scratch') || join(tmpdir(), 'kpxc-parity');

const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const exists = async path => access(path).then(() => true, () => false);
const sleep = ms => new Promise(done => setTimeout(done, ms));

function cheap(tool, params) {
  // The Cheap Version runs the same headless tools without an MCP transport.
  const cli = spawnSync('uv', ['run', '--directory', lowlevelDir, 'lowlevel-computer-use-cheap', tool, '--json', JSON.stringify(params)], {
    encoding: 'utf8', windowsHide: true, timeout: 120000
  });
  if (cli.status !== 0) throw new Error(`cheap ${tool} exited ${cli.status}: ${cli.stderr || cli.stdout}`);
  // The CLI pretty-prints one JSON object; anything before its first brace is noise.
  const text = cli.stdout.slice(cli.stdout.indexOf(String.fromCharCode(123)));
  const result = JSON.parse(text);
  if (!result.ok) throw new Error(`cheap ${tool}: ${result.error}`);
  return result;
}

function countBlackBottomRows(png) {
  let rows = 0;
  for (let y = png.height - 1; y >= 0; y -= 1) {
    let allBlack = true;
    for (let x = 0; x < png.width; x += 7) {
      const i = (y * png.width + x) * 4;
      if (png.data[i] > 8 || png.data[i + 1] > 8 || png.data[i + 2] > 8) { allBlack = false; break; }
    }
    if (!allBlack) break;
    rows += 1;
  }
  return rows;
}

function gitHead() {
  const git = spawnSync('git', ['-C', repoRoot, 'rev-parse', 'HEAD'], { encoding: 'utf8' });
  return git.stdout.trim();
}

async function captureReference(row, outDir, receipt) {
  const url = `http://127.0.0.1:${referencePort}${row.referenceRoute}`;
  const profile = join(scratch, `edge-${row.id}`);
  await rm(profile, { recursive: true, force: true });
  const png = join(outDir, 'reference.png');
  await rm(png, { force: true });
  const edgeArgs = [
    '--headless=new', '--disable-gpu', '--hide-scrollbars', '--no-first-run', '--no-default-browser-check',
    '--disable-extensions', '--disable-sync', '--disable-component-extensions-with-background-pages',
    '--disable-features=msEdgeFirstRunExperience,msEdgeSignin,msEdgeSync,Translate',
    `--user-data-dir=${profile}`, `--window-size=${row.tuple.viewport.width},${row.tuple.viewport.height}`,
    `--force-device-scale-factor=${row.tuple.scale}`, `--lang=${row.tuple.locale}`,
    '--virtual-time-budget=12000', `--screenshot=${png}`, url
  ];
  const edge = spawnSync(edgePath, edgeArgs, { encoding: 'utf8', windowsHide: true, timeout: 120000 });
  if (!(await exists(png))) throw new Error(`Edge produced no screenshot for ${row.id}: ${edge.stderr}`);
  const bytes = await readFile(png);
  receipt.reference = { url, png: 'reference.png', sha256: sha256(bytes), bytes: bytes.length, tool: 'msedge --headless=new --screenshot', edgeArgs };
  await rm(profile, { recursive: true, force: true });
}

async function writeProfile(row, dir) {
  await mkdir(dir, { recursive: true });
  const ini = join(dir, 'keepassxc.ini');
  const theme = row.tuple.theme;
  const mode = row.determinism.languageMode;
  const language = mode === 'both' ? 'Bilingual' : mode === 'yue' ? 'Cantonese' : 'English';
  await writeFile(ini, [
    '[General]', 'ConfigVersion=2', '',
    '[GUI]', `ApplicationTheme=${theme}`, 'MaterialSeed=keepass', 'MaterialDensity=comfortable', 'MaterialBackdrop=false',
    `VoiceLanguage=${language}`, 'FunnyLevelEnglish=3', 'FunnyLevelCantonese=3', 'VoiceDisclosureShown=true', 'DimSumSurprise=false',
    'CheckForUpdates=false', 'ShowTrayIcon=false', 'MinimizeOnStartup=false', 'ShowExpiredEntriesOnDatabaseUnlock=false', ''
  ].join('\r\n'));
  return ini;
}

async function captureBuilt(row, outDir, receipt) {
  if (!appPath) throw new Error('--app <KeePassXC.exe> is required for the built side');
  const profileDir = join(scratch, `profile-${row.id}`);
  await rm(profileDir, { recursive: true, force: true });
  const ini = await writeProfile(row, profileDir);
  // An isolated local config that already exists, so the launch never reads the
  // real user's local state (last databases, geometry) into a capture.
  const localIni = join(profileDir, 'keepassxc-local.ini');
  await writeFile(localIni, '[General]' + String.fromCharCode(13, 10) + 'ConfigVersion=2' + String.fromCharCode(13, 10));
  const fixture = join(here, 'fixtures', 'parity.kdbx');
  const keyFile = join(here, 'fixtures', 'parity.keyx');
  const receiptPath = join(profileDir, 'capture-receipt.json');
  const { width, height } = row.tuple.viewport;
  const state = row.state || 'default';
  const lang = row.determinism.languageMode === 'both' ? 'bilingual' : row.determinism.languageMode;
  const route = `kpxc://capture/${row.screen}?state=${encodeURIComponent(state)}&width=${width}&height=${height}&theme=${row.tuple.theme}&lang=${lang}${row.screen === 'shell' || row.screen === 'regex-builder' ? '' : '&target=page'}`;
  // The welcome screen is captured with no database at all.
  const database = row.screen === 'welcome' ? '' : ` --keyfile "${keyFile}" "${fixture}"`;
  const command = `"${appPath}" --config "${ini}" --localconfig "${localIni}" --allow-screencapture --capture-route "${route}" --capture-receipt "${receiptPath}"${database}`;
  cheap('create_headless_desktop', { name: desktopName });
  const launch = cheap('launch_on_headless_desktop', { name: desktopName, command });
  let routeReceipt = null;
  const started = Date.now();
  while (Date.now() - started < 45000) {
    if (await exists(receiptPath)) {
      routeReceipt = JSON.parse(await readFile(receiptPath, 'utf8'));
      break;
    }
    await sleep(500);
  }
  try {
    if (!routeReceipt) throw new Error(`No capture receipt within 45 s for ${row.id} (pid ${launch.pid})`);
    if (routeReceipt.outcome !== 'ready') throw new Error(`Route reported ${routeReceipt.outcome} for ${row.id}`);
    // Let the last relayout paint before the capture.
    await sleep(700);
    const windows = cheap('list_headless_windows', { name: desktopName }).windows
      .filter(w => w.process_id === launch.pid && w.width > 0 && w.height > 0 && w.title);
    const target = windows.find(w => String(w.handle) === String(routeReceipt.hwnd)) || windows[0];
    if (!target) throw new Error(`No top-level window for pid ${launch.pid} on ${desktopName}`);
    const png = join(outDir, 'built.png');
    await rm(png, { force: true });
    // PrintWindow returns black for any region the window has not painted yet,
    // which happens on the tallest windows right after the final resize. Capture
    // until the bottom rows are painted, up to a bounded number of attempts.
    let attempts = 0;
    let blackRows = -1;
    do {
      cheap('screenshot', { hwnd: target.handle, output_path: png, client_only: true });
      blackRows = countBlackBottomRows(PNG.sync.read(await readFile(png)));
      attempts += 1;
      if (blackRows > 0) await sleep(900);
    } while (blackRows > 0 && attempts < 6);
    if (blackRows > 0) throw new Error(`${row.id}: ${blackRows} unpainted rows remain after ${attempts} captures`);
    // Destination rows are compared against the design's destination content
    // alone, so crop the client capture to the rectangle the route reported.
    const wholeShell = row.screen === 'shell' || row.screen === 'regex-builder' || row.screen === 'welcome';
    let cropRect = null;
    if (!wholeShell && routeReceipt.pageRect) {
      const [x, y, w, h] = routeReceipt.pageRect.split(/[ ,x]/).map(Number);
      cropRect = { x, y, w, h };
      const full = PNG.sync.read(await readFile(png));
      const out = new PNG({ width: w, height: h });
      for (let yy = 0; yy < h; yy += 1) full.data.copy(out.data, yy * w * 4, ((y + yy) * full.width + x) * 4, ((y + yy) * full.width + x + w) * 4);
      await writeFile(join(outDir, 'built-window.png'), await readFile(png));
      await writeFile(png, PNG.sync.write(out));
    }
    const bytes = await readFile(png);
    receipt.built = {
      route, png: 'built.png', sha256: sha256(bytes), bytes: bytes.length, pid: launch.pid, hwnd: target.handle,
      windowTitle: target.title, windowClass: target.class, dpi: target.dpi, routeReceipt, cropRect, windowPng: cropRect ? 'built-window.png' : null,
      tool: 'lowlevel-computer-use-cheap screenshot(hwnd, client_only) on a named off-screen desktop'
    };
  } finally {
    try { cheap('kill_process', { pid: launch.pid, force: true }); } catch {}
    await sleep(300);
  }
}

async function main() {
  const inventory = JSON.parse(await readFile(inventoryPath, 'utf8'));
  const rows = inventory.rows.filter(row => !rowFilter || rowFilter.has(row.id));
  if (!rows.length) throw new Error('No inventory rows matched');
  await mkdir(scratch, { recursive: true });
  const head = gitHead();
  let server = null;
  if (side !== 'built') {
    server = spawn(process.execPath, [join(designRoot, 'reference-app', 'serve-reference.mjs'), String(referencePort)], { stdio: 'ignore', windowsHide: true });
    await sleep(800);
  }
  const results = [];
  try {
    for (const row of rows) {
      const outDir = join(evidenceRoot, row.id);
      await mkdir(outDir, { recursive: true });
      const receiptFile = join(outDir, 'capture-receipt.json');
      const previous = (await exists(receiptFile)) ? JSON.parse(await readFile(receiptFile, 'utf8')) : {};
      const receipt = {
        ...previous,
        schemaVersion: 1, rowId: row.id, tuple: row.tuple, determinism: row.determinism,
        sourceCommit: head, capturedAt: new Date().toISOString()
      };
      if (side !== 'built') await captureReference(row, outDir, receipt);
      if (side !== 'reference') {
        if (row.builtRouteStatus === 'unimplemented') receipt.built = { skipped: 'builtRouteStatus is unimplemented' };
        else await captureBuilt(row, outDir, receipt);
      }
      delete receipt.comparison; // stale once either side is recaptured
      await writeFile(receiptFile, JSON.stringify(receipt, null, 2) + String.fromCharCode(10));
      results.push(receipt);
      const ref = receipt.reference ? receipt.reference.sha256.slice(0, 12) : '-';
      const built = receipt.built && receipt.built.sha256 ? receipt.built.sha256.slice(0, 12) : (receipt.built && receipt.built.skipped) || '-';
      console.log(`${row.id}: reference ${ref} built ${built}`);
    }
  } finally {
    if (server) server.kill();
    if (side !== 'reference') {
      try { cheap('close_headless_desktop', { name: desktopName }); } catch {}
    }
  }
  return results;
}

main().catch(error => {
  console.error(error.message);
  process.exit(1);
});
