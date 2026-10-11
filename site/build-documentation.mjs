import {createHash} from 'node:crypto';
import {readFileSync,readdirSync,writeFileSync} from 'node:fs';
import {resolve,relative} from 'node:path';

export function buildDocumentation(repository,outputDirectory,sourceCommit){
    if(!/^[a-f0-9]{40}$/.test(sourceCommit))throw Error('Invalid documentation source commit.');
    const walk=directory=>readdirSync(directory,{withFileTypes:true}).flatMap(entry=>{
        if(entry.isSymbolicLink())throw Error('Documentation symbolic links are not supported.');
        return entry.isDirectory()?walk(resolve(directory,entry.name)):entry.name.endsWith('.md')?[resolve(directory,entry.name)]:[];
    });
    const wikiRoot=resolve(repository,'docs/wiki');
    const wiki=JSON.parse(readFileSync(resolve(wikiRoot,'inventory.json'),'utf8'));
    if(wiki.schemaVersion!==1||wiki.repository!=='Ding-Ding-Projects/keepassxc.wiki'||!/^([a-f0-9]{40})$/.test(wiki.revision)||!Array.isArray(wiki.pages))throw Error('Invalid wiki snapshot provenance.');
    const actual=walk(wikiRoot).map(file=>relative(wikiRoot,file).replaceAll('\\','/')).filter(file=>file!=='README.md').sort();
    const expected=wiki.pages.map(page=>page.file).sort();
    if(JSON.stringify(actual)!==JSON.stringify(expected)||new Set(expected).size!==expected.length)throw Error('Wiki snapshot inventory differs from delivered pages.');
    for(const page of wiki.pages){
        if(!/^[-A-Za-z0-9_ ]+\.md$/.test(page.file)||!/^[a-f0-9]{64}$/.test(page.sha256)||!/^[a-f0-9]{40}$/.test(page.sourceBlob))throw Error('Invalid wiki snapshot page provenance.');
        const content=readFileSync(resolve(wikiRoot,page.file),'utf8').replaceAll('\r\n','\n');
        if(createHash('sha256').update(content).digest('hex')!==page.sha256)throw Error(`Wiki snapshot content differs: ${page.file}`);
    }
    const paths=[...walk(resolve(repository,'docs/features')),...walk(wikiRoot)].sort();
    const documents=paths.map(file=>{
        const path=relative(repository,file).replaceAll('\\','/');
        const markdown=readFileSync(file,'utf8').replaceAll('\r\n','\n');
        if(/!\[[^\]]*\]\(|<img\b/i.test(markdown))throw Error(`Documentation images require a reviewed local asset mapping: ${path}`);
        const title=markdown.match(/^#\s+(.+)$/m)?.[1]||path;
        return {path,title,category:path.startsWith('docs/wiki/')?'wiki':path.split('/')[2]||'features',markdown};
    });
    const bundle={schemaVersion:1,sourceCommit,wikiRevision:wiki.revision,documents};
    writeFileSync(resolve(outputDirectory,'documentation.json'),JSON.stringify(bundle)+'\n');
    return bundle;
}
