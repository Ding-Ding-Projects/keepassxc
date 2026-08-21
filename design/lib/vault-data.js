// Demo vault. Nothing here is a real credential: every "password" is a
// placeholder string, and the health verdicts are authored, not computed from
// anything sensitive. Mirrors the shape MaterialVaultScreen's EntryListModel
// exposes (title / username / url / modified / health / notes / fields).

export const DATABASES = [
  { id: 'personal', name: 'Personal.kdbx', short: 'Personal', entries: 148, locked: false, pinned: true, dirty: false },
  { id: 'work', name: 'Acme Corp.kdbx', short: 'Acme Corp', entries: 312, locked: false, pinned: true, dirty: true },
  { id: 'legacy', name: 'Legacy-2019.kdbx', short: 'Legacy-2019', entries: 61, locked: true, pinned: false, dirty: false },
  { id: 'shared', name: 'Household.kdbx', short: 'Household', entries: 44, locked: false, pinned: false, dirty: false },
  { id: 'archive', name: 'Archive.kdbx', short: 'Archive', entries: 903, locked: false, pinned: false, dirty: false },
  { id: 'travel', name: 'Travel.kdbx', short: 'Travel', entries: 27, locked: false, pinned: false, dirty: false },
  { id: 'devkeys', name: 'Dev Keys.kdbx', short: 'Dev Keys', entries: 88, locked: false, pinned: false, dirty: false }
];

export const GROUPS = [
  { id: 'root', name: 'Personal', yue: '私人', symbol: 'database', depth: 0, count: 148, expanded: true },
  { id: 'banking', name: 'Banking', yue: '銀行', symbol: 'account_balance', depth: 1, count: 19 },
  { id: 'email', name: 'Email', yue: '電郵', symbol: 'mail', depth: 1, count: 12 },
  { id: 'dev', name: 'Development', yue: '開發', symbol: 'terminal', depth: 1, count: 41, expanded: true },
  { id: 'dev-cloud', name: 'Cloud', yue: '雲端', symbol: 'cloud', depth: 2, count: 23 },
  { id: 'dev-vcs', name: 'Version control', yue: '版本控制', symbol: 'commit', depth: 2, count: 8 },
  { id: 'shopping', name: 'Shopping', yue: '購物', symbol: 'shopping_bag', depth: 1, count: 33 },
  { id: 'social', name: 'Social', yue: '社交', symbol: 'group', depth: 1, count: 21 },
  { id: 'utilities', name: 'Utilities', yue: '水電煤', symbol: 'bolt', depth: 1, count: 14 },
  { id: 'recycle', name: 'Recycle Bin', yue: '回收筒', symbol: 'delete', depth: 1, count: 8 }
];

const E = (id, group, title, username, url, health, modified, tags, notes) =>
  ({ id, group, title, username, url, health, modified, tags, notes, password: '••••••••••••••••' });

export const ENTRIES = [
  E('e01', 'dev-cloud', 'AWS — production root', 'ops@acme.example', 'https://console.aws.amazon.com', 'breached', '2026-08-14T09:12:00Z', ['prod', 'critical'], 'Root account. MFA hardware token in the safe. Rotate quarterly.'),
  E('e02', 'dev-vcs', 'GitHub — Ding-Ding-Projects', 'ding-ding-bot', 'https://github.com', 'ok', '2026-08-19T21:40:00Z', ['ci'], 'Fine-grained PAT. Scope: contents, issues, actions.'),
  E('e03', 'banking', 'HSBC HK — personal', '••••3391', 'https://www.hsbc.com.hk', 'ok', '2026-07-02T11:05:00Z', ['bank'], ''),
  E('e04', 'banking', 'Hang Seng — joint account', '••••7742', 'https://bank.hangseng.com', 'reused', '2026-05-18T08:30:00Z', ['bank', 'joint'], 'Shared with household. Same password as the utilities login — fix this.'),
  E('e05', 'email', 'Fastmail', 'me@fastmail.example', 'https://app.fastmail.com', 'ok', '2026-08-01T14:22:00Z', ['email'], ''),
  E('e06', 'email', 'Outlook — Acme', 'first.last@acme.example', 'https://outlook.office.com', 'weak', '2025-11-30T17:45:00Z', ['work'], 'Password policy forces 8 chars. Complain to IT.'),
  E('e07', 'dev-cloud', 'Cloudflare', 'ops@acme.example', 'https://dash.cloudflare.com', 'ok', '2026-08-11T10:02:00Z', ['dns'], ''),
  E('e08', 'dev-cloud', 'Hetzner Cloud', 'ding@acme.example', 'https://console.hetzner.cloud', 'ok', '2026-06-22T19:14:00Z', [], ''),
  E('e09', 'dev', 'Postgres — status-hub', 'statushub', 'postgres://db.internal:5432', 'ok', '2026-08-17T07:55:00Z', ['db'], 'Connection string in the URL field so the browser extension leaves it alone.'),
  E('e10', 'dev', 'Docker Hub', 'dingding', 'https://hub.docker.com', 'reused', '2026-02-09T13:20:00Z', [], ''),
  E('e11', 'shopping', 'HKTVmall', 'ding@fastmail.example', 'https://www.hktvmall.com', 'weak', '2025-09-14T20:11:00Z', [], ''),
  E('e12', 'shopping', 'Taobao', 'dingding_hk', 'https://world.taobao.com', 'ok', '2026-04-27T09:38:00Z', [], ''),
  E('e13', 'social', 'Discord — status bot', 'status-discord-bot', 'https://discord.com/developers', 'ok', '2026-08-20T02:15:00Z', ['bot'], 'Bot token. Rotating breaks the Status Hub webhook until redeploy.'),
  E('e14', 'social', 'Mastodon — hachyderm', '@dingding', 'https://hachyderm.io', 'ok', '2026-03-03T16:44:00Z', [], ''),
  E('e15', 'utilities', 'CLP Power', '••••1180', 'https://www.clp.com.hk', 'reused', '2026-05-18T08:31:00Z', [], ''),
  E('e16', 'utilities', 'Towngas', '••••0455', 'https://www.towngas.com', 'ok', '2026-01-19T12:00:00Z', [], ''),
  E('e17', 'utilities', 'HKBN broadband', '••••2207', 'https://www.hkbn.net', 'weak', '2024-12-02T18:25:00Z', [], 'Six digits. Their portal will not accept anything longer.'),
  E('e18', 'dev-vcs', 'GitLab — mirror', 'ding-ding', 'https://gitlab.com', 'ok', '2026-07-30T22:05:00Z', [], ''),
  E('e19', 'dev', 'Tidbyt device key', 'tidbyt', 'https://api.tidbyt.com', 'ok', '2026-08-05T06:30:00Z', ['iot'], ''),
  E('e20', 'banking', 'Example Brokerage', 'account-4471', 'https://broker.example', 'breached', '2026-06-11T15:50:00Z', ['bank', 'critical'], 'Appeared in the fictional 2026-06 credential-stuffing list. Rotated, but the old value is still in three backups.')
];

export const FIELDS = {
  e01: [
    { key: 'Account ID', value: '4471-8820-1163', protect: false },
    { key: 'MFA serial', value: 'GAHT-88201', protect: false },
    { key: 'Break-glass', value: '••••••••••••', protect: true },
    { key: 'Owner', value: 'Platform team', protect: false }
  ],
  e02: [
    { key: 'Token scope', value: 'contents:write, issues:write, actions:read', protect: false },
    { key: 'Expires', value: '2026-11-04', protect: false },
    { key: 'TOTP seed', value: '••••••••••••••••', protect: true }
  ]
};

export const ATTACHMENTS = {
  e01: [
    { name: 'root-recovery-codes.txt.gpg', size: '1.2 KB' },
    { name: 'account-diagram.png', size: '184 KB' }
  ],
  e02: [{ name: 'fine-grained-pat-scopes.json', size: '812 B' }]
};

// One revision stream, matching MaterialHistoryStore's append-only model. A
// restore is itself a revision, never a rewrite.
export const REVISIONS = [
  { id: 'r014', at: '2026-08-20T02:15:11Z', kind: 'edit', record: 'Discord — status bot', label: 'Password changed', detail: 'Password field replaced. Previous value retained in entry history.', author: 'you', sha: '9f2c1ab' },
  { id: 'r013', at: '2026-08-19T21:40:03Z', kind: 'edit', record: 'GitHub — Ding-Ding-Projects', label: 'URL and notes changed', detail: 'URL host moved to github.com; notes gained PAT scope list.', author: 'you', sha: '3d17b7e' },
  { id: 'r012', at: '2026-08-19T18:02:47Z', kind: 'settings', record: 'Settings › Appearance', label: 'Seed palette → Baseline purple', detail: 'Config key GUI_MaterialSeed: "keepass" → "purple".', author: 'you', sha: '5d42588' },
  { id: 'r011', at: '2026-08-18T11:30:22Z', kind: 'restore', record: 'Hang Seng — joint account', label: 'Restored revision r006', detail: 'Recorded as a new revision. r006 remains reachable; nothing was discarded.', author: 'you', sha: '332bf39' },
  { id: 'r010', at: '2026-08-17T07:55:10Z', kind: 'create', record: 'Postgres — status-hub', label: 'Entry created', detail: 'Created in group Development.', author: 'you', sha: 'edc9665' },
  { id: 'r009', at: '2026-08-14T09:12:58Z', kind: 'edit', record: 'AWS — production root', label: 'Health verdict → Breached', detail: 'Password appeared in an offline HIBP set. Entry flagged, not modified.', author: 'reports', sha: '593c3ac' },
  { id: 'r008', at: '2026-08-11T10:02:04Z', kind: 'edit', record: 'Cloudflare', label: 'Password rotated', detail: 'Generated: 24 chars, upper+lower+digits+symbols.', author: 'you', sha: 'f6f9d8c' },
  { id: 'r007', at: '2026-08-05T06:30:19Z', kind: 'create', record: 'Tidbyt device key', label: 'Entry created', detail: 'Created in group Development.', author: 'you', sha: '785e4f0' },
  { id: 'r006', at: '2026-05-18T08:30:41Z', kind: 'edit', record: 'Hang Seng — joint account', label: 'Password changed', detail: 'Value reused from CLP Power. Reports flagged both.', author: 'you', sha: '0dd3d70' },
  { id: 'r005', at: '2026-04-27T09:38:00Z', kind: 'edit', record: 'Taobao', label: 'Username changed', detail: 'dingding → dingding_hk.', author: 'you', sha: 'c766e2e' },
  { id: 'r004', at: '2026-03-03T16:44:12Z', kind: 'create', record: 'Mastodon — hachyderm', label: 'Entry created', detail: 'Created in group Social.', author: 'you', sha: '4387b8c' },
  { id: 'r003', at: '2026-02-09T13:20:55Z', kind: 'delete', record: 'Bitbucket (old)', label: 'Entry deleted', detail: 'Moved to Recycle Bin. Recoverable from this revision.', author: 'you', sha: '2373225' },
  { id: 'r002', at: '2026-01-19T12:00:03Z', kind: 'settings', record: 'Settings › Security', label: 'Clipboard timeout → 10 s', detail: 'Config key Security_ClearClipboardTimeout: 30 → 10.', author: 'you', sha: '8e1089f' },
  { id: 'r001', at: '2025-12-01T09:00:00Z', kind: 'create', record: 'Personal.kdbx', label: 'Database snapshotted', detail: 'History repository initialised beside the app data directory.', author: 'system', sha: 'f6c7031' }
];

export const REPORTS = [
  { id: 'breached', title: 'Breached passwords', yue: '洩漏咗嘅密碼', count: 2, severity: 'error', symbol: 'gpp_bad', blurb: 'Found in an offline breach corpus. Rotate these first.', rows: ['AWS — production root', 'Interactive Brokers'] },
  { id: 'weak', title: 'Weak passwords', yue: '弱密碼', count: 3, severity: 'amber', symbol: 'lock_open', blurb: 'Below the configured entropy floor of 60 bits.', rows: ['Outlook — Acme', 'HKTVmall', 'HKBN broadband'] },
  { id: 'reused', title: 'Reused passwords', yue: '翻用嘅密碼', count: 3, severity: 'amber', symbol: 'content_copy', blurb: 'One value shared by more than one entry.', rows: ['Hang Seng — joint account', 'CLP Power', 'Docker Hub'] },
  { id: 'expired', title: 'Expired entries', yue: '過咗期', count: 1, severity: 'amber', symbol: 'schedule', blurb: 'Past the expiry date recorded on the entry.', rows: ['Outlook — Acme'] },
  { id: 'health', title: 'Healthy', yue: '健康', count: 141, severity: 'green', symbol: 'verified_user', blurb: 'No finding against these entries.', rows: [] },
  { id: 'statistics', title: 'Database statistics', yue: '資料庫統計', count: 12, severity: 'neutral', symbol: 'query_stats', blurb: 'Size, entry counts, unique passwords, average length.', rows: [] }
];

export const CHANGELOG = [
  {
    version: '2.9.0', date: '2026-08-19', codename: 'Bamboo Shoot Har Gow · 筍尖蝦餃',
    changes: [
      { kind: 'added', text: 'Material Design 3 shell: navigation rail with ten destinations, top app bar, database tab strip.' },
      { kind: 'added', text: 'Appearance destination with live seed, density and per-element style editing.' },
      { kind: 'added', text: 'Regex builder reachable from every search bar, with Qt and JS dialect export.' },
      { kind: 'added', text: 'Local Git-backed history for entries, groups and settings, with restore-as-new-revision.' },
      { kind: 'fixed', text: 'History held a strong reference to the database, so locking never released the decrypted data.' },
      { kind: 'fixed', text: 'A restore could apply a revision the user never selected after a history truncation.' }
    ]
  },
  {
    version: '2.8.1', date: '2026-07-31', codename: 'Scallop Har Gow · 帶子蝦餃',
    changes: [
      { kind: 'fixed', text: 'MSI packaging failed when KPXC_FEATURE_DOCS was off: WiX declared shortcuts to files the flag had removed.' },
      { kind: 'fixed', text: 'Start Menu shortcuts for Getting Started and the User Guide were missing from every shipped installer.' },
      { kind: 'changed', text: 'CodeQL C++ analysis moved to Windows; the Linux runner could never build a Windows-only tree.' }
    ]
  },
  {
    version: '2.8.0', date: '2026-06-14', codename: 'Har Gow · 蝦餃',
    changes: [
      { kind: 'added', text: 'Notification centre: dismissed snackbars stay reviewable.' },
      { kind: 'added', text: 'Language mode (English, Cantonese, bilingual) with independent funny-level sliders.' },
      { kind: 'added', text: 'Changelog viewer covering every released version, with date filter and regex search.' },
      { kind: 'changed', text: 'Auto-Type, generator defaults and shortcuts moved off General and Security into their own pages.' }
    ]
  },
  {
    version: '2.7.9', date: '2026-04-02', codename: 'Siu Mai · 燒賣',
    changes: [
      { kind: 'added', text: 'External editor integration: detect, choose and open the current database folder.' },
      { kind: 'fixed', text: 'Reports walked the live entry tree from a worker thread while a nested event loop ran on the GUI thread.' },
      { kind: 'security', text: 'Clipboard clear timer now survives a window close.' }
    ]
  },
  {
    version: '2.7.8', date: '2026-01-28', codename: 'Cheung Fun · 腸粉',
    changes: [
      { kind: 'added', text: 'Dim sum surprise: a 1% chance at startup of a randomly chosen dish, off by default in quiet mode.' },
      { kind: 'changed', text: 'Density setting drives every list, tree and table: 40, 52 or 64 logical px rows.' }
    ]
  },
  {
    version: '2.7.7', date: '2025-11-11', codename: 'Lo Mai Gai · 糯米雞',
    changes: [{ kind: 'fixed', text: 'No user-visible changes were recorded for this release.' }]
  }
];

export const DIM_SUM = [
  { id: 'hk-dish-0002', en: 'Scallop har gow', yue: '帶子蝦餃', note: 'Shrimp dumpling with a scallop crown. Four pleats minimum or it is not trying.' },
  { id: 'hk-dish-0003', en: 'Bamboo shoot har gow', yue: '筍尖蝦餃', note: 'The bamboo shoot is the point. Skin should be translucent, not chewy.' },
  { id: 'hk-dish-0011', en: 'Rice noodle roll', yue: '腸粉', note: 'Sauce goes under, never over. This is not negotiable.' }
];

export const EXTERNAL_EDITORS = [
  { id: 'vscode', name: 'Visual Studio Code', path: '%LOCALAPPDATA%\\Programs\\Microsoft VS Code\\Code.exe', found: true },
  { id: 'nvim', name: 'Neovim', path: 'C:\\Program Files\\Neovim\\bin\\nvim-qt.exe', found: true },
  { id: 'notepadpp', name: 'Notepad++', path: 'C:\\Program Files\\Notepad++\\notepad++.exe', found: true },
  { id: 'sublime', name: 'Sublime Text', path: '—', found: false }
];
