import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { validateStructure } from './check-parity.mjs';

const here = dirname(fileURLToPath(import.meta.url));
const original = JSON.parse(readFileSync(join(here, 'inventory.json'), 'utf8'));

function clone() {
  return JSON.parse(JSON.stringify(original));
}

function expectRed(name, mutate) {
  const candidate = clone();
  mutate(candidate);
  const errors = validateStructure(candidate);
  if (!errors.length) throw new Error(`${name}: guard stayed green`);
  process.stdout.write(`RED: ${name} — ${errors[0]}\n`);
}

const green = validateStructure(original);
if (green.length) throw new Error(`baseline is red: ${green.join('; ')}`);
process.stdout.write('GREEN: baseline structure and reference hashes\n');

expectRed('reference omitted', inventory => inventory.rows.pop());
expectRed('duplicate row id', inventory => { inventory.rows[1].id = inventory.rows[0].id; });
expectRed('missing reference route', inventory => { inventory.rows[0].referenceRoute = ''; });
expectRed('missing built route', inventory => { inventory.rows[0].builtRoute = ''; });
expectRed('tuple screen mismatch', inventory => { inventory.rows[0].tuple.screen = 'wrong-screen'; });
expectRed('tuple viewport field missing', inventory => { delete inventory.rows[0].tuple.viewport.width; });
expectRed('deterministic input missing', inventory => { delete inventory.rows[0].determinism.random; });
expectRed('audit missing', inventory => { delete inventory.rows[0].audit; });
expectRed('evidence field missing', inventory => { delete inventory.rows[0].evidence.diff; });
expectRed('deviation reason missing', inventory => { inventory.rows[0].deviations = [{ approval: 'review-1' }]; });
expectRed('deviation approval missing', inventory => { inventory.rows[0].deviations = [{ reason: 'Reviewed difference' }]; });
expectRed('stale reference hash', inventory => { inventory.rows[0].referenceSha256 = '0'.repeat(64); });

const restored = validateStructure(original);
if (restored.length) throw new Error(`restored baseline is red: ${restored.join('; ')}`);
process.stdout.write('GREEN: restored baseline after every negative probe\n');
