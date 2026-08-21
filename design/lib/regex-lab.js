// The regex engine behind every search bar and the standalone builder.
//
// Two dialects are described and one is executed. The prototype runs ECMAScript
// RegExp because that is what a browser has; the Qt build runs
// QRegularExpression (PCRE2). Where the two disagree the difference is named
// rather than smoothed over — see DIALECTS and translate().

export const DIALECTS = {
  js: {
    id: 'js',
    label: 'ECMAScript (RegExp)',
    yue: 'JS 引擎',
    flags: [
      { f: 'i', en: 'ignore case', yue: '唔理大細楷' },
      { f: 'm', en: 'multiline ^ $', yue: '逐行錨點' },
      { f: 's', en: 'dot matches newline', yue: '點號食換行' },
      { f: 'u', en: 'unicode mode', yue: 'Unicode 模式' },
      { f: 'g', en: 'global', yue: '全部' },
      { f: 'y', en: 'sticky', yue: '黐住' }
    ],
    notes: [
      'Lookbehind is supported in modern engines but is not in the ES2018 baseline everywhere.',
      '\\p{…} requires the u or v flag.',
      'No possessive quantifiers, no atomic groups, no recursion.'
    ]
  },
  qt: {
    id: 'qt',
    label: 'QRegularExpression (PCRE2)',
    yue: 'Qt 引擎',
    flags: [
      { f: 'CaseInsensitiveOption', en: 'ignore case', yue: '唔理大細楷' },
      { f: 'MultilineOption', en: 'multiline ^ $', yue: '逐行錨點' },
      { f: 'DotMatchesEverythingOption', en: 'dot matches newline', yue: '點號食換行' },
      { f: 'UseUnicodePropertiesOption', en: 'unicode properties', yue: 'Unicode 屬性' },
      { f: 'ExtendedPatternSyntaxOption', en: 'extended / free-spacing', yue: '自由排版' },
      { f: 'InvertedGreedinessOption', en: 'inverted greediness', yue: '反貪心' }
    ],
    notes: [
      'Supports atomic groups (?>…), possessive quantifiers a++ and recursion (?R).',
      'Unicode properties need UseUnicodePropertiesOption explicitly.',
      'Backslashes double inside a C++ string literal: \\d becomes "\\\\d".'
    ]
  }
};

export const PRESETS = [
  { id: 'url-host', name: 'URL host', yue: '網址主機', pattern: '^https?://(?<host>[^/:?#]+)', flags: 'i', sample: 'https://service.example/console\nhttp://host.example:5432/status' },
  { id: 'email', name: 'Email address', yue: '電郵地址', pattern: '(?<local>[\\w.+-]+)@(?<domain>[\\w-]+\\.[\\w.-]+)', flags: 'gi', sample: 'ops@acme.example\nme@fastmail.example\nnot-an-email@' },
  { id: 'masked', name: 'Masked account tail', yue: '尾四位', pattern: '•{2,}(?<tail>\\d{3,4})\\b', flags: 'g', sample: '••••3391\n••••7742\n••••1180' },
  { id: 'ipv4', name: 'IPv4 address', yue: 'IPv4', pattern: '\\b(?:(?:25[0-5]|2[0-4]\\d|1?\\d?\\d)\\.){3}(?:25[0-5]|2[0-4]\\d|1?\\d?\\d)\\b', flags: 'g', sample: '192.0.2.10\n198.51.100.42\n999.1.1.1' },
  { id: 'uuid', name: 'UUID v4', yue: 'UUID', pattern: '\\b[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}\\b', flags: 'gi', sample: '9f2c1ab0-3d17-4b7b-9c33-2bf39c001188' },
  { id: 'kpxc-ref', name: 'KeePassXC field reference', yue: '欄位引用', pattern: '\\{REF:(?<want>[TUPAN])@(?<by>[TUPANIO]):(?<query>[^}]+)\\}', flags: 'g', sample: '{REF:P@T:AWS — production root}\n{REF:U@I:9f2c1ab0}' },
  { id: 'cjk', name: 'Any CJK character', yue: '中文字', pattern: '\\p{Script=Han}+', flags: 'gu', sample: '帶子蝦餃 scallop har gow 筍尖蝦餃' },
  { id: 'dupe-word', name: 'Doubled word (backreference)', yue: '重複字', pattern: '\\b(\\w+)\\s+\\1\\b', flags: 'gi', sample: 'the the vault is is fine' },
  { id: 'not-prod', name: 'Host that is not prod (lookahead)', yue: '唔係 prod', pattern: '^(?!.*prod)(?<name>[\\w.-]+)$', flags: 'm', sample: 'db.staging.internal\ndb.prod.internal\ncache.dev.internal' },
  { id: 'after-colon', name: 'Value after a key (lookbehind)', yue: '冒號之後', pattern: '(?<=password:\\s)(?<value>\\S+)', flags: 'gi', sample: 'username: ops\npassword: •••••••••' }
];

// ── token model ────────────────────────────────────────────────────────────
// A flat token list, each with the source span it came from. Enough for the
// explainer, the visual blocks and the reorder handle; not a full AST.

const CLASS_NAMES = {
  d: ['any digit 0-9', '數字 0-9'],
  D: ['anything that is not a digit', '唔係數字'],
  w: ['word character: letter, digit or _', '字母、數字或者底線'],
  W: ['not a word character', '唔係字母數字'],
  s: ['whitespace', '空白位'],
  S: ['not whitespace', '唔係空白'],
  b: ['word boundary', '字界'],
  B: ['not a word boundary', '唔係字界'],
  n: ['newline', '換行'],
  t: ['tab', 'Tab'],
  r: ['carriage return', '回車']
};

export function tokenize(src) {
  const out = [];
  let i = 0;
  let groupNo = 0;
  const push = (type, text, en, yue, extra) =>
    out.push(Object.assign({ type, text, en, yue, start: i, end: i + text.length }, extra || {}));

  while (i < src.length) {
    const c = src[i];

    if (c === '\\') {
      const n = src[i + 1];
      if (n === 'p' || n === 'P') {
        const close = src.indexOf('}', i);
        if (src[i + 2] === '{' && close > -1) {
          const body = src.slice(i + 3, close);
          const text = src.slice(i, close + 1);
          push('unicode', text, `Unicode property ${body}${n === 'P' ? ' (negated)' : ''}`, `Unicode 屬性 ${body}`);
          i = close + 1;
          continue;
        }
      }
      if (n === 'k' && src[i + 2] === '<') {
        const close = src.indexOf('>', i);
        const name = src.slice(i + 3, close);
        const text = src.slice(i, close + 1);
        push('backref', text, `back-reference to group “${name}”`, `回頭引用 “${name}”`);
        i = close + 1;
        continue;
      }
      if (/[1-9]/.test(n)) {
        push('backref', '\\' + n, `back-reference to group ${n}`, `回頭引用 第${n}組`);
        i += 2;
        continue;
      }
      if (CLASS_NAMES[n]) {
        const kind = n === 'b' || n === 'B' ? 'anchor' : 'class';
        push(kind, '\\' + n, CLASS_NAMES[n][0], CLASS_NAMES[n][1]);
        i += 2;
        continue;
      }
      push('escape', '\\' + n, `a literal ${n}`, `真係個 ${n} 字`);
      i += 2;
      continue;
    }

    if (c === '[') {
      let j = i + 1, depth = 1;
      if (src[j] === '^') j++;
      if (src[j] === ']') j++;
      while (j < src.length && depth > 0) {
        if (src[j] === '\\') j += 2;
        else if (src[j] === ']') { depth--; j++; }
        else j++;
      }
      const text = src.slice(i, j);
      const neg = text[1] === '^';
      push('charclass', text, neg ? 'any character NOT in this set' : 'any one character from this set',
        neg ? '唔喺呢個範圍嘅字' : '呢個範圍入面任何一個字');
      i = j;
      continue;
    }

    if (c === '(') {
      const head3 = src.slice(i, i + 3);
      const head4 = src.slice(i, i + 4);
      if (head3 === '(?:') { push('group', '(?:', 'group, not captured', '分組但唔捕捉'); i += 3; continue; }
      if (head3 === '(?=') { push('look', '(?=', 'lookahead: what follows must match', '向前望：後面要係咁'); i += 3; continue; }
      if (head3 === '(?!') { push('look', '(?!', 'negative lookahead: what follows must NOT match', '向前望：後面唔可以係咁'); i += 3; continue; }
      if (head4 === '(?<=') { push('look', '(?<=', 'lookbehind: what precedes must match', '向後望：前面要係咁'); i += 4; continue; }
      if (head4 === '(?<!') { push('look', '(?<!', 'negative lookbehind: what precedes must NOT match', '向後望：前面唔可以係咁'); i += 4; continue; }
      if (head3 === '(?>') { push('group', '(?>', 'atomic group — PCRE2 only, not ECMAScript', '原子組 — 淨係 Qt 有'); i += 3; continue; }
      if (src.slice(i, i + 3) === '(?<') {
        const close = src.indexOf('>', i);
        const name = src.slice(i + 3, close);
        groupNo++;
        push('group', src.slice(i, close + 1), `capture group ${groupNo}, named “${name}”`, `第${groupNo}組，叫 “${name}”`, { name, no: groupNo });
        i = close + 1;
        continue;
      }
      groupNo++;
      push('group', '(', `capture group ${groupNo}`, `第${groupNo}組`, { no: groupNo });
      i++;
      continue;
    }

    if (c === ')') { push('group', ')', 'end of group', '組完'); i++; continue; }
    if (c === '|') { push('alt', '|', 'or — either side may match', '或者'); i++; continue; }
    if (c === '^') { push('anchor', '^', 'start of the string (or line with m)', '開頭'); i++; continue; }
    if (c === '$') { push('anchor', '$', 'end of the string (or line with m)', '結尾'); i++; continue; }
    if (c === '.') { push('dot', '.', 'any character except newline (unless s)', '任何一個字'); i++; continue; }

    if (c === '*' || c === '+' || c === '?') {
      const lazy = src[i + 1] === '?';
      const poss = src[i + 1] === '+';
      const text = src.slice(i, i + (lazy || poss ? 2 : 1));
      const base = c === '*' ? 'zero or more' : c === '+' ? 'one or more' : 'optional — zero or one';
      const baseYue = c === '*' ? '零次或者更多' : c === '+' ? '一次或者更多' : '有冇都得';
      push('quant', text,
        base + (lazy ? ', lazy (as few as possible)' : poss ? ', possessive — PCRE2 only' : ', greedy'),
        baseYue + (lazy ? '，懶（要幾少有幾少）' : poss ? '，霸住唔放（淨係 Qt 有）' : '，貪心'));
      i += text.length;
      continue;
    }

    if (c === '{') {
      const close = src.indexOf('}', i);
      if (close > -1 && /^\{\d+(,\d*)?\}$/.test(src.slice(i, close + 1))) {
        const lazy = src[close + 1] === '?';
        const text = src.slice(i, close + 1 + (lazy ? 1 : 0));
        const body = src.slice(i + 1, close);
        push('quant', text, `repeated ${body.replace(',', ' to ').replace(/ to $/, ' or more')} times${lazy ? ', lazy' : ''}`,
          `重複 ${body} 次${lazy ? '，懶' : ''}`);
        i += text.length;
        continue;
      }
    }

    let j = i;
    while (j < src.length && !'\\[](){}|^$.*+?'.includes(src[j])) j++;
    if (j === i) j = i + 1;
    const text = src.slice(i, j);
    push('literal', text, `the text “${text}”`, `“${text}” 呢啲字`);
    i = j;
  }
  return out;
}

// ── safety ─────────────────────────────────────────────────────────────────
// Catastrophic backtracking is detected two ways: a static shape check for a
// quantifier applied to a group that already quantifies, and a hard wall-clock
// budget around every evaluation. Neither is a proof; both are cheap.

const RISK_SHAPES = [
  { re: /\([^)]*[+*][^)]*\)\s*[+*]/, en: 'a quantified group whose body is itself quantified — (a+)+', yue: '組入面有 +，組外面又有 +' },
  { re: /\([^)]*\|[^)]*\)\s*[+*]/, en: 'a quantified alternation — (a|a)*', yue: '有 | 嘅組再加 *' },
  { re: /\.\*\.\*/, en: 'two greedy .* in sequence', yue: '兩個 .* 排埋一齊' }
];

export function riskReport(pattern) {
  const hits = [];
  for (const s of RISK_SHAPES) if (s.re.test(pattern)) hits.push({ en: s.en, yue: s.yue });
  return hits;
}

export const LIMITS = { pattern: 512, sample: 20000, budgetMs: 120, maxMatches: 500 };

export function compile(pattern, flags) {
  if (!pattern) return { ok: false, empty: true, error: null, re: null };
  if (pattern.length > LIMITS.pattern) {
    return { ok: false, error: `Pattern is longer than ${LIMITS.pattern} characters.`, re: null };
  }
  try {
    const f = flags.includes('g') ? flags : flags + 'g';
    return { ok: true, error: null, re: new RegExp(pattern, f) };
  } catch (err) {
    return { ok: false, error: String(err.message || err), re: null };
  }
}

export function run(pattern, flags, sample) {
  const c = compile(pattern, flags);
  if (!c.ok) return { ok: false, empty: !!c.empty, error: c.error, matches: [], names: [], truncated: false, ms: 0, timedOut: false };
  const text = sample.length > LIMITS.sample ? sample.slice(0, LIMITS.sample) : sample;
  const started = performance.now();
  const matches = [];
  let timedOut = false;
  let last = -1;
  c.re.lastIndex = 0;
  let m;
  while ((m = c.re.exec(text)) !== null) {
    if (m.index === last && m[0] === '') { c.re.lastIndex++; }
    last = m.index;
    matches.push({
      text: m[0],
      index: m.index,
      groups: m.slice(1),
      named: m.groups ? Object.assign({}, m.groups) : null
    });
    if (m[0] === '') c.re.lastIndex++;
    if (matches.length >= LIMITS.maxMatches) break;
    if (performance.now() - started > LIMITS.budgetMs) { timedOut = true; break; }
  }
  const names = [];
  const nameRe = /\(\?<([A-Za-z_$][\w$]*)>/g;
  let nm;
  while ((nm = nameRe.exec(pattern)) !== null) names.push(nm[1]);
  return {
    ok: true, error: null, matches, names, timedOut,
    truncated: matches.length >= LIMITS.maxMatches,
    ms: Math.round((performance.now() - started) * 10) / 10
  };
}

export function substitute(pattern, flags, sample, replacement) {
  const c = compile(pattern, flags);
  if (!c.ok) return { ok: false, error: c.error, out: '' };
  try {
    return { ok: true, error: null, out: sample.replace(c.re, replacement) };
  } catch (err) {
    return { ok: false, error: String(err.message || err), out: '' };
  }
}

// ── export ─────────────────────────────────────────────────────────────────

export function translate(pattern, flags) {
  const cpp = pattern.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
  const qtOpts = [];
  if (flags.includes('i')) qtOpts.push('QRegularExpression::CaseInsensitiveOption');
  if (flags.includes('m')) qtOpts.push('QRegularExpression::MultilineOption');
  if (flags.includes('s')) qtOpts.push('QRegularExpression::DotMatchesEverythingOption');
  if (flags.includes('u')) qtOpts.push('QRegularExpression::UseUnicodePropertiesOption');
  const pyFlags = [];
  if (flags.includes('i')) pyFlags.push('re.IGNORECASE');
  if (flags.includes('m')) pyFlags.push('re.MULTILINE');
  if (flags.includes('s')) pyFlags.push('re.DOTALL');
  return {
    js: `/${pattern}/${flags}`,
    qt: `QRegularExpression re(QStringLiteral("${cpp}")${qtOpts.length ? ',\n                       ' + qtOpts.join(' |\n                       ') : ''});`,
    python: `re.compile(r"${pattern}"${pyFlags.length ? ', ' + pyFlags.join(' | ') : ''})`,
    grep: `grep -P${flags.includes('i') ? 'i' : ''} '${pattern.replace(/'/g, `'\\''`)}'`
  };
}

// Plain-text search that every surface uses when regex mode is off. Returned in
// the same shape as run() so a surface does not branch on which mode it is in.
export function plainFilter(rows, query, keyOf) {
  const q = query.trim().toLowerCase();
  if (!q) return rows;
  return rows.filter(r => keyOf(r).toLowerCase().includes(q));
}

export function regexFilter(rows, pattern, flags, keyOf) {
  const c = compile(pattern, flags.replace('g', ''));
  if (!c.ok) return rows;
  const re = new RegExp(pattern, flags.replace('g', ''));
  return rows.filter(r => { re.lastIndex = 0; return re.test(keyOf(r)); });
}
