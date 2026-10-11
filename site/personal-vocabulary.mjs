// Shared neutral contract. No built-in replacement data is provided.
export const vocabularyLimits=Object.freeze({bytes:256*1024,depth:8,keyCodePoints:160,valueCodePoints:1000});
const unsafe=new Set(['__proto__','constructor','prototype']);
const scalar=value=>{for(let i=0;i<value.length;i++){const n=value.charCodeAt(i);if(n>=0xd800&&n<=0xdbff){const next=value.charCodeAt(++i);if(!(next>=0xdc00&&next<=0xdfff))throw Error('Invalid Unicode');}else if(n>=0xdc00&&n<=0xdfff)throw Error('Invalid Unicode');}return value;};
export function parseVocabulary(input){
 let source;if(typeof input==='string'){scalar(input);if(new TextEncoder().encode(input).length>vocabularyLimits.bytes)throw Error('Size limit');source=input;}else{if(!(input instanceof Uint8Array)||input.byteLength>vocabularyLimits.bytes)throw Error('Size limit');source=new TextDecoder('utf-8',{fatal:true,ignoreBOM:true}).decode(input);scalar(source);}
 let at=0;const whitespace=()=>{while(/[ \t\r\n]/.test(source[at]||'!'))at++;};
 function string(){const start=at++;while(at<source.length){const c=source[at++];if(c==='"')return scalar(JSON.parse(source.slice(start,at)));if(c.charCodeAt(0)===92)at++;}throw Error('Unclosed string');}
 function value(depth){whitespace();const c=source[at];if(c==='"')return string();if(c==='{'||c==='['){if(depth>=vocabularyLimits.depth)throw Error('Depth limit');at++;const object=c==='{'?Object.create(null):[];const end=c==='{'?'}':']';const keys=new Set();whitespace();if(source[at]===end){at++;return object;}while(true){whitespace();if(c==='{'){if(source[at]!=='"')throw Error('Object key');const key=string();if(keys.has(key)||unsafe.has(key))throw Error('Unsafe or duplicate key');keys.add(key);whitespace();if(source[at++]!==':')throw Error('Colon');object[key]=value(depth+1);}else object.push(value(depth+1));whitespace();const next=source[at++];if(next===end)return object;if(next!==',')throw Error('Separator');}}const match=/^(?:true|false|null|-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)/.exec(source.slice(at));if(!match)throw Error('JSON value');at+=match[0].length;return JSON.parse(match[0]);}
 const root=value(0);whitespace();if(at!==source.length||!root||Array.isArray(root)||typeof root!=='object'||Object.keys(root).length!==2||root.schemaVersion!==1||!Object.hasOwn(root,'entries')||!root.entries||Array.isArray(root.entries)||typeof root.entries!=='object')throw Error('Schema');
 for(const [key,replacement] of Object.entries(root.entries)){if(!key||[...key].length>vocabularyLimits.keyCodePoints||typeof replacement!=='string'||[...replacement].length>vocabularyLimits.valueCodePoints)throw Error('Entry bounds');}
 return {schemaVersion:1,entries:root.entries};
}
export function applyVocabulary(text,document){let result=text;for(const key of Object.keys(document.entries).sort((a,b)=>b.length-a.length||a.localeCompare(b))){const escaped=key.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');result=result.replace(new RegExp('(?<![\\p{L}\\p{N}_])'+escaped+'(?![\\p{L}\\p{N}_])','gu'),()=>document.entries[key]);}return result;}

// Storage operations finish before changing the active document.
export function createVocabularyStore(storage,key){
 let active={schemaVersion:1,entries:{}};
 return {
  get active(){return active;},
  load(){active={schemaVersion:1,entries:{}};const cached=storage.getItem(key);if(cached!==null)active=parseVocabulary(cached);return active;},
  replace(bytes){const next=parseVocabulary(bytes);storage.setItem(key,JSON.stringify(next));active=next;return active;},
  clear(){storage.removeItem(key);active={schemaVersion:1,entries:{}};return active;}
 };
}
