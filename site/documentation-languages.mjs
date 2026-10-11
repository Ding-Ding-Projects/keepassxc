// Shared data validation and language selection, without DOM or filesystem access.
export const documentationLimits={bundleBytes:2097152,documentCharacters:131072,documents:100};
const hash=/^[a-f0-9]{64}$/;
const object=value=>value!==null&&typeof value==='object'&&!Array.isArray(value);
const keys=(value,expected)=>object(value)&&Object.keys(value).sort().join(',')===[...expected].sort().join(',');
export function documentationHashTarget(elements,canonicalIds,hash){
    let requested;try{requested=decodeURIComponent(hash.slice(1));}catch{return null;}
    const canonical=canonicalIds.includes(requested)?requested:requested.replace(/^(?:en|yue)--/,'');
    return elements.find(element=>element.id===requested)||elements.find(element=>canonicalIds[Number(element.dataset.headingIndex)]===canonical)||null;
}
export function focusDocumentationTarget(target,focus=true){
    if(!target)return;
    if(focus){if(!target.matches('a[href],button,input,select,textarea'))target.tabIndex=-1;target.focus({preventScroll:true});}
    target.scrollIntoView({block:'start'});
}
export function translationInventoryForDocuments(documents){
    return {schemaVersion:1,documents:documents.map(article=>{
        if(!keys(article.translation,['sourceSha256','status','title','markdown']))throw Error('Invalid delivered translation fields.');
        return {path:article.path,...article.translation};
    })};
}
export function documentationHeadingIds(titles){
    const used=new Set();
    return titles.map(title=>{
        const base=title.toLowerCase().replace(/[^\p{L}\p{N}_ -]/gu,'').trim().replace(/\s/g,'-')||'section';
        let id=base,index=0;while(used.has(id))id=`${base}-${++index}`;
        used.add(id);return id;
    });
}
export function validateTranslationInventory(inventory,documents){
    if(!keys(inventory,['schemaVersion','documents'])||inventory.schemaVersion!==1||!Array.isArray(inventory.documents)||inventory.documents.length!==documents.length)throw Error('Translation inventory is incomplete.');
    const sources=new Map(documents.map(document=>[document.path,document]));
    const seen=new Set();
    for(const entry of inventory.documents){
        if(!keys(entry,['path','sourceSha256','status','title','markdown'])||typeof entry.path!=='string'||seen.has(entry.path)||!sources.has(entry.path)||typeof entry.sourceSha256!=='string'||!hash.test(entry.sourceSha256)||!['complete','missing'].includes(entry.status)||typeof entry.title!=='string'||entry.title.length>300||typeof entry.markdown!=='string'||entry.markdown.length>documentationLimits.documentCharacters)throw Error('Invalid translation record.');
        seen.add(entry.path);
        const source=sources.get(entry.path);
        if(source.sourceSha256!==entry.sourceSha256)throw Error(`Stale translation source: ${entry.path}`);
        if(entry.status==='complete'&&(!entry.title.trim()||!entry.markdown.trim()||entry.markdown===source.markdown))throw Error(`Empty or untranslated document: ${entry.path}`);
        if(entry.status==='missing'&&(entry.title!==''||entry.markdown!==''))throw Error('Missing translations cannot contain partial content.');
    }
    return inventory;
}
export function documentLanguages(article,mode){
    const translated=article.translation?.status==='complete';
    if(mode==='en')return [{language:'en',title:article.title,markdown:article.markdown}];
    if(mode==='yue')return translated?[{language:'yue-Hant',title:article.translation.title,markdown:article.translation.markdown}]:[];
    return [{language:'en',title:article.title,markdown:article.markdown},...(translated?[{language:'yue-Hant',title:article.translation.title,markdown:article.translation.markdown}]:[])];
}
export function documentationTitle(article,mode){
    if(mode==='en')return article.title;
    if(article.translation?.status!=='complete')return mode==='yue'?`${article.title}（粵語譯文未完成）`:`${article.title} · Cantonese translation unavailable · 粵語譯文未完成`;
    return mode==='yue'?article.translation.title:`${article.title} · ${article.translation.title}`;
}
