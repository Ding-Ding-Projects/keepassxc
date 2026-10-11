import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';
const source=readFileSync(new URL('./app.js',import.meta.url),'utf8');
const html=readFileSync(new URL('./index.html',import.meta.url),'utf8');
const production=source.match(/function openPersonalVocabularyPicker\(event\)\{[\s\S]*?\n\}/)?.[0];
assert.ok(production,'Production picker activation function must exist');
assert.match(source,/vocab\.addEventListener\('click',openPersonalVocabularyPicker\)/);
let opened=0;const picker={value:'previous',click(){opened++;}};const state={school:false};
const activate=runInNewContext(production+';openPersonalVocabularyPicker',{state,$:selector=>selector==='#personal-vocabulary-file'?picker:null});
assert.equal(opened,0,'Binding does not automatically open a picker');
assert.equal(activate({isTrusted:false}),false);assert.equal(opened,0);
assert.equal(activate({isTrusted:true,detail:1}),true);assert.equal(opened,1);assert.equal(picker.value,'');
assert.equal(activate({isTrusted:true,detail:0}),true);assert.equal(opened,2,'Trusted keyboard activation uses the same button click');
state.school=true;assert.equal(activate({isTrusted:true}),false);assert.equal(opened,2);
state.school=false;assert.equal(activate({isTrusted:true}),true);assert.equal(opened,3);
for(const [key,en,yue] of [['offlineDocumentationSource','Offline documentation source','離線文件來源'],['featureDocumentation','Feature documentation','功能文件'],['externalEditorDocumentation','External editor documentation','外部編輯器文件']]){
 assert.ok(html.includes('data-copy="'+key+'"'));
 assert.ok(source.includes(key+":['"+en+"','"+yue+"']"));
 assert.ok(source.includes(yue));
}
console.log('PASS production upload activation, trusted keyboard/pointer clicks, restriction and resource-copy ownership');
