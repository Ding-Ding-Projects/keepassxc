import {marked} from 'marked';
import DOMPurify from 'dompurify';

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
        while(true){const {value,done}=await stream.read();if(done)break;length+=value.byteLength;if(length>524288){await stream.cancel();throw Error('Documentation exceeds its delivery bound.');}chunks.push(value);}
        const bytes=new Uint8Array(length);let offset=0;
        for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}
        bundle=JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes));
    }finally{clearTimeout(deadline);stream?.releaseLock();}
    const validPath=path=>typeof path==='string'&&/^docs\/(?:features\/[A-Za-z0-9/_-]+|wiki\/[-A-Za-z0-9_ ]+)\.md$/.test(path)&&!path.includes('..');
    if(bundle.schemaVersion!==1||!/^([a-f0-9]{40})$/.test(bundle.sourceCommit)||!/^([a-f0-9]{40})$/.test(bundle.wikiRevision)||!Array.isArray(bundle.documents)||bundle.documents.length>100||bundle.documents.some(document=>!validPath(document.path)||typeof document.title!=='string'||document.title.length>300||typeof document.markdown!=='string'||document.markdown.length>131072)||new Set(bundle.documents.map(document=>document.path)).size!==bundle.documents.length)throw Error('Invalid documentation inventory.');
    const documents=new Map(bundle.documents.map(document=>[document.path,document]));
    const sourceRoot='https://github.com/Ding-Ding-Projects/keepassxc/';
    let activePath='';
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
        const fragment=DOMPurify.sanitize(marked.parse(article.markdown),{RETURN_DOM_FRAGMENT:true,USE_PROFILES:{html:true},FORBID_TAGS:['script','style','iframe','form']});
        body.replaceChildren(fragment);
        for(const input of body.querySelectorAll('input'))input.disabled=true;
        const seen=new Map();
        for(const heading of body.querySelectorAll('h1,h2,h3,h4,h5,h6')){
            const base=heading.textContent.toLowerCase().replace(/[^\p{L}\p{N}_ -]/gu,'').trim().replace(/\s/g,'-');
            const count=seen.get(base)||0;seen.set(base,count+1);heading.id=count?`${base}-${count}`:base;
        }
        for(const anchor of body.querySelectorAll('a[href]')){
            const local=localTarget(anchor.getAttribute('href'),path);
            if(local){anchor.href=documentationHref(local.path,local.hash);anchor.dataset.documentPath=local.path;}
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
        refresh();
        if(hash){const target=[...body.querySelectorAll('[id]')].find(element=>element.id===decodeURIComponent(hash.slice(1)));target?.scrollIntoView({block:'start'});}
        else if(focus){const heading=body.querySelector('h1,h2')||body;heading.tabIndex=-1;heading.focus({preventScroll:true});heading.scrollIntoView({block:'start'});}
    }
    refresh=()=>{
        navigation.setAttribute('aria-label',copy('Documentation categories and wiki','文件分類同維基'));
        navigation.replaceChildren();
        for(const document of bundle.documents.filter(document=>document.path.endsWith('/README.md'))){
            const anchor=documentElement('a',document.title);anchor.href=documentationHref(document.path);anchor.dataset.documentPath=document.path;navigation.append(anchor);
        }
        outline.setAttribute('aria-label',copy('Article contents','文章目錄'));
        outline.replaceChildren();
        for(const heading of body.querySelectorAll('h2,h3')){const anchor=documentElement('a',heading.textContent);anchor.href=documentationHref(activePath,'#'+heading.id);anchor.dataset.documentPath=activePath;outline.append(anchor);}
        if(activePath){
            status.replaceChildren(document.createTextNode(copy('Documentation source: ','文件來源：')+bundle.sourceCommit+' · '));
            const source=documentElement('a',copy('View original source','查看原始文件'));source.href=sourceRoot+'blob/'+bundle.sourceCommit+'/'+activePath;source.rel='noopener noreferrer';status.append(source);
        }
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
