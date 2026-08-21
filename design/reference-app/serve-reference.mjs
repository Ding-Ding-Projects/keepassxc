import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { dirname, extname, join, normalize, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const designRoot = resolve(here, '..');
const inventoryPath = join(designRoot, 'parity', 'inventory.json');
const host = '127.0.0.1';
const requestedPort = Number.parseInt(process.argv[2] || '43110', 10);
const fixedNow = '2026-08-20T12:00:00.000Z';

const mime = new Map([
  ['.css', 'text/css; charset=utf-8'],
  ['.html', 'text/html; charset=utf-8'],
  ['.js', 'text/javascript; charset=utf-8'],
  ['.json', 'application/json; charset=utf-8'],
  ['.png', 'image/png']
]);

function deterministicPrelude(row) {
  const seed = [...row.id].reduce((value, char) => ((value * 33) ^ char.charCodeAt(0)) >>> 0, 5381);
  return `<base href="/design/">
<style>*,*::before,*::after{animation:none!important;transition:none!important;caret-color:transparent!important}</style>
<script>
(() => {
  let seed = ${seed};
  Math.random = () => ((seed = (1664525 * seed + 1013904223) >>> 0) / 4294967296);
  const NativeDate = Date;
  const fixed = NativeDate.parse(${JSON.stringify(fixedNow)});
  class FixedDate extends NativeDate {
    constructor(...args) { super(...(args.length ? args : [fixed])); }
    static now() { return fixed; }
  }
  window.Date = FixedDate;
  window.__KPXC_REFERENCE_ROUTE__ = ${JSON.stringify(row.id)};
  window.__KPXC_REFERENCE_TUPLE__ = ${JSON.stringify(row.tuple)};
})();
<\/script>`;
}

function insideDesignRoot(path) {
  const rel = relative(designRoot, path);
  return rel && !rel.startsWith('..') && !rel.includes(':');
}

async function loadInventory() {
  return JSON.parse(await readFile(inventoryPath, 'utf8'));
}

const server = createServer(async (request, response) => {
  try {
    const url = new URL(request.url, `http://${host}`);
    if (url.pathname === '/') {
      response.writeHead(302, { Location: '/design/reference-app/index.html' });
      response.end();
      return;
    }
    if (url.pathname === '/inventory') {
      const bytes = await readFile(inventoryPath);
      response.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
      response.end(bytes);
      return;
    }
    if (url.pathname.startsWith('/reference/')) {
      const id = decodeURIComponent(url.pathname.slice('/reference/'.length));
      const inventory = await loadInventory();
      const row = inventory.rows.find(candidate => candidate.id === id);
      if (!row) {
        response.writeHead(404).end('Unknown reference route');
        return;
      }
      const source = resolve(designRoot, row.referenceFile);
      if (!insideDesignRoot(source)) throw new Error('Reference path escaped design root');
      const html = await readFile(source, 'utf8');
      const rendered = html.replace('<head>', `<head>\n${deterministicPrelude(row)}`);
      response.writeHead(200, {
        'Content-Type': 'text/html; charset=utf-8',
        'Cache-Control': 'no-store',
        'X-Reference-Source-SHA256': createHash('sha256').update(html).digest('hex')
      });
      response.end(rendered);
      return;
    }
    if (url.pathname.startsWith('/design/')) {
      const relativePath = normalize(decodeURIComponent(url.pathname.slice('/design/'.length)));
      const file = resolve(designRoot, relativePath);
      if (!insideDesignRoot(file)) throw new Error('Static path escaped design root');
      const bytes = await readFile(file);
      response.writeHead(200, {
        'Content-Type': mime.get(extname(file).toLowerCase()) || 'application/octet-stream',
        'Cache-Control': 'no-store'
      });
      response.end(bytes);
      return;
    }
    response.writeHead(404).end('Not found');
  } catch (error) {
    response.writeHead(error?.code === 'ENOENT' ? 404 : 500, { 'Content-Type': 'text/plain; charset=utf-8' });
    response.end(error instanceof Error ? error.message : String(error));
  }
});

server.listen(requestedPort, host, () => {
  const address = server.address();
  process.stdout.write(`Design reference server: http://${host}:${address.port}/\n`);
});
