import {build} from 'esbuild';
import {mkdirSync,copyFileSync,readFileSync,writeFileSync,readdirSync,rmSync,existsSync,mkdtempSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {resolve,relative,isAbsolute,sep} from 'node:path';
import {tmpdir} from 'node:os';

const root=fileURLToPath(new URL('.',import.meta.url));
const repository=fileURLToPath(new URL('../',import.meta.url));
const documentationDirectory=resolve(repository,'docs/features');
const outputDirectory=resolve(root,'dist');
const repositoryName='Ding-Ding-Projects/keepassxc';
const releaseRoot=`https://github.com/${repositoryName}/releases/`;
const documentationRoot=`https://github.com/${repositoryName}/blob/main/`;
const categories=new Set(['delivery','design','messaging','navigation','records','search']);
const statuses=new Set(['implemented','partial','missing','not-tracked']);
const commit=/^[a-f0-9]{40}$/i;
const sha256=/^[a-f0-9]{64}$/i;
const versionPattern=/^\d+\.\d+\.\d+$/;

function requireValue(condition,message){if(!condition)throw Error(message);}
function readJson(filename){return JSON.parse(readFileSync(filename,'utf8'));}
function exactKeys(record,keys,label){
  requireValue(record&&typeof record==='object'&&!Array.isArray(record),`${label} must be an object.`);
  const actual=Object.keys(record).sort(),expected=[...keys].sort();
  requireValue(actual.length===expected.length&&actual.every((key,index)=>key===expected[index]),`${label} has unsupported or missing fields.`);
}
function validUtc(value){return typeof value==='string'&&/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?Z$/.test(value)&&Number.isFinite(Date.parse(value))&&new Date(value).toISOString().slice(0,19)===value.slice(0,19);}
function ensureContained(rootDirectory,candidate,label){
  const relativePath=relative(rootDirectory,candidate);
  requireValue(relativePath!==''&&relativePath!=='..'&&!relativePath.startsWith(`..${sep}`)&&!isAbsolute(relativePath),`${label} escapes its allowed directory.`);
  return relativePath.split(sep).join('/');
}
function recreateOutputDirectory(directory){rmSync(directory,{recursive:true,force:true});mkdirSync(directory,{recursive:true});}
function assertAbsent(filename,label){requireValue(!existsSync(filename),`${label} survived output recreation.`);}

const changes=execFileSync('git',['status','--porcelain','--untracked-files=normal','--','site','docs/features','social-preview.png'],{cwd:repository,encoding:'utf8'}).trim();
if(changes)throw Error('Commit every website, documentation, and social-preview input before producing its versioned build.');

function validateRelease(release){
  exactKeys(release,['schemaVersion','version','tag','sourceCommit','updatedAtUtc','updatedAtSource','notesUrl','installer','package','unsigned'],'Release metadata');
  requireValue(release.schemaVersion===1&&versionPattern.test(release.version)&&release.tag===`v${release.version}`&&commit.test(release.sourceCommit)&&validUtc(release.updatedAtUtc)&&release.updatedAtSource==='build-provenance.generatedAtUtc'&&release.notesUrl===`${releaseRoot}tag/${release.tag}`&&release.unsigned===true,'Release metadata has invalid release provenance.');
  const base=`${releaseRoot}download/${release.tag}/`,packageName=`KeePassXC.Material-${release.version}-full.nupkg`;
  for(const [label,asset,name] of [['installer',release.installer,'Setup.exe'],['package',release.package,packageName]]){
    exactKeys(asset,['url','bytes','sha256'],`Release ${label}`);
    requireValue(asset.url===base+name&&Number.isSafeInteger(asset.bytes)&&asset.bytes>0&&sha256.test(asset.sha256),`Release ${label} has invalid unsigned download evidence.`);
  }
  const taggedCommit=execFileSync('git',['rev-parse',`${release.tag}^{commit}`],{cwd:repository,encoding:'utf8'}).trim();
  requireValue(taggedCommit.toLowerCase()===release.sourceCommit.toLowerCase(),'Release tag does not resolve to the recorded source commit.');
  return release;
}
function validatePublishedRelease(release){
  const published=JSON.parse(execFileSync('gh',['release','view',release.tag,'--repo',repositoryName,'--json','tagName,targetCommitish,isDraft,isPrerelease,assets'],{cwd:repository,encoding:'utf8',maxBuffer:1024*1024}));
  requireValue(published.tagName===release.tag&&published.targetCommitish?.toLowerCase()===release.sourceCommit.toLowerCase()&&published.isDraft===false&&published.isPrerelease===false&&Array.isArray(published.assets),'Published release identity is inconsistent with tracked metadata.');
  for(const asset of published.assets)requireValue(typeof asset.name==='string'&&asset.name.length>0&&Number.isSafeInteger(asset.size)&&asset.size>0,`Published release asset metadata is invalid: ${asset?.name??'(unnamed)'}.`);
  for(const [label,asset] of [['installer',release.installer],['package',release.package]]){
    const matches=published.assets.filter(candidate=>candidate.url===asset.url);
    requireValue(matches.length===1&&matches[0].size===asset.bytes&&matches[0].digest===`sha256:${asset.sha256.toLowerCase()}`,`Published ${label} asset differs from tracked metadata.`);
  }
}
function documentationArticles(directory=documentationDirectory){
  const entries=[];
  for(const category of readdirSync(directory,{withFileTypes:true}).filter(entry=>entry.isDirectory()).map(entry=>entry.name).sort()){
    requireValue(categories.has(category),`Unexpected documentation category: ${category}`);
    const categoryDirectory=resolve(directory,category);
    const visit=(currentDirectory)=>{
      for(const entry of readdirSync(currentDirectory,{withFileTypes:true})){
        const candidate=resolve(currentDirectory,entry.name);
        const categoryRelative=ensureContained(categoryDirectory,candidate,'Documentation article');
        if(entry.isDirectory()){visit(candidate);continue;}
        if(!entry.isFile()||!entry.name.endsWith('.md')||categoryRelative==='README.md')continue;
        entries.push(`docs/features/${category}/${categoryRelative}`);
      }
    };
    visit(categoryDirectory);
  }
  return entries.sort();
}
function validateContentManifest(manifest,directory=documentationDirectory){
  exactKeys(manifest,['schemaVersion','articles'],'Content manifest');
  requireValue(manifest.schemaVersion===1&&Array.isArray(manifest.articles)&&manifest.articles.length,'Content manifest must contain documented articles.');
  const expected=documentationArticles(directory),actual=[];
  for(const entry of manifest.articles){
    exactKeys(entry,['article','category','title','implementationStatus','evidenceLink'],'Content manifest article');
    requireValue(typeof entry.article==='string'&&entry.article===entry.article.replaceAll('\\','/')&&!entry.article.split('/').includes('..')&&typeof entry.category==='string'&&categories.has(entry.category)&&entry.article.startsWith(`docs/features/${entry.category}/`)&&entry.article.endsWith('.md')&&statuses.has(entry.implementationStatus)&&entry.evidenceLink===documentationRoot+entry.article,'Content manifest article has invalid provenance.');
    ensureContained(repository,resolve(repository,entry.article),'Content manifest article');
    exactKeys(entry.title,['en','zh-Hant'],'Content manifest localized title');
    requireValue(Object.values(entry.title).every(value=>typeof value==='string'&&value.trim().length>0),'Content manifest localized titles must be non-empty.');
    actual.push(entry.article);
  }
  const ordered=[...actual].sort();
  requireValue(new Set(ordered).size===ordered.length&&ordered.length===expected.length&&ordered.every((article,index)=>article===expected[index]),'Content manifest must list every documented feature article exactly once.');
}
function expectFailure(task,label){let failed=false;try{task();}catch{failed=true;}requireValue(failed,`${label} did not fail closed.`);}
function runNestedArticleProbe(){
  const fixture=mkdtempSync(resolve(tmpdir(),'keepassxc-site-docs-'));
  try{
    const category=resolve(fixture,'delivery'),nested=resolve(category,'nested');
    mkdirSync(nested,{recursive:true});
    writeFileSync(resolve(category,'README.md'),'# Delivery\n');
    writeFileSync(resolve(nested,'probe.md'),'# Probe\n');
    expectFailure(()=>validateContentManifest({schemaVersion:1,articles:[]},fixture),'Nested documentation omission');
    validateContentManifest({schemaVersion:1,articles:[{article:'docs/features/delivery/nested/probe.md',category:'delivery',title:{en:'Probe','zh-Hant':'測試'},implementationStatus:'not-tracked',evidenceLink:`${documentationRoot}docs/features/delivery/nested/probe.md`}]},fixture);
  }finally{rmSync(fixture,{recursive:true,force:true});}
}
function runStaleOutputProbe(){
  const fixture=mkdtempSync(resolve(tmpdir(),'keepassxc-site-output-'));
  try{
    const stale=resolve(fixture,'stale.txt');
    writeFileSync(stale,'obsolete');
    expectFailure(()=>assertAbsent(stale,'Stale output'),'Stale output');
    recreateOutputDirectory(fixture);
    assertAbsent(stale,'Stale output');
  }finally{rmSync(fixture,{recursive:true,force:true});}
}
const buildProbe=process.env.KPXC_BUILD_PROBE;
if(buildProbe==='nested-article')runNestedArticleProbe();
else if(buildProbe==='stale-output')runStaleOutputProbe();
else if(buildProbe&&buildProbe!=='published-release')throw Error(`Unknown build probe: ${buildProbe}`);

recreateOutputDirectory(outputDirectory);
const bundled=await build({absWorkingDir:root,entryPoints:['app.js','search-worker.js'],outdir:'dist',bundle:true,format:'esm',target:'es2022',minify:true,metafile:true});
for(const file of ['index.html','styles.css','release.json','content-manifest.json'])copyFileSync(new URL(file,import.meta.url),new URL(`dist/${file}`,import.meta.url));
if(process.env.KPXC_REFRESH_RELEASE==='1')execFileSync(process.execPath,[fileURLToPath(new URL('../scripts/fetch-site-release-data.mjs',import.meta.url)),fileURLToPath(new URL('dist/release.json',import.meta.url))],{cwd:root,stdio:'inherit',timeout:100000});
const release=validateRelease(readJson(resolve(outputDirectory,'release.json')));
if(buildProbe==='published-release'){
  const fabricated=structuredClone(release);
  fabricated.installer.sha256='0'.repeat(64);
  expectFailure(()=>validatePublishedRelease(fabricated),'Fabricated release metadata');
}
validatePublishedRelease(release);
validateContentManifest(readJson(resolve(root,'content-manifest.json')));
copyFileSync(new URL('../social-preview.png',import.meta.url),new URL('dist/social-preview.png',import.meta.url));
const licenseDirectory=new URL('dist/licenses/',import.meta.url);
mkdirSync(licenseDirectory,{recursive:true});
const licenseIndex=[];
const runtimePackages=[...new Set(Object.keys(bundled.metafile.inputs).filter(path=>path.startsWith('node_modules/')).map(path=>{const parts=path.split('/');return parts.slice(1,parts[1].startsWith('@')?3:2).join('/');}))].sort();
for(const name of runtimePackages){
  const directory=new URL(`node_modules/${name}/`,import.meta.url);
  const files=readdirSync(directory).filter(file=>/^(?:licen[sc]e|notice)(?:[._-].*)?$/i.test(file));
  if(!files.length)throw Error(`Missing runtime license: ${name}`);
  for(const file of files){const target=name.replace(/[@/]/g,'_')+'-'+file;copyFileSync(new URL(file,directory),new URL(target,licenseDirectory));licenseIndex.push(`${name}: ${target}`);}
}
writeFileSync(new URL('README.txt',licenseDirectory),licenseIndex.join('\n')+'\n');
writeFileSync(new URL('dist/build-provenance.json',import.meta.url),JSON.stringify({schemaVersion:1,version:release.version,sourceCommit:release.sourceCommit,updatedAtUtc:release.updatedAtUtc,updatedAtSource:release.updatedAtSource},null,2)+'\n');
console.log(`Built website ${release.version} from published release provenance ${release.sourceCommit}.`);
