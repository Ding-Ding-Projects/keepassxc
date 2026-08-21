// Language modes and the two funny-level dials.
//
// Copy contract, restated so it is checkable: the funny level changes VOICE, never
// FACTS. Every level of every message still names what happened, what is
// affected, and what the user can do. Which file, which account, which action
// is irreversible, what the error actually was. A level-5 warning is funnier;
// it is not vaguer. Where a message carries a number, a path or a key name, the
// same number, path and key name appear at all five levels.

export const LANGS = [
  { id: 'en', label: 'English', native: 'English' },
  { id: 'yue', label: 'Cantonese', native: '廣東話' },
  { id: 'both', label: 'Bilingual', native: 'English + 粵' }
];

// Fixed vocabulary. Not funny at any level — these are names of things.
export const NAV = {
  vault: ['Vault', '夾萬'],
  reports: ['Reports', '報告'],
  editor: ['Entry', '條目'],
  database: ['Database', '資料庫'],
  tools: ['Tools', '工具'],
  history: ['History', '歷史'],
  changelog: ['Changelog', '更新紀錄'],
  settings: ['Settings', '設定'],
  appearance: ['Appearance', '外觀'],
  help: ['Help', '幫手']
};

export const TERMS = {
  search: ['Search', '搵嘢'],
  regex: ['Regex', '正則'],
  plain: ['Plain text', '純文字'],
  builder: ['Builder', '砌規則'],
  copy: ['Copy', '複製'],
  username: ['Username', '用戶名'],
  password: ['Password', '密碼'],
  url: ['URL', '網址'],
  notes: ['Notes', '備註'],
  tags: ['Tags', '標籤'],
  modified: ['Modified', '改過'],
  created: ['Created', '整咗'],
  expires: ['Expires', '過期'],
  attachments: ['Attachments', '附件'],
  fields: ['Custom fields', '自訂欄位'],
  healthy: ['Healthy', '健康'],
  weak: ['Weak', '弱'],
  reused: ['Reused', '翻用'],
  breached: ['Breached', '洩漏咗'],
  restore: ['Restore', '還原'],
  diff: ['Diff', '比較'],
  revision: ['Revision', '版本'],
  dismiss: ['Dismiss', '收工'],
  undo: ['Undo', '返轉頭'],
  cancel: ['Cancel', '算數'],
  save: ['Save', '存'],
  matches: ['matches', '個中'],
  noMatch: ['No matches', '一個都冇中']
};

// Voice-variable messages. Index 0 is level 1 (fully serious), index 4 is
// level 5 (maximum playfulness). Same facts across the row.
const M = {
  clipboardCopied: {
    en: [
      'Password copied. The clipboard will be cleared in 10 seconds.',
      'Password copied — clipboard clears in 10 seconds.',
      'Copied. You have 10 seconds before the clipboard is wiped.',
      'Copied it. Ten seconds on the clock, then the clipboard gets wiped.',
      'Copied! Paste it in 10 seconds or it evaporates. Clipboard: cleared. Gone. Bye.'
    ],
    yue: [
      '密碼已複製。剪貼簿將於 10 秒後清除。',
      '密碼抄咗。10 秒後清剪貼簿。',
      '抄咗喇，10 秒之後清走個剪貼簿。',
      '抄咗！10 秒之後個剪貼簿就清走，快手貼。',
      '抄咗喇！10 秒之內貼咗佢，唔係個剪貼簿就同你講拜拜，清到一粒都冇。'
    ]
  },
  entrySaved: {
    en: [
      'Entry “{name}” saved to {db}.',
      'Saved “{name}” to {db}.',
      'Saved. “{name}” is in {db} now.',
      '“{name}” is saved into {db}. Nothing was lost.',
      'Done — “{name}” is tucked into {db}, safe and sound and slightly smug.'
    ],
    yue: [
      '條目「{name}」已儲存至 {db}。',
      '「{name}」存咗入 {db}。',
      '搞掂，「{name}」入咗 {db} 喇。',
      '「{name}」已經穩陣咁入咗 {db}，一個字都冇甩。',
      '搞掂晒！「{name}」已經舒舒服服咁瞓咗喺 {db} 度，安全過返屋企。'
    ]
  },
  deleteConfirm: {
    en: [
      'Delete “{name}” from {db}? This moves it to the Recycle Bin.',
      'Delete “{name}”? It goes to the Recycle Bin in {db}.',
      'Send “{name}” to the Recycle Bin in {db}?',
      'About to bin “{name}” from {db}. It lands in the Recycle Bin, not the void.',
      'Binning “{name}” from {db}. It goes to the Recycle Bin — recoverable — not to the great beyond.'
    ],
    yue: [
      '確定要從 {db} 刪除「{name}」？將移至回收筒。',
      '要刪除「{name}」？會移去 {db} 嘅回收筒。',
      '掉「{name}」入 {db} 嘅回收筒？',
      '就快掉「{name}」入 {db} 嘅回收筒喇。仲執得返，唔係真係無咗。',
      '準備掉「{name}」入 {db} 個回收筒。放心，係回收筒，唔係去咗第二個世界，執得返嘅。'
    ]
  },
  breachedWarn: {
    en: [
      '{n} passwords appear in a known breach corpus. Rotate them.',
      '{n} passwords were found in a breach list. Rotate them.',
      '{n} of your passwords are in a breach list. Time to rotate.',
      '{n} passwords turned up in a breach list. They need rotating, today.',
      '{n} of your passwords have been out clubbing in a breach list. Rotate them before they bring friends home.'
    ],
    yue: [
      '{n} 個密碼出現喺已知洩漏名單中，請更換。',
      '{n} 個密碼喺洩漏名單搵到，換咗佢。',
      '{n} 個密碼上咗洩漏名單，快啲換。',
      '{n} 個密碼喺洩漏名單度出現咗，今日就要換。',
      '{n} 個密碼喺洩漏名單度威威咁企咗喺度。快啲換，唔好等佢哋帶埋班朋友返嚟。'
    ]
  },
  lockWarn: {
    en: [
      'Database {db} locked after 5 minutes of inactivity.',
      '{db} locked — 5 minutes idle.',
      'Locked {db}. You were idle for 5 minutes.',
      'Locked {db} after 5 idle minutes. Nothing was lost.',
      'Locked {db} — you went quiet for 5 minutes, so it shut the door. Nothing lost, just re-enter the key.'
    ],
    yue: [
      '資料庫 {db} 因閒置 5 分鐘已鎖定。',
      '{db} 閒置 5 分鐘，鎖咗。',
      '{db} 鎖咗喇，你 5 分鐘冇郁過。',
      '閒置咗 5 分鐘，{db} 鎖返。一個字都冇甩。',
      '你靜咗 5 分鐘，{db} 就自己閂咗門。乜都冇甩，入返條匙咪得囉。'
    ]
  },
  saveError: {
    en: [
      'Could not write {db}: the file is locked by another process.',
      'Failed to write {db} — another process holds the file.',
      'Cannot save {db}. Another process has the file open.',
      'Could not save {db}: another process is holding the file open. Your changes are still in memory.',
      '{db} would not save — another process is sitting on the file like a cat on a keyboard. Your changes are still here, unharmed. Close the other program and try again.'
    ],
    yue: [
      '無法寫入 {db}：檔案被另一程序鎖定。',
      '寫唔到 {db}，畀第二個程式鎖住咗。',
      '{db} 存唔到，有第二個程式揸住個檔。',
      '{db} 存唔到：有另一個程式揸住個檔。你啲改動仲喺記憶體度，未甩。',
      '{db} 死都唔肯存 — 有另一個程式好似隻貓咁瞓咗喺個檔上面。放心，你啲改動仲喺度，關咗嗰個程式再試過。'
    ]
  },
  restored: {
    en: [
      'Restored revision {rev}. The restore was recorded as a new revision.',
      'Restored {rev}. Recorded as a new revision.',
      'Restored {rev} — logged as a new revision, so this is undoable.',
      'Restored {rev}. It went in as a new revision, so you can undo the undo.',
      'Restored {rev}. Written as a NEW revision, never a rewrite — so yes, you can undo your undo. History here is append-only and slightly stubborn about it.'
    ],
    yue: [
      '已還原版本 {rev}，並記錄為新版本。',
      '還原咗 {rev}，記錄成新版本。',
      '還原咗 {rev}，記低咗做新版本，仲可以返轉頭。',
      '還原咗 {rev}。佢入咗做新版本，所以你可以再返轉頭。',
      '還原咗 {rev}。呢個係寫成一個新版本，唔係改歷史 — 所以你想返轉頭再返轉頭都得。呢度啲歷史淨係識加，唔識刪，硬頸得嚟又幾可靠。'
    ]
  },
  regexInvalid: {
    en: [
      'Invalid pattern: {err}',
      'Pattern will not compile: {err}',
      'That pattern does not compile: {err}',
      'Pattern is broken: {err}. Nothing was searched.',
      'That pattern refuses to compile: {err}. Nothing was searched — fix it and it will run.'
    ],
    yue: [
      '無效模式：{err}',
      '呢個 pattern 編譯唔到：{err}',
      '個 pattern 有事：{err}',
      '個 pattern 爛咗：{err}。乜都冇搵過。',
      '個 pattern 死都唔肯編譯：{err}。所以乜都未搵過 — 執好佢就會行。'
    ]
  },
  emptyVault: {
    en: [
      'No entries in this group.',
      'This group has no entries.',
      'Nothing in this group yet.',
      'This group is empty. Add an entry with the button below.',
      'Completely empty. Not a single secret. Suspiciously tidy — add one with the button below.'
    ],
    yue: [
      '此群組沒有條目。',
      '呢個組冇條目。',
      '呢個組仲係空嘅。',
      '呢個組空嘅。撳下面粒掣加返個條目。',
      '一個都冇，乾淨到有啲可疑。撳下面粒掣加返個先啦。'
    ]
  },
  noResults: {
    en: [
      'No entries match “{q}”.',
      'Nothing matches “{q}”.',
      'No matches for “{q}”.',
      'Nothing matched “{q}”. Try a shorter query or turn regex off.',
      'Not one thing matched “{q}”. Try something shorter, or switch regex off and search like a normal person.'
    ],
    yue: [
      '沒有條目符合「{q}」。',
      '冇嘢符合「{q}」。',
      '「{q}」搵唔到嘢。',
      '「{q}」一個都冇中。試下打短啲，或者熄咗正則。',
      '「{q}」一個都冇中，零。試下打短啲，或者熄咗正則，當返個正常人咁搵。'
    ]
  },
  dimSum: {
    en: [
      'Today’s dim sum: {dish}.',
      'Dim sum of the day: {dish}.',
      'Here is a dim sum: {dish}.',
      'A dim sum has appeared: {dish}. Back to work in a moment.',
      'Surprise dim sum! {dish}. No, it does not do anything. Yes, it was worth shipping.'
    ],
    yue: [
      '今日點心：{dish}。',
      '今日推介：{dish}。',
      '嚟碟點心：{dish}。',
      '有碟點心走咗出嚟：{dish}。睇完繼續做嘢。',
      '突然有碟點心！{dish}。冇錯，佢乜都做唔到。冇錯，都係值得寫落去。'
    ]
  },
  externalEditor: {
    en: [
      'Opened {path} in {editor}.',
      'Opened {path} in {editor}.',
      '{editor} is opening {path}.',
      'Handed {path} to {editor}.',
      'Threw {path} at {editor} and it caught it. Nicely done, both of you.'
    ],
    yue: [
      '已用 {editor} 開啟 {path}。',
      '用 {editor} 開咗 {path}。',
      '{editor} 開緊 {path}。',
      '{path} 交咗畀 {editor} 開。',
      '{path} 掟咗畀 {editor}，佢接到。兩邊都叻仔。'
    ]
  }
};

export function msg(key, level, lang, vars) {
  const row = M[key];
  if (!row) return key;
  const i = Math.max(0, Math.min(4, (level | 0) - 1));
  const fill = s => s.replace(/\{(\w+)\}/g, (_, k) => (vars && vars[k] != null ? vars[k] : `{${k}}`));
  return { en: fill(row.en[i]), yue: fill(row.yue[i]) };
}

export function pick(pair, lang) {
  if (!pair) return '';
  const [en, yue] = Array.isArray(pair) ? pair : [pair.en, pair.yue];
  if (lang === 'en') return en;
  if (lang === 'yue') return yue;
  return en;
}

export function secondary(pair, lang) {
  if (lang !== 'both' || !pair) return '';
  const [, yue] = Array.isArray(pair) ? pair : [pair.en, pair.yue];
  return yue;
}

export const MESSAGE_KEYS = Object.keys(M);
