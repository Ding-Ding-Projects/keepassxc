import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {documentationHref} from './documentation.js';
const root=new URL('../',import.meta.url);
const html=readFileSync(new URL('./index.html',import.meta.url),'utf8');
const bundle=JSON.parse(readFileSync(new URL('./dist/documentation.json',import.meta.url),'utf8'));
const routes=[['docs/features/README.md',1],['docs/features/navigation/external-editor.md',2],['docs/features/delivery/build-scripts.md',1]];
for(const [path,count] of routes){
 const source=readFileSync(new URL(path,root),'utf8').replaceAll('\r\n','\n');
 const document=bundle.documents.find(item=>item.path===path);
 assert.ok(document,'Target must be registered in the retained bundle: '+path);
 assert.equal(document.markdown,source,'Bundled article must match its actual source');
 assert.equal(document.sourceSha256,createHash('sha256').update(source).digest('hex'));
 const route=documentationHref(path);
 assert.equal(new URL(route,'https://example.invalid/').searchParams.get('article'),path);
 assert.equal(html.split('href="'+route+'"').length-1,count,'Each scoped action must use the actual internal route');
 assert.ok(!html.includes('href="https://github.com/Ding-Ding-Projects/keepassxc/blob/main/'+path+'"'));
}
assert.equal(html.split('href="https://github.com/Ding-Ding-Projects/keepassxc/tree/main/docs"').length-1,2,'Supplemental offline source links remain');
assert.equal((html.match(/Offline documentation source/g)||[]).length,2);
console.log('PASS four documentation actions, exact source/bundle hashes, registered routes and supplemental source links');
