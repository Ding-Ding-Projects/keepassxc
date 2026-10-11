import {marked} from 'marked';
import DOMPurify from 'dompurify';
import {documentationLimits,documentLanguages,documentationTitle,documentationHeadingIds,validateTranslationInventory} from './documentation-languages.mjs';

export const documentationHref=(path,hash='')=>`?article=${encodeURIComponent(path)}${hash}`;
let refresh=()=>{};
export function refreshDocumentationCopy(){refresh();}

export async function initializeDocumentation({language,showDocs}){
    const panel=document.querySelector('#docs');
    const navigation=document.createElement('nav');
    navigation.className='documentation-index';
    const reader=document.createElement('article');
    reader.className='documentation-reader';
    reader.hidden=true;
    const status=document.createElement('p');
    status.setAttribute('role','status');
    const outline=document.createElement('nav');
    outline.className='documentation-outline';
    const body=document.createElement('div');
    body.className='documentation-body';
    reader.append(status,outline,body);
    document.querySelector('#doc-list').after(navigation,reader);
    const copy=(en,yue)=>language()==='yue'?yue:language()==='both'?`${en} · ${yue}`:en;
    const controller=new AbortController();
    let stream,bundle;
    const deadline=setTimeout(()=>{controller.abort();if(stream)void stream.cancel().catch(()=>{});},15000);
    try{
        const response=await fetch('./documentation.json',{cache:'no-cache',signal:controller.signal});
        if(!response.ok)throw Error('Documentation is unavailable.');
        stream=response.body.getReader();
        const chunks=[];let length=0;
        while(true){const {value,done}=await stream.read();if(done)break;length+=value.byteLength;if(length>documentationLimits.bundleBytes){await stream.cancel();throw Error('Documentation exceeds its delivery bound.');}chunks.push(value);}
        const bytes=new Uint8Array(length);let offset=0;
        for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}
        bundle=JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes));
    }finally{clearTimeout(deadline);stream?.releaseLock();}
    const validPath=path=>typeof path==='string'&&/^docs\/(?:features\/[A-Za-z0-9/_-]+|wiki\/[-A-Za-z0-9_ ]+)\.md$/.test(path)&&!path.includes('..');
    if(bundle.schemaVersion!==2||typeof bundle.sourceCommit!=='string'||!/^([a-f0-9]{40})$/.test(bundle.sourceCommit)||typeof bundle.wikiRevision!=='string'||!/^([a-f0-9]{40})$/.test(bundle.wikiRevision)||!Array.isArray(bundle.documents)||bundle.documents.length>documentationLimits.documents||bundle.documents.some(document=>!validPath(document.path)||typeof document.title!=='string'||document.title.length>300||typeof document.markdown!=='string'||document.markdown.length>documentationLimits.documentCharacters||typeof document.sourceSha256!=='string'||!/^[a-f0-9]{64}$/.test(document.sourceSha256)||!document.translation)||new Set(bundle.documents.map(document=>document.path)).size!==bundle.documents.length)throw Error('Invalid documentation inventory.');
    validateTranslationInventory({schemaVersion:1,documents:bundle.documents.map(article=>({path:article.path,...article.translation}))},bundle.documents);
    for(const article of bundle.documents){
        const digest=await crypto.subtle.digest('SHA-256',new TextEncoder().encode(article.markdown));
        if([...new Uint8Array(digest)].map(byte=>byte.toString(16).padStart(2,'0')).join('')!==article.sourceSha256)throw Error('Documentation source digest differs.');
    }
    const documents=new Map(bundle.documents.map(document=>[document.path,document]));
    const sourceRoot='https://github.com/Ding-Ding-Projects/keepassxc/';
    let activePath='';
    let renderedLanguage='';
    function localTarget(raw,path){
        let url;
        try{url=new URL(raw,`https://documentation.invalid/${path}`);decodeURIComponent(url.pathname);decodeURIComponent(url.hash);}catch{return null;}
        let target;
        if(url.origin==='https://documentation.invalid')target=decodeURIComponent(url.pathname.slice(1));
        else if(url.origin==='https://github.com'&&url.pathname.startsWith('/Ding-Ding-Projects/keepassxc/blob/'))target=url.pathname.replace(/^\/Ding-Ding-Projects\/keepassxc\/blob\/[^/]+\//,'');
        else if(url.origin==='https://github.com'&&url.pathname.startsWith('/Ding-Ding-Projects/keepassxc/wiki/'))target='docs/wiki/'+decodeURIComponent(url.pathname.split('/').pop())+'.md';
        target=target?.replace(/\/+$/,'');
        if(target&&!target.endsWith('.md')&&documents.has(target+'/README.md'))target+='/README.md';
        if(target?.startsWith('docs/wiki/')&&!target.endsWith('.md')&&documents.has(target+'.md'))target+='.md';
        return target&&documents.has(target)?{path:target,hash:url.hash}:null;
    }
    function render(path,hash='',focus=true){
        const article=documents.get(path);
        if(!article){status.textContent=copy('This documentation article is unavailable.','呢篇文件未能取得。');reader.hidden=false;body.replaceChildren();outline.replaceChildren();return;}
        activePath=path;
        reader.hidden=false;
        const sanitize=markdown=>DOMPurify.sanitize(marked.parse(markdown),{RETURN_DOM_FRAGMENT:true,USE_PROFILES:{html:true},FORBID_TAGS:['script','style','iframe','form'],FORBID_ATTR:['id','name']});
        const original=sanitize(article.markdown);
        const canonical=[...original.querySelectorAll('h1,h2,h3,h4,h5,h6')];
        const ids=documentationHeadingIds(canonical.map(heading=>heading.textContent));
        renderedLanguage=language();
        const rendered=document.createDocumentFragment();
        for(const variant of documentLanguages(article,renderedLanguage)){
            const section=document.createElement('section');section.lang=variant.language;
            section.dataset.documentLanguage=variant.language;
            section.setAttribute('aria-label',variant.language==='en'?'English':'粵語');
            section.append(variant.language==='en'?original:sanitize(variant.markdown));
            const headings=[...section.querySelectorAll('h1,h2,h3,h4,h5,h6')];
            if(headings.length!==ids.length||headings.some((heading,index)=>heading.tagName!==canonical[index].tagName))throw Error('Translation heading structure differs.');
            headings.forEach((heading,index)=>{heading.id=renderedLanguage==='both'?`${variant.language==='en'?'en':'yue'}--${ids[index]}`:ids[index];heading.dataset.headingIndex=String(index);});
            rendered.append(section);
        }
        if(!rendered.childNodes.length){
            const unavailable=documentElement('h1','粵語譯文未完成');unavailable.lang='yue-Hant';
            rendered.append(unavailable,documentElement('p','可以切換至英文模式閱讀完整原文。'));
        }
        body.replaceChildren(rendered);
        for(const input of body.querySelectorAll('input'))input.disabled=true;
        for(const anchor of body.querySelectorAll('a[href]')){
            const local=localTarget(anchor.getAttribute('href'),path);
            if(local){
                let targetHash=local.hash;
                if(renderedLanguage==='both'&&local.path===path&&targetHash){
                    let decoded;try{decoded=decodeURIComponent(targetHash.slice(1));}catch{decoded='';}
                    if(ids.includes(decoded))targetHash='#'+(anchor.closest('section').lang==='en'?'en':'yue')+'--'+decoded;
                }
                anchor.href=documentationHref(local.path,targetHash);anchor.dataset.documentPath=local.path;
            }
            else{
                const raw=anchor.getAttribute('href');
                if(raw&&!/^[a-z][a-z0-9+.-]*:/i.test(raw)){
                    const source=new URL(raw,sourceRoot+'blob/'+bundle.sourceCommit+'/'+path);
                    anchor.href=source.href;
                }
                anchor.rel='noopener noreferrer';
            }
        }
        for(const image of body.querySelectorAll('img')){
            const description=document.createElement('span');description.textContent=image.alt;
            image.replaceWith(description);
        }
        updateCopy();
        if(hash){
            let requested;try{requested=decodeURIComponent(hash.slice(1));}catch{return;}
            const target=[...body.querySelectorAll('[id]')].find(element=>element.id===requested)||[...body.querySelectorAll('[data-heading-index]')].find(element=>ids[Number(element.dataset.headingIndex)]===requested.replace(/^(?:en|yue)--/,''));
            target?.scrollIntoView({block:'start'});
        }
        else if(focus){const heading=body.querySelector('h1,h2')||body;heading.tabIndex=-1;heading.focus({preventScroll:true});heading.scrollIntoView({block:'start'});}
    }
    function updateCopy(){
        navigation.setAttribute('aria-label',copy('Documentation categories and wiki','文件分類同維基'));
        navigation.replaceChildren();
        for(const document of bundle.documents.filter(document=>document.path.endsWith('/README.md'))){
            const anchor=documentElement('a',documentationTitle(document,language()));anchor.href=documentationHref(document.path);anchor.dataset.documentPath=document.path;navigation.append(anchor);
        }
        outline.setAttribute('aria-label',copy('Article contents','文章目錄'));
        outline.replaceChildren();
        const sections=[...body.querySelectorAll(':scope > section')];
        for(const heading of sections[0]?.querySelectorAll('h2,h3')||[]){
            const counterpart=sections[1]?.querySelector(`[data-heading-index="${heading.dataset.headingIndex}"]`);
            const anchor=documentElement('a',counterpart?`${heading.textContent} · ${counterpart.textContent}`:heading.textContent);anchor.href=documentationHref(activePath,'#'+heading.id);anchor.dataset.documentPath=activePath;outline.append(anchor);
        }
        if(activePath){
            status.replaceChildren(document.createTextNode(copy('Documentation source: ','文件來源：')+bundle.sourceCommit+' · '));
            const source=documentElement('a',copy('View original source','查看原始文件'));source.href=sourceRoot+'blob/'+bundle.sourceCommit+'/'+activePath;source.rel='noopener noreferrer';status.append(source);
            if(language()!=='en'&&documents.get(activePath)?.translation.status!=='complete')status.append(document.createTextNode(copy(' · Cantonese translation unavailable. English content is available in English mode.',' · 粵語譯文未完成。可以切換至英文模式閱讀完整原文。')));
        }
    }
    refresh=()=>{
        if(activePath&&renderedLanguage!==language()){
            const focused=body.contains(document.activeElement)?document.activeElement:null;
            const headingIndex=focused?.dataset.headingIndex;
            const focusLinkIndex=focused?.matches('a')?[...focused.closest('section').querySelectorAll('a')].indexOf(focused):-1;
            const focusLanguage=focused?.closest('section')?.dataset.documentLanguage;
            const visible=[...body.querySelectorAll('h1,h2,h3,h4,h5,h6')].find(heading=>heading.getBoundingClientRect().bottom>=0);
            const scrollIndex=visible?.dataset.headingIndex;
            render(activePath,'',false);
            const section=body.querySelector(`section[data-document-language="${focusLanguage}"]`)||body.querySelector('section');
            const focusTarget=focusLinkIndex>=0?section?.querySelectorAll('a')[focusLinkIndex]:body.querySelector(`[data-heading-index="${headingIndex}"]`);
            if(focused&&focusTarget){focusTarget.tabIndex=-1;focusTarget.focus({preventScroll:true});}
            const target=body.querySelector(`[data-heading-index="${headingIndex??scrollIndex}"]`);
            target?.scrollIntoView({block:'start'});
        }else updateCopy();
    };
    panel.addEventListener('click',event=>{
        const anchor=event.target.closest('a[data-document-path]');
        if(!anchor||event.defaultPrevented||event.button!==0||event.ctrlKey||event.metaKey||event.shiftKey||event.altKey)return;
        event.preventDefault();const url=new URL(anchor.href);history.pushState(null,'',url);showDocs();render(anchor.dataset.documentPath,url.hash);
    });
    const route=()=>{const url=new URL(location.href),path=url.searchParams.get('article');if(path){showDocs();render(path,url.hash);}else{reader.hidden=true;activePath='';}};
    window.addEventListener('popstate',route);
    refresh();route();
}

function documentElement(tag,text){const element=document.createElement(tag);element.textContent=text;return element;}
