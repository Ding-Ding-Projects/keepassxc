import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';
const app=readFileSync(new URL('./app.js',import.meta.url),'utf8');
const html=readFileSync(new URL('./index.html',import.meta.url),'utf8');
// Hand-maintained inventory of all existing restricted settings subtrees.
const required=['school-language-control','school-english-tone-control','school-cantonese-tone-control','school-emoji-control','school-narrator-controls','school-vocabulary-controls'];
function inventory(source){return [...source.matchAll(/<[^>]*\bid="([^"]+)"[^>]*\bdata-school-suppressed[^>]*>/g)].map(match=>match[1]).sort();}
const verifyInventory=source=>assert.deepEqual(inventory(source),[...required].sort());
verifyInventory(html);
assert.throws(()=>verifyInventory(html.replace('data-school-suppressed','data-omission-missing')),'Missing required marker must fail the inventory');
const production=app.match(/function applyRestrictedControls\(\)\{[\s\S]*?\n\}/)?.[0];assert.ok(production);
assert.match(app,/function renderLanguage\(\)\{\s*applyRestrictedControls\(\);/);
const state={school:false,language:'both',funny:{en:4,yue:2},vocabulary:{schemaVersion:1,entries:{neutral:'replacement'}}};
const original=JSON.stringify(state);let active=null,focusCount=0;
const wrappers=required.map((id,index)=>{const attrs=new Map(index===1?[['hidden','until-found'],['inert','prior'],['aria-hidden','false']]:index===2?[['aria-hidden','true']]:[]);return {id,attrs,contains:element=>element?.owner===id,getAttribute:name=>attrs.has(name)?attrs.get(name):null,setAttribute:(name,value)=>attrs.set(name,value),removeAttribute:name=>attrs.delete(name)};});
const initial=wrappers.map(wrapper=>[...wrapper.attrs]);
const mode={focus(){focusCount++;active={owner:'mode'};}};
const document={querySelectorAll:()=>wrappers,get activeElement(){return active;}};
const apply=runInNewContext(production+';applyRestrictedControls',{state,document,restrictedControlState:new WeakMap(),$:selector=>selector==='#school-mode'?mode:null});
apply();assert.deepEqual(wrappers.map(wrapper=>[...wrapper.attrs]),initial);assert.equal(focusCount,0);
active={owner:'school-vocabulary-controls'};state.school=true;apply();assert.equal(focusCount,1);
for(const wrapper of wrappers){assert.equal(wrapper.getAttribute('hidden'),'');assert.equal(wrapper.getAttribute('inert'),'');assert.equal(wrapper.getAttribute('aria-hidden'),'true');}
apply();assert.equal(focusCount,1,'Repeated render must not steal focus or overwrite the original snapshot');
state.school=false;apply();assert.deepEqual(wrappers.map(wrapper=>[...wrapper.attrs]),initial);
assert.equal(JSON.stringify(state),original,'Omission must preserve language, tone and replacement choices');
wrappers[0].setAttribute('aria-hidden','false');const changed=wrappers.map(wrapper=>[...wrapper.attrs]);state.school=true;apply();state.school=false;apply();assert.deepEqual(wrappers.map(wrapper=>[...wrapper.attrs]),changed,'A later cycle must capture the new original state');
console.log('PASS six-wrapper inventory, negative omission, focus return, hidden/inert/ARIA omission, exact restoration and preference preservation');
