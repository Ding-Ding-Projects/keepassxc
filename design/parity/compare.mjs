// Compare the raw reference and built captures of every inventory row that has
// both, write a labelled side-by-side PNG and a machine-readable diff.json, and
// record the artifact hashes in the row's capture receipt.
//
//   node design/parity/compare.mjs [--rows a,b] [--threshold 0.1]
//
// The diff is pixelmatch over the overlapping area; a size mismatch is itself a
// finding and is reported rather than hidden by scaling. Labels are drawn with a
// small built-in bitmap font so the comparison needs no system font.
import { createHash } from 'node:crypto';
import { readFile, writeFile, access } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { PNG } from 'pngjs';
import pixelmatch from 'pixelmatch';

const here = dirname(fileURLToPath(import.meta.url));
const inventoryPath = join(here, 'inventory.json');
const evidenceRoot = join(here, 'evidence');

const args = new Map();
for (let i = 2; i < process.argv.length; i += 1) {
  const key = process.argv[i];
  if (!key.startsWith('--')) continue;
  const next = process.argv[i + 1];
  args.set(key.slice(2), next && !next.startsWith('--') ? process.argv[++i] : 'true');
}
const rowFilter = args.get('rows') ? new Set(args.get('rows').split(',')) : null;
const threshold = Number.parseFloat(args.get('threshold') || '0.1');

const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const exists = async path => access(path).then(() => true, () => false);

// 5x7 glyphs for the label strip: uppercase letters, digits and a few marks.
const GLYPHS = {
  A: ['01110', '10001', '10001', '11111', '10001', '10001', '10001'],
  B: ['11110', '10001', '10001', '11110', '10001', '10001', '11110'],
  C: ['01110', '10001', '10000', '10000', '10000', '10001', '01110'],
  D: ['11110', '10001', '10001', '10001', '10001', '10001', '11110'],
  E: ['11111', '10000', '10000', '11110', '10000', '10000', '11111'],
  F: ['11111', '10000', '10000', '11110', '10000', '10000', '10000'],
  G: ['01110', '10001', '10000', '10111', '10001', '10001', '01111'],
  H: ['10001', '10001', '10001', '11111', '10001', '10001', '10001'],
  I: ['01110', '00100', '00100', '00100', '00100', '00100', '01110'],
  K: ['10001', '10010', '10100', '11000', '10100', '10010', '10001'],
  L: ['10000', '10000', '10000', '10000', '10000', '10000', '11111'],
  M: ['10001', '11011', '10101', '10101', '10001', '10001', '10001'],
  N: ['10001', '11001', '10101', '10011', '10001', '10001', '10001'],
  O: ['01110', '10001', '10001', '10001', '10001', '10001', '01110'],
  P: ['11110', '10001', '10001', '11110', '10000', '10000', '10000'],
  R: ['11110', '10001', '10001', '11110', '10100', '10010', '10001'],
  S: ['01111', '10000', '10000', '01110', '00001', '00001', '11110'],
  T: ['11111', '00100', '00100', '00100', '00100', '00100', '00100'],
  U: ['10001', '10001', '10001', '10001', '10001', '10001', '01110'],
  V: ['10001', '10001', '10001', '10001', '10001', '01010', '00100'],
  W: ['10001', '10001', '10001', '10101', '10101', '10101', '01010'],
  X: ['10001', '01010', '00100', '00100', '00100', '01010', '10001'],
  Y: ['10001', '01010', '00100', '00100', '00100', '00100', '00100'],
  0: ['01110', '10001', '10011', '10101', '11001', '10001', '01110'],
  1: ['00100', '01100', '00100', '00100', '00100', '00100', '01110'],
  2: ['01110', '10001', '00001', '00010', '00100', '01000', '11111'],
  3: ['11110', '00001', '00001', '01110', '00001', '00001', '11110'],
  4: ['00010', '00110', '01010', '10010', '11111', '00010', '00010'],
  5: ['11111', '10000', '11110', '00001', '00001', '10001', '01110'],
  6: ['00110', '01000', '10000', '11110', '10001', '10001', '01110'],
  7: ['11111', '00001', '00010', '00100', '01000', '01000', '01000'],
  8: ['01110', '10001', '10001', '01110', '10001', '10001', '01110'],
  9: ['01110', '10001', '10001', '01111', '00001', '00010', '01100'],
  '-': ['00000', '00000', '00000', '11111', '00000', '00000', '00000'],
  '.': ['00000', '00000', '00000', '00000', '00000', '00100', '00100'],
  '%': ['11001', '11010', '00010', '00100', '01000', '01011', '10011'],
  ':': ['00000', '00100', '00100', '00000', '00100', '00100', '00000'],
  ' ': ['00000', '00000', '00000', '00000', '00000', '00000', '00000']
};

function drawText(png, text, x, y, scale, rgb) {
  let cursor = x;
  for (const char of text.toUpperCase()) {
    const glyph = GLYPHS[char] || GLYPHS[' '];
    glyph.forEach((row, gy) => {
      [...row].forEach((bit, gx) => {
        if (bit !== '1') return;
        for (let sy = 0; sy < scale; sy += 1) {
          for (let sx = 0; sx < scale; sx += 1) {
            const px = cursor + gx * scale + sx;
            const py = y + gy * scale + sy;
            if (px < 0 || py < 0 || px >= png.width || py >= png.height) continue;
            const idx = (py * png.width + px) * 4;
            png.data[idx] = rgb[0]; png.data[idx + 1] = rgb[1]; png.data[idx + 2] = rgb[2]; png.data[idx + 3] = 255;
          }
        }
      });
    });
    cursor += 6 * scale;
  }
}

function fill(png, x, y, w, h, rgb) {
  for (let py = y; py < y + h; py += 1) {
    for (let px = x; px < x + w; px += 1) {
      if (px < 0 || py < 0 || px >= png.width || py >= png.height) continue;
      const idx = (py * png.width + px) * 4;
      png.data[idx] = rgb[0]; png.data[idx + 1] = rgb[1]; png.data[idx + 2] = rgb[2]; png.data[idx + 3] = 255;
    }
  }
}

function blit(target, source, dx, dy) {
  for (let y = 0; y < source.height; y += 1) {
    for (let x = 0; x < source.width; x += 1) {
      const tx = dx + x; const ty = dy + y;
      if (tx >= target.width || ty >= target.height) continue;
      const s = (y * source.width + x) * 4; const t = (ty * target.width + tx) * 4;
      target.data[t] = source.data[s]; target.data[t + 1] = source.data[s + 1]; target.data[t + 2] = source.data[s + 2]; target.data[t + 3] = 255;
    }
  }
}

function boundingBoxes(diff, w, h, cell = 32) {
  // Coarse grid of cells that contain any differing pixel, merged into boxes by row.
  const cells = [];
  for (let cy = 0; cy < Math.ceil(h / cell); cy += 1) {
    for (let cx = 0; cx < Math.ceil(w / cell); cx += 1) {
      let hit = false;
      for (let y = cy * cell; y < Math.min(h, (cy + 1) * cell) && !hit; y += 1) {
        for (let x = cx * cell; x < Math.min(w, (cx + 1) * cell); x += 1) {
          const idx = (y * w + x) * 4;
          if (diff.data[idx] === 255 && diff.data[idx + 1] === 0 && diff.data[idx + 2] === 0) { hit = true; break; }
        }
      }
      if (hit) cells.push({ x: cx * cell, y: cy * cell, w: Math.min(cell, w - cx * cell), h: Math.min(cell, h - cy * cell) });
    }
  }
  return cells;
}

async function compareRow(row) {
  const dir = join(evidenceRoot, row.id);
  const referencePath = join(dir, 'reference.png');
  const builtPath = join(dir, 'built.png');
  if (!(await exists(referencePath)) || !(await exists(builtPath))) return { rowId: row.id, skipped: 'both captures are required' };
  const reference = PNG.sync.read(await readFile(referencePath));
  const built = PNG.sync.read(await readFile(builtPath));
  const w = Math.min(reference.width, built.width);
  const h = Math.min(reference.height, built.height);
  const crop = (png) => {
    const out = new PNG({ width: w, height: h });
    for (let y = 0; y < h; y += 1) {
      png.data.copy(out.data, y * w * 4, y * png.width * 4, y * png.width * 4 + w * 4);
    }
    return out;
  };
  const refCrop = crop(reference);
  const builtCrop = crop(built);
  const diff = new PNG({ width: w, height: h });
  const mismatched = pixelmatch(refCrop.data, builtCrop.data, diff.data, w, h, { threshold, includeAA: false, diffColor: [255, 0, 0], diffColorAlt: [255, 0, 0] });
  const ratio = mismatched / (w * h);
  const boxes = boundingBoxes(diff, w, h);

  const strip = 26; const gap = 8; const scale = 2;
  const comparison = new PNG({ width: w * 3 + gap * 2, height: h + strip });
  fill(comparison, 0, 0, comparison.width, comparison.height, [246, 246, 250]);
  const labels = [`REFERENCE ${reference.width}X${reference.height}`, `BUILT ${built.width}X${built.height}`, `DIFF ${(ratio * 100).toFixed(2)}% ${row.id}`];
  [refCrop, builtCrop, diff].forEach((png, i) => {
    const x = i * (w + gap);
    fill(comparison, x, 0, w, strip, i === 2 ? [180, 30, 30] : [30, 30, 40]);
    drawText(comparison, labels[i], x + 8, 6, scale, [255, 255, 255]);
    blit(comparison, png, x, strip);
  });
  const comparisonBytes = PNG.sync.write(comparison);
  const comparisonPath = join(dir, 'comparison.png');
  await writeFile(comparisonPath, comparisonBytes);

  const result = {
    schemaVersion: 1, rowId: row.id, threshold, comparedWidth: w, comparedHeight: h,
    referenceSize: { width: reference.width, height: reference.height },
    builtSize: { width: built.width, height: built.height },
    sizeMismatch: reference.width !== built.width || reference.height !== built.height,
    mismatchedPixels: mismatched, mismatchRatio: ratio, differingCells: boxes.length, cellSize: 32, boxes: boxes.slice(0, 400),
    referenceSha256: sha256(await readFile(referencePath)), builtSha256: sha256(await readFile(builtPath)), comparisonSha256: sha256(comparisonBytes),
    generatedAt: new Date().toISOString()
  };
  const diffBytes = Buffer.from(JSON.stringify(result, null, 2) + '\n', 'utf8');
  await writeFile(join(dir, 'diff.json'), diffBytes);
  result.diffSha256 = sha256(diffBytes);
  const receiptPath = join(dir, 'capture-receipt.json');
  if (await exists(receiptPath)) {
    const receipt = JSON.parse(await readFile(receiptPath, 'utf8'));
    receipt.comparison = { png: 'comparison.png', sha256: result.comparisonSha256, diff: 'diff.json', diffSha256: result.diffSha256, mismatchRatio: ratio, sizeMismatch: result.sizeMismatch };
    await writeFile(receiptPath, JSON.stringify(receipt, null, 2) + '\n');
  }
  return result;
}

const inventory = JSON.parse(await readFile(inventoryPath, 'utf8'));
for (const row of inventory.rows) {
  if (rowFilter && !rowFilter.has(row.id)) continue;
  const result = await compareRow(row);
  if (result.skipped) console.log(`${row.id}: skipped (${result.skipped})`);
  else console.log(`${row.id}: ${(result.mismatchRatio * 100).toFixed(2)}% mismatch, ${result.differingCells} cells${result.sizeMismatch ? ', SIZE MISMATCH' : ''}`);
}
