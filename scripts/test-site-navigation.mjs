import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const argument=process.argv.indexOf('--built');
const built=argument<0?resolve(root,'site/dist'):resolve(process.argv[argument+1]);
const html=readFileSync(resolve(built,'index.html'),'utf8');
const css=readFileSync(resolve(built,'styles.css'),'utf8');
let passed=0,failed=0;
function check(name,fn){try{fn();passed++;console.log(`PASS ${name}`);}catch(error){failed++;console.error(`FAIL ${name}: ${error.message}`);}}

// This checks the CSS cause found in the captured 320px layout. It is not a
// substitute for text-range geometry and keyboard checks in the rebuilt page.
check('tab minimum width retains the complete intrinsic label',()=>{
  const declarations=[...css.matchAll(/(?:^|})\s*md-primary-tab\s*\{([^}]*)\}/g)].flatMap(match=>match[1].split(';'));
  let minimum;
  for(const declaration of declarations){
    const [name,value]=declaration.split(':').map(part=>part.trim());
    // In the horizontal writing mode both properties address the same axis.
    if(name==='min-width'||name==='min-inline-size')minimum=value;
  }
  assert.equal(minimum,'max-content','The tab label must set its intrinsic minimum, not a fixed 48px floor.');
  assert.match(css,/md-tabs\s*\{[^}]*overflow-x\s*:\s*auto/,'Keep the scroll route for the full tab strip.');
});
check('navigation and accessible overflow controls remain available',()=>{
  const expected=['overview','downloads','docs','changelog','settings'];
  for(const id of expected)assert.match(html,new RegExp(`<md-primary-tab[^>]*id="tab-${id}"[^>]*aria-controls="${id}"`));
  for(const id of ['tab-overflow','tab-search-open','tab-reorder-open','tab-pin-open'])assert.ok(html.includes(`id="${id}"`));
  assert.doesNotMatch(css,/md-primary-tab\s*\{[^}]*(?:display\s*:\s*none|visibility\s*:\s*hidden|font-size\s*:\s*0(?:px)?\b)/);
});
check('document explicitly selects a local favicon',()=>{
  assert.match(html,/<link\s+rel="icon"\s+href="\.\/favicon\.ico"\s+type="image\/x-icon"\s*\/?\s*>/);
});
check('built favicon is the original project icon',()=>{
  const icon=resolve(built,'favicon.ico');
  assert.ok(existsSync(icon),'favicon.ico is missing from built output');
  const bytes=readFileSync(icon);
  assert.equal(bytes.readUInt32LE(0),65536,'ICO header must declare an icon');
  assert.ok(bytes.readUInt16LE(4)>0,'ICO must contain an image');
  assert.deepEqual(bytes,readFileSync(resolve(root,'share/windows/keepassxc.ico')));
});
console.log(`${passed} checks passed; ${failed} failed.`);
process.exitCode=failed?1:0;
