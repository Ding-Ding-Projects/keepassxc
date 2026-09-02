#!/usr/bin/env node
// Counts this repository's lines of code and prints the table the release and
// the README publish.
//
// Only files Git tracks are counted, so build output, dependency trees and
// anything ignored are excluded by construction. Vendored third-party trees are
// tracked but are not this project's code, so they land in their own excluded
// row; generated files inside the project total are reported separately so a
// reader can see how much a person actually wrote. Attribution is per
// surviving line via `git blame`: a commit counts as agent-written when its
// author is an automation identity or it carries a Co-Authored-By trailer
// naming an agent.
//
//   node scripts/count-lines.mjs [--json] [--no-attribution]
import { execSync, spawn } from 'node:child_process';
import { readFileSync, statSync } from 'node:fs';

const CountedExtensions = new Set([
  'cpp', 'h', 'c', 'hpp', 'ui', 'qrc', 'cmake', 'txt', 'in',
  'ps1', 'bat', 'sh', 'py',
  'mjs', 'js', 'ts', 'json', 'css', 'html',
  'md', 'adoc', 'yml', 'yaml', 'xml', 'ts', 'rc', 'nuspec'
]);
const MaximumFileBytes = 8 * 1024 * 1024;

// Files a generator wrote. They stay inside the project total but are reported
// on their own line so the hand-written figure is visible.
const GeneratedPatterns = [
  /^share\/changelog\/.*\.json$/,
  /^design\/parity\/evidence\/.*\.json$/,
  /^design\/parity\/inventory\.json$/,
  /^share\/translations\/keepassxc_.*\.ts$/
];

// Most specific first. The last row is a catch-all so nothing is dropped silently.
const Areas = [
  { name: 'Vendored reference runtime and fonts', test: /^design\/lib\/vendor\//, project: false },
  { name: 'Bundled third-party sources', test: /^src\/thirdparty\//, project: false },
  { name: 'vcpkg and toolchain manifests', test: /^vcpkg\//, project: false },
  { name: 'Upstream documentation sources', test: /^docs\/(topics|images|styles|man)\//, project: false },
  { name: 'Translations', test: /^share\/translations\//, project: false },
  { name: 'Layout probe dumps (clipping evidence)', test: /^design\/parity\/evidence\/clipping\/.*\.json$/, project: false },
  { name: 'Design references and parity tooling', test: /^design\//, project: true },
  { name: 'Material UI (src/gui/material)', test: /^src\/gui\/material\//, project: true },
  { name: 'Application source (src)', test: /^src\//, project: true },
  { name: 'Tests', test: /^tests\//, project: true },
  { name: 'Scripts and build entry points', test: /^(scripts\/|build.*\.bat$|download-dependencies\.bat$|release-tool\.py$)/, project: true },
  { name: 'Packaging', test: /^packaging\//, project: true },
  { name: 'CI workflows', test: /^\.github\//, project: true },
  { name: 'CMake modules', test: /^cmake\//, project: true },
  { name: 'Shared resources (share)', test: /^share\//, project: true },
  { name: 'Feature documentation, site and wiki', test: /^(docs|site|wiki)\//, project: true },
  { name: 'Utilities and ports (utils)', test: /^utils\//, project: true },
  { name: 'Translation tooling config (.tx)', test: /^\.tx\//, project: false },
  { name: 'Repository root', test: /^[^/]+$/, project: true },
  { name: 'Unclassified', test: /.*/, project: true }
];

const AgentAuthorPattern = /^(claude|codex|opencode|copilot|dependabot)/i;
const AgentTrailerPattern = /claude|codex|opencode|copilot/i;

function repositoryFiles() {
  return execSync('git ls-files -z', { maxBuffer: 1 << 28 }).toString('utf8').split('\0').filter(Boolean);
}

export function agentCommits() {
  const raw = execSync('git log --format=%H%x01%an%x01%(trailers:key=Co-Authored-By,valueonly,separator=%x02)', { maxBuffer: 1 << 28 }).toString('utf8');
  const agents = new Set();
  for (const line of raw.split('\n')) {
    if (!line) continue;
    const [sha, author = '', trailers = ''] = line.split('\x01');
    if (AgentAuthorPattern.test(author) || AgentTrailerPattern.test(trailers)) agents.add(sha);
  }
  return agents;
}

async function attributeLines(files, agents, concurrency = 8) {
  const totals = { agent: 0, human: 0 };
  let next = 0;
  async function blameOne(file) {
    const counts = await new Promise(resolve => {
      const child = spawn('git', ['blame', '--line-porcelain', '--', file], { stdio: ['ignore', 'pipe', 'ignore'] });
      let buffered = '';
      const local = { agent: 0, human: 0 };
      child.stdout.setEncoding('utf8');
      child.stdout.on('data', chunk => {
        buffered += chunk;
        const lines = buffered.split('\n');
        buffered = lines.pop() ?? '';
        for (const line of lines) {
          const match = /^([0-9a-f]{40}) \d+ \d+(?: \d+)?$/.exec(line);
          if (match) { if (agents.has(match[1])) local.agent++; else local.human++; }
        }
      });
      child.on('close', () => resolve(local));
      child.on('error', () => resolve(null));
    });
    if (counts) { totals.agent += counts.agent; totals.human += counts.human; }
  }
  await Promise.all(Array.from({ length: concurrency }, async () => { while (next < files.length) await blameOne(files[next++]); }));
  return totals;
}

export async function countRepository({ attribution = true } = {}) {
  const rows = new Map();
  const generated = { files: 0, lines: 0, nonBlank: 0 };
  const counted = [];
  for (const file of repositoryFiles()) {
    const extension = (file.split('.').pop() ?? '').toLowerCase();
    const base = file.split('/').pop();
    if (!CountedExtensions.has(extension) && base !== 'CMakeLists.txt') continue;
    let text;
    try {
      if (statSync(file).size > MaximumFileBytes) continue;
      text = readFileSync(file, 'utf8');
    } catch { continue; }
    if (text.includes('\0')) continue;
    const all = text.length === 0 ? [] : text.split('\n');
    if (all.length > 0 && all[all.length - 1] === '') all.pop();
    const lines = all.length;
    const nonBlank = all.filter(line => line.trim() !== '').length;
    const area = Areas.find(candidate => candidate.test.test(file));
    if (area.project && GeneratedPatterns.some(pattern => pattern.test(file))) {
      generated.files++; generated.lines += lines; generated.nonBlank += nonBlank;
    }
    const row = rows.get(area.name) ?? { name: area.name, project: area.project, files: 0, lines: 0, nonBlank: 0 };
    row.files++; row.lines += lines; row.nonBlank += nonBlank;
    rows.set(area.name, row);
    if (area.project) counted.push(file);
  }
  const ordered = [...rows.values()].sort((a, b) => b.lines - a.lines);
  const project = ordered.filter(row => row.project);
  const sum = (list, key) => list.reduce((total, row) => total + row[key], 0);
  let authored = null;
  let attributionError = null;
  if (attribution) {
    try { authored = await attributeLines(counted, agentCommits()); } catch (error) { authored = null; attributionError = String(error && error.message ? error.message : error); }
  }
  let commit = 'unknown';
  try { commit = execSync('git rev-parse --short HEAD').toString().trim(); } catch {}
  const result = {
    rows: ordered, generated,
    project: { files: sum(project, 'files'), lines: sum(project, 'lines'), nonBlank: sum(project, 'nonBlank') },
    everything: { files: sum(ordered, 'files'), lines: sum(ordered, 'lines'), nonBlank: sum(ordered, 'nonBlank') },
    authored, attributionError, commit
  };
  // The attribution total must equal the project line total, or the counter is wrong.
  if (authored && authored.agent + authored.human !== result.project.lines) {
    result.consistency = { attributed: authored.agent + authored.human, projectLines: result.project.lines, agrees: false };
  } else if (authored) {
    result.consistency = { attributed: authored.agent + authored.human, projectLines: result.project.lines, agrees: true };
  }
  return result;
}

const number = value => value.toLocaleString('en-US');

export function markdown(result) {
  const lines = ['| Area | Files | Lines | Non-blank |', '| --- | ---: | ---: | ---: |'];
  for (const row of result.rows) {
    lines.push(`| ${row.project ? row.name : `${row.name} *(excluded)*`} | ${number(row.files)} | ${number(row.lines)} | ${number(row.nonBlank)} |`);
  }
  lines.push(`| **Project total** | **${number(result.project.files)}** | **${number(result.project.lines)}** | **${number(result.project.nonBlank)}** |`);
  lines.push(`| **Everything counted** | **${number(result.everything.files)}** | **${number(result.everything.lines)}** | **${number(result.everything.nonBlank)}** |`);
  lines.push('');
  lines.push(`Of the project total, ${number(result.generated.lines)} lines across ${number(result.generated.files)} files are generated by tooling rather than written by hand. Excluded rows are vendored, bundled third-party, upstream documentation and translation files that are not this project's code.`);
  if (result.authored) {
    const { agent, human } = result.authored;
    const attributed = agent + human;
    const share = attributed === 0 ? 0 : Math.round((agent / attributed) * 1000) / 10;
    lines.push('', '| Written by | Lines | Share |', '| --- | ---: | ---: |');
    lines.push(`| Agents | ${number(agent)} | ${share}% |`);
    lines.push(`| People | ${number(human)} | ${Math.round((100 - share) * 10) / 10}% |`);
    lines.push(`| **Total attributed** | **${number(attributed)}** | **100%** |`);
    lines.push('', 'Attribution is per surviving line via `git blame`, not lines added: a line written and later deleted counts for nobody. A commit counts as agent-written when its author is an automation identity (Claude, Codex, OpenCode, Copilot, Dependabot) or it carries a `Co-Authored-By` trailer naming one.');
    if (result.consistency && !result.consistency.agrees) {
      lines.push('', `**Counter inconsistency:** ${number(result.consistency.attributed)} attributed lines versus ${number(result.consistency.projectLines)} project lines. Fix the counter before publishing this table.`);
    }
  }
  if (!result.authored && result.attributionError) {
    lines.push('', `**Attribution unavailable:** ${result.attributionError}. The line totals above stand; the agent/people split could not be measured in this run.`);
  }
  lines.push('', `Measured at ${result.commit} with \`node scripts/count-lines.mjs\`.`);
  return lines.join('\n');
}

const invokedDirectly = process.argv[1] !== undefined && import.meta.url.endsWith(process.argv[1].split(/[\\/]/).pop() ?? ' ');
if (invokedDirectly) {
  const result = await countRepository({ attribution: !process.argv.includes('--no-attribution') });
  if (process.argv.includes('--json')) console.log(JSON.stringify(result, null, 2));
  else console.log(markdown(result));
  if (result.consistency && !result.consistency.agrees) process.exit(2);
}
