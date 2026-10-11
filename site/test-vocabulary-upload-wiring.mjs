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
const renderProduction=source.match(/function renderVocabularyControls\(\)\{[\s\S]*?\n\}/)?.[0];
assert.ok(renderProduction,'Production vocabulary rendering function must exist');
const controls=new Map(['personal-vocabulary-upload','personal-vocabulary-reset'].map(id=>[id,{tagName:'MD-OUTLINED-BUTTON',children:[],replaceChildren(...children){this.children=children;}}]));
const renderState={school:false,language:'both'};
const context={state:renderState,vocabularyStatus:'empty',$:selector=>controls.get(selector.slice(1)),document:{createElement:tag=>({tagName:tag.toUpperCase(),className:'',textContent:''})}};
const render=runInNewContext(renderProduction+';renderVocabularyControls',context);
const verifyWrapping=()=>{for(const control of controls.values()){assert.equal(control.children.length,1,'Repeated rendering must retain exactly one wrapping label');assert.equal(control.children[0].tagName,'SPAN');assert.equal(control.children[0].className,'button-copy');}};
const missingWrapping=runInNewContext(renderProduction.replace("label.className='button-copy'","label.className='missing-wrap'")+';renderVocabularyControls',context);
missingWrapping();assert.throws(verifyWrapping,'Losing the production wrapping class must fail the fixture');
for(const status of ['empty','loaded','invalid','loaded','empty']){
 context.vocabularyStatus=status;render();
 verifyWrapping();
 const upload=status==='loaded'?'Replace vocabulary JSON file · 替換個人用詞 JSON 檔案':'Choose vocabulary JSON file · 揀選個人用詞 JSON 檔案';
 for(const [id,expected] of [['personal-vocabulary-upload',upload],['personal-vocabulary-reset','Clear personal vocabulary · 清除個人用詞']]){
  const control=controls.get(id);
  assert.equal(control.children[0].textContent,expected);assert.equal(control.ariaLabel,expected,'Accessible name retains the complete action');
 }
}
console.log('PASS production upload activation, trusted keyboard/pointer clicks, restriction, resource-copy ownership and wrapping action labels across empty/loaded/invalid states');
