// Promote the per-row capture receipts, diffs and audits into inventory.json.
//
//   node design/parity/promote-evidence.mjs
//
// Every hash written here is recomputed from the bytes on disk, never copied
// from a receipt, so a stale or edited artifact cannot pass as current. A row
// is marked complete only when both raw captures, the comparison, the diff and
// a complete audit exist for the same source commit.
import { createHash } from 'node:crypto';
import { readFile, writeFile, access } from 'node:fs/promises';
import { dirname, join, relative } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(here, '..', '..');
const inventoryPath = join(here, 'inventory.json');
const exists = async path => access(path).then(() => true, () => false);
const digest = async path => createHash('sha256').update(await readFile(path)).digest('hex');
const rel = path => relative(repoRoot, path).split('\\').join('/');

const inventory = JSON.parse(await readFile(inventoryPath, 'utf8'));
const report = [];
for (const row of inventory.rows) {
  const dir = join(here, 'evidence', row.id);
  const receiptPath = join(dir, 'capture-receipt.json');
  if (!(await exists(receiptPath))) { report.push(`${row.id}: no capture receipt`); continue; }
  const receipt = JSON.parse(await readFile(receiptPath, 'utf8'));
  const files = {
    referenceRaw: join(dir, 'reference.png'),
    builtRaw: join(dir, 'built.png'),
    comparison: join(dir, 'comparison.png'),
    diff: join(dir, 'diff.json')
  };
  const missing = [];
  for (const [field, path] of Object.entries(files)) {
    if (!(await exists(path))) { missing.push(field); continue; }
    row.evidence[field] = rel(path);
    row.evidence[`${field}Sha256`] = await digest(path);
  }
  const auditPath = join(repoRoot, row.audit.path);
  let auditStatus = 'pending';
  if (await exists(auditPath)) {
    const audit = JSON.parse(await readFile(auditPath, 'utf8'));
    auditStatus = audit.status === 'complete' ? 'complete' : 'pending';
    row.audit.status = auditStatus;
    row.audit.sha256 = await digest(auditPath);
    if (auditStatus === 'complete') delete row.audit.reason;
    else row.audit.reason = audit.reason || 'Audit file is not marked complete.';
    row.deviations = Array.isArray(audit.deviations) ? audit.deviations : row.deviations;
  }
  row.sourceCommit = receipt.sourceCommit || 'pending';
  const tools = [receipt.reference && receipt.reference.tool, receipt.built && receipt.built.tool].filter(Boolean);
  row.captureTool = tools.length ? tools.join(' | ') : 'pending';
  row.builtRouteStatus = receipt.built && receipt.built.sha256 ? 'implemented-captured' : row.builtRouteStatus;
  row.evidence.status = missing.length === 0 && auditStatus === 'complete' && row.sourceCommit !== 'pending' ? 'complete' : 'pending';
  if (receipt.comparison) row.evidence.mismatchRatio = receipt.comparison.mismatchRatio;
  report.push(`${row.id}: evidence ${row.evidence.status}${missing.length ? ` (missing ${missing.join(', ')})` : ''}, audit ${auditStatus}, commit ${row.sourceCommit.slice(0, 12)}`);
}
await writeFile(inventoryPath, JSON.stringify(inventory, null, 2) + '\n');
console.log(report.join('\n'));
