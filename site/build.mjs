import {build} from 'esbuild';
import {mkdirSync,copyFileSync,readFileSync,writeFileSync,readdirSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('.',import.meta.url));
const repository=fileURLToPath(new URL('../',import.meta.url));
const changes=execFileSync('git',['status','--porcelain','--untracked-files=normal','--','site'],{cwd:fileURLToPath(new URL('../',import.meta.url)),encoding:'utf8'}).trim();
if(changes)throw Error('Commit website source before producing its versioned build.');
const commit=/^[a-f0-9]{40}$/i;
const sha256=/^[a-f0-9]{64}$/i;
const versionPattern=/^\d+\.\d+\.\d+$/;
const releaseRoot='https://github.com/Ding-Ding-Projects/keepassxc/releases/';
const documentationRoot='https://github.com/Ding-Ding-Projects/keepassxc/blob/main/';
const categories=new Set(['delivery','design','messaging','navigation','records','search']);
const statuses=new Set(['implemented','partial','missing','not-tracked']);
const readJson=(url)=>JSON.parse(readFileSync(url,'utf8'));
const exactKeys=(record,keys,label)=>{
  if(!record||typeof record!=='object'||Array.isArray(record))throw Error(`${label} must be an object.`);
  const actual=Object.keys(record).sort(),expected=[...keys].sort();
  if(actual.length!==expected.length||actual.some((key,index)=>key!==expected[index]))throw Error(`${label} has unsupported or missing fields.`);
};
const validUtc=(value)=>typeof value==='string'&&/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?Z$/.test(value)&&Number.isFinite(Date.parse(value))&&new Date(value).toISOString().slice(0,19)===value.slice(0,19);
function validateRelease(release){
  exactKeys(release,['schemaVersion','version','tag','sourceCommit','updatedAtUtc','updatedAtSource','notesUrl','installer','package','unsigned'],'Release metadata');
  if(release.schemaVersion!==1||!versionPattern.test(release.version)||release.tag!==`v${release.version}`||!commit.test(release.sourceCommit)||!validUtc(release.updatedAtUtc)||release.updatedAtSource!=='build-provenance.generatedAtUtc'||release.notesUrl!==`${releaseRoot}tag/${release.tag}`||release.unsigned!==true)throw Error('Release metadata has invalid release provenance.');
  const base=`${releaseRoot}download/${release.tag}/`,packageName=`KeePassXC.Material-${release.version}-full.nupkg`;
  for(const [label,asset,name] of [['installer',release.installer,'Setup.exe'],['package',release.package,packageName]]){
    exactKeys(asset,['url','bytes','sha256'],`Release ${label}`);
    if(asset.url!==base+name||!Number.isSafeInteger(asset.bytes)||asset.bytes<=0||!sha256.test(asset.sha256))throw Error(`Release ${label} has invalid unsigned download evidence.`);
  }
  const taggedCommit=execFileSync('git',['rev-parse',`${release.tag}^{commit}`],{cwd:repository,encoding:'utf8'}).trim();
  if(taggedCommit.toLowerCase()!==release.sourceCommit.toLowerCase())throw Error('Release tag does not resolve to the recorded source commit.');
  return release;
}
function documentationArticles(directory){
  const entries=[];
  for(const category of readdirSync(directory,{withFileTypes:true}).filter(entry=>entry.isDirectory()).map(entry=>entry.name).sort()){
    if(!categories.has(category))throw Error(`Unexpected documentation category: ${category}`);
    for(const article of readdirSync(new URL(`../docs/features/${category}/`,import.meta.url),{withFileTypes:true}).filter(entry=>entry.isFile()&&entry.name.endsWith('.md')&&entry.name!=='README.md').map(entry=>entry.name).sort())entries.push(`docs/features/${category}/${article}`);
  }
  return entries;
}
function validateContentManifest(manifest){
  exactKeys(manifest,['schemaVersion','articles'],'Content manifest');
  if(manifest.schemaVersion!==1||!Array.isArray(manifest.articles)||!manifest.articles.length)throw Error('Content manifest must contain documented articles.');
  const expected=documentationArticles(new URL('../docs/features/',import.meta.url));
  const actual=[];
  for(const entry of manifest.articles){
    exactKeys(entry,['article','category','title','implementationStatus','evidenceLink'],'Content manifest article');
    if(typeof entry.article!=='string'||typeof entry.category!=='string'||!categories.has(entry.category)||!entry.article.startsWith(`docs/features/${entry.category}/`)||!entry.article.endsWith('.md')||!statuses.has(entry.implementationStatus)||entry.evidenceLink!==documentationRoot+entry.article)throw Error('Content manifest article has invalid provenance.');
    exactKeys(entry.title,['en','zh-Hant'],'Content manifest localized title');
    if(!Object.values(entry.title).every(value=>typeof value==='string'&&value.trim().length>0))throw Error('Content manifest localized titles must be non-empty.');
    actual.push(entry.article);
  }
  const ordered=[...actual].sort();
  if(new Set(ordered).size!==ordered.length||ordered.length!==expected.length||ordered.some((article,index)=>article!==expected[index]))throw Error('Content manifest must list every documented feature article exactly once.');
  return manifest;
}
mkdirSync(new URL('dist/',import.meta.url),{recursive:true});
const bundled=await build({absWorkingDir:root,entryPoints:['app.js','search-worker.js'],outdir:'dist',bundle:true,format:'esm',target:'es2022',minify:true,metafile:true});
for(const file of ['index.html','styles.css','release.json','content-manifest.json'])copyFileSync(new URL(file,import.meta.url),new URL('dist/'+file,import.meta.url));
if(process.env.KPXC_REFRESH_RELEASE==='1')execFileSync(process.execPath,[fileURLToPath(new URL('../scripts/fetch-site-release-data.mjs',import.meta.url)),fileURLToPath(new URL('dist/release.json',import.meta.url))],{cwd:root,stdio:'inherit',timeout:100000});
const release=validateRelease(readJson(new URL('dist/release.json',import.meta.url)));
validateContentManifest(readJson(new URL('content-manifest.json',import.meta.url)));
copyFileSync(new URL('../social-preview.png',import.meta.url),new URL('dist/social-preview.png',import.meta.url));
const licenseDirectory=new URL('dist/licenses/',import.meta.url);
mkdirSync(licenseDirectory,{recursive:true});
const licenseIndex=[];
const runtimePackages=[...new Set(Object.keys(bundled.metafile.inputs).filter(path=>path.startsWith('node_modules/')).map(path=>{const parts=path.split('/');return parts.slice(1,parts[1].startsWith('@')?3:2).join('/');}))].sort();
for(const name of runtimePackages){
  const directory=new URL('node_modules/'+name+'/',import.meta.url);
  const files=readdirSync(directory).filter(file=>/^(?:licen[sc]e|notice)(?:[._-].*)?$/i.test(file));
  if(!files.length)throw Error('Missing runtime license: '+name);
  for(const file of files){const target=name.replace(/[@/]/g,'_')+'-'+file;copyFileSync(new URL(file,directory),new URL(target,licenseDirectory));licenseIndex.push(name+': '+target);}
}
writeFileSync(new URL('README.txt',licenseDirectory),licenseIndex.join('\n')+'\n');
writeFileSync(new URL('dist/build-provenance.json',import.meta.url),JSON.stringify({schemaVersion:1,version:release.version,sourceCommit:release.sourceCommit,updatedAtUtc:release.updatedAtUtc,updatedAtSource:release.updatedAtSource},null,2)+'\n');
console.log(`Built website ${release.version} from recorded release provenance ${release.sourceCommit}.`);
