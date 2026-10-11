import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {mkdtempSync,mkdirSync,writeFileSync,readFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {buildDocumentation} from '../site/build-documentation.mjs';
import {documentLanguages,documentationTitle,documentationHeadingIds,validateTranslationInventory,translationInventoryForDocuments} from '../site/documentation-languages.mjs';

const hash=text=>createHash('sha256').update(text).digest('hex');
const root=mkdtempSync(resolve(tmpdir(),'documentation-languages-'));
try{
    for(const directory of ['docs/features','docs/wiki','site/translations','output'])mkdirSync(resolve(root,directory),{recursive:true});
    const source='# Example\n\nComplete English paragraph with `literal` and [link](README.md).\n\n## Details\n\n```sh\necho example\n```\n';
    writeFileSync(resolve(root,'docs/features/README.md'),source);
    writeFileSync(resolve(root,'docs/wiki/README.md'),'# Wiki\n');
    writeFileSync(resolve(root,'docs/wiki/inventory.json'),JSON.stringify({schemaVersion:1,repository:'Ding-Ding-Projects/keepassxc.wiki',revision:'a'.repeat(40),pages:[]}));
    const translated='# 示例\n\n完整粵語段落，包括 `literal` 同[連結](README.md)。\n\n## 詳情\n\n```sh\necho example\n```\n';
    const inventory={schemaVersion:1,documents:[{path:'docs/features/README.md',sourceSha256:hash(source),status:'complete',title:'示例',markdown:translated},{path:'docs/wiki/README.md',sourceSha256:hash('# Wiki\n'),status:'missing',title:'',markdown:''}]};
    const save=()=>writeFileSync(resolve(root,'site/translations/documentation-yue.json'),JSON.stringify(inventory));
    const build=()=>{save();return buildDocumentation(root,resolve(root,'output'),'b'.repeat(40));};
    const result=build();assert.equal(result.schemaVersion,2);
    const ids=documentationHeadingIds(['Same','Same','Same-1','yue--Same','','']);assert.equal(new Set(ids).size,ids.length);
    assert.equal(new Set(ids.flatMap(id=>[`en--${id}`,`yue--${id}`])).size,ids.length*2);
    const article=result.documents[0];assert.deepEqual(documentLanguages(article,'both').map(record=>record.markdown),[source,translated]);
    assert.doesNotThrow(()=>validateTranslationInventory(translationInventoryForDocuments(result.documents),result.documents));
    for(const property of ['path','extra','__proto__']){
        const changed=structuredClone(result.documents);Object.defineProperty(changed[0].translation,property,{value:'docs/wiki/README.md',enumerable:true});
        assert.throws(()=>translationInventoryForDocuments(changed),/fields/);
    }
    const swapped=structuredClone(result.documents);[swapped[0].translation,swapped[1].translation]=[swapped[1].translation,swapped[0].translation];
    assert.throws(()=>validateTranslationInventory(translationInventoryForDocuments(swapped),swapped),/Stale/);
    assert.equal(documentLanguages(article,'yue')[0].language,'yue-Hant');
    assert.equal(documentLanguages(result.documents[1],'yue').length,0);
    assert.match(documentationTitle(result.documents[1],'both'),/unavailable/);
    const original=structuredClone(inventory);
    const restore=()=>{inventory.documents=structuredClone(original.documents);};
    inventory.documents.pop();assert.throws(build,/incomplete/);restore();
    inventory.documents[1]=structuredClone(inventory.documents[0]);assert.throws(build,/record/);restore();
    inventory.documents[0].sourceSha256='c'.repeat(64);assert.throws(build,/Stale/);restore();
    inventory.documents[0].markdown=source;assert.throws(build,/untranslated/);restore();
    inventory.documents[0].markdown=translated.replace('## 詳情','### 詳情');assert.throws(build,/heading structure/);restore();
    inventory.documents[0].markdown=translated.replace('echo example','echo changed');assert.throws(build,/code blocks/);restore();
    inventory.documents[0].markdown=translated.replace('`literal`','`changed`');assert.throws(build,/technical literals/);restore();
    inventory.documents[0].markdown=translated.replace('(README.md)','(outside.md)');assert.throws(build,/link destinations/);restore();
    inventory.documents[1].markdown='Partial content';assert.throws(build,/partial/);restore();
    for(const bad of [null,[],{},1]){inventory.documents[0].title=bad;assert.throws(build,/record/);restore();}
    inventory.documents[0].unexpected=true;assert.throws(build,/record/);restore();
    inventory.documents[0].markdown='x'.repeat(131073);assert.throws(build,/record/);restore();
    assert.equal(build().documents.length,2);
    const actual=JSON.parse(readFileSync(new URL('../site/translations/documentation-yue.json',import.meta.url),'utf8'));
    const productionRoot=fileURLToPath(new URL('..',import.meta.url));
    const production=actual.documents.map(entry=>({path:entry.path,markdown:readFileSync(resolve(productionRoot,entry.path),'utf8').replaceAll('\r\n','\n'),sourceSha256:entry.sourceSha256}));
    for(const document of production)assert.equal(hash(document.markdown),document.sourceSha256,document.path);
    validateTranslationInventory(actual,production);
    const delivered=buildDocumentation(productionRoot,resolve(root,'output'),'d'.repeat(40));
    assert.equal(delivered.documents.length,actual.documents.length);
    console.log(`PASS: language selection and explicit missing state; omission, duplicate, stale, untranslated, structure, code, partial, malformed and bounds negatives; ${actual.documents.length} source bindings.`);
}finally{rmSync(root,{recursive:true,force:true});}
