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
const statuses=new Set(['implemented','partial','missing','not-tracked','mixed']);
const manifestInventoryPath='docs/features/inventory.json';
const commit=/^[a-f0-9]{40}$/i;
const sha256=/^[a-f0-9]{64}$/i;
const versionPattern=/^\d+\.\d+\.\d+$/;

function requireValue(condition,message){if(!condition)throw Error(message);}
function readJson(filename){return JSON.parse(readFileSync(filename,'utf8'));}
function readGitJson(revision,filename){return JSON.parse(execFileSync('git',['show',`${revision}:${filename}`],{cwd:repository,encoding:'utf8',maxBuffer:1024*1024}));}
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
const buildSourceCommit=execFileSync('git',['rev-parse','HEAD'],{cwd:repository,encoding:'utf8'}).trim();

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
function blobAt(revision,filename){return execFileSync('git',['rev-parse',`${revision}:${filename}`],{cwd:repository,encoding:'utf8'}).trim();}
function isAncestor(ancestor,descendant){try{execFileSync('git',['merge-base','--is-ancestor',ancestor,descendant],{cwd:repository,stdio:'ignore'});return true;}catch{return false;}}
function assertEvidenceAncestry(evidenceCommit,sourceCommit,ancestry=isAncestor){requireValue(ancestry(evidenceCommit,sourceCommit),'Evidence commit is not an ancestor of the build source commit.');}
function assertArticleBlobIdentity(evidenceCommit,evidencePath,sourceCommit,currentPath,resolveBlob=blobAt){requireValue(resolveBlob(evidenceCommit,evidencePath)===resolveBlob(sourceCommit,currentPath),'Current article bytes differ from immutable evidence.');}
function validateContentManifest(manifest,directory=documentationDirectory,inventoryOverride){
  exactKeys(manifest,['schemaVersion','evidenceCommit','articles'],'Content manifest');
  requireValue(manifest.schemaVersion===2&&commit.test(manifest.evidenceCommit)&&Array.isArray(manifest.articles)&&manifest.articles.length,'Content manifest must contain a valid evidence commit and documented articles.');
  execFileSync('git',['cat-file','-e',`${manifest.evidenceCommit}^{commit}`],{cwd:repository,stdio:'ignore'});
  assertEvidenceAncestry(manifest.evidenceCommit,buildSourceCommit);
  const inventory=inventoryOverride??readGitJson(manifest.evidenceCommit,manifestInventoryPath);
  requireValue(Array.isArray(inventory.rows),'Feature inventory must contain rows.');
  const expected=documentationArticles(directory),actual=[];
  const ids=[];
  const claimedFeatureIds=new Set();
  for(const entry of manifest.articles){
    exactKeys(entry,['id','article','category','title','implementationStatus','statusProvenance','evidence'],'Content manifest article');
    requireValue(typeof entry.id==='string'&&/^[a-z0-9]+(?:-[a-z0-9]+)*$/.test(entry.id)&&typeof entry.article==='string'&&entry.article===entry.article.replaceAll('\\','/')&&!entry.article.split('/').includes('..')&&typeof entry.category==='string'&&categories.has(entry.category)&&entry.article.startsWith(`docs/features/${entry.category}/`)&&entry.article.endsWith('.md')&&statuses.has(entry.implementationStatus),'Content manifest article has invalid identity or status.');
    ensureContained(repository,resolve(repository,entry.article),'Content manifest article');
    exactKeys(entry.title,['en','zh-Hant'],'Content manifest localized title');
    requireValue(Object.values(entry.title).every(value=>typeof value==='string'&&value.trim().length>0),'Content manifest localized titles must be non-empty.');
    exactKeys(entry.statusProvenance,['sourcePath','mapping','featureIds'],'Content manifest status provenance');
    requireValue(entry.statusProvenance.sourcePath===manifestInventoryPath&&entry.statusProvenance.mapping==='article'&&Array.isArray(entry.statusProvenance.featureIds)&&entry.statusProvenance.featureIds.every(id=>typeof id==='string'&&/^[a-z0-9]+(?:-[a-z0-9]+)*$/.test(id))&&new Set(entry.statusProvenance.featureIds).size===entry.statusProvenance.featureIds.length,'Content manifest status provenance is invalid.');
    for(const featureId of entry.statusProvenance.featureIds){requireValue(!claimedFeatureIds.has(featureId),'Content manifest maps one status provenance feature ID to multiple article records.');claimedFeatureIds.add(featureId);}
    const mappedRows=inventory.rows.filter(row=>row.article?.file===entry.article);
    const mappedRowKeys=mappedRows.map(row=>`${row.id}\u0000${row.surface??''}`);
    requireValue(new Set(mappedRowKeys).size===mappedRowKeys.length,'Feature inventory contains duplicate mapped IDs for one surface.');
    const mappedIds=[...new Set(mappedRows.map(row=>row.id))].sort();
    const listedIds=[...new Set(entry.statusProvenance.featureIds)].sort();
    requireValue(mappedIds.length===listedIds.length&&mappedIds.every((id,index)=>id===listedIds[index]),'Content manifest feature inventory mapping differs from the article record.');
    const mappedStatuses=[...new Set(mappedRows.map(row=>row.status))];
    const expectedStatus=mappedStatuses.length===0?'not-tracked':mappedStatuses.length===1?mappedStatuses[0]:'mixed';
    requireValue(entry.implementationStatus===expectedStatus,'Content manifest status differs from the mapped feature inventory.');
    exactKeys(entry.evidence,['repository','ref','path','url'],'Content manifest evidence provenance');
    requireValue(entry.evidence.repository===repositoryName&&entry.evidence.ref===manifest.evidenceCommit&&entry.evidence.path===entry.article&&entry.evidence.url===`https://github.com/${repositoryName}/blob/${manifest.evidenceCommit}/${entry.article}`,'Content manifest evidence provenance is invalid.');
    execFileSync('git',['cat-file','-e',`${manifest.evidenceCommit}:${entry.evidence.path}`],{cwd:repository,stdio:'ignore'});
    assertArticleBlobIdentity(manifest.evidenceCommit,entry.evidence.path,buildSourceCommit,entry.article);
    actual.push(entry.article);
    ids.push(entry.id);
  }
  const ordered=[...actual].sort();
  requireValue(new Set(ids).size===ids.length&&new Set(ordered).size===ordered.length&&ordered.length===expected.length&&ordered.every((article,index)=>article===expected[index]),'Content manifest must list every documented feature article exactly once.');
}
function expectFailure(task,label,expectedMessage){
  try{task();}catch(error){
    requireValue(error instanceof Error&&error.message===expectedMessage,`${label} failed for an unexpected reason.`);
    return;
  }
  throw Error(`${label} did not fail closed.`);
}
function runNestedArticleProbe(){
  const fixture=mkdtempSync(resolve(tmpdir(),'keepassxc-site-docs-'));
  try{
    const category=resolve(fixture,'delivery'),nested=resolve(category,'nested');
    mkdirSync(nested,{recursive:true});
    writeFileSync(resolve(category,'README.md'),'# Delivery\n');
    writeFileSync(resolve(nested,'probe.md'),'# Probe\n');
    expectFailure(()=>requireValue(documentationArticles(fixture).length===0,'Nested documentation omission'),'Nested documentation omission','Nested documentation omission');
    requireValue(documentationArticles(fixture).length===1&&documentationArticles(fixture)[0]==='docs/features/delivery/nested/probe.md','Nested documentation discovery is incomplete.');
  }finally{rmSync(fixture,{recursive:true,force:true});}
}
function runStaleOutputProbe(){
  const fixture=mkdtempSync(resolve(tmpdir(),'keepassxc-site-output-'));
  try{
    const stale=resolve(fixture,'stale.txt');
    writeFileSync(stale,'obsolete');
    expectFailure(()=>assertAbsent(stale,'Stale output'),'Stale output','Stale output survived output recreation.');
    recreateOutputDirectory(fixture);
    assertAbsent(stale,'Stale output');
  }finally{rmSync(fixture,{recursive:true,force:true});}
}
function runManifestSchemaProbe(manifest){
  const duplicate=structuredClone(manifest);
  duplicate.articles[1].id=duplicate.articles[0].id;
  expectFailure(()=>validateContentManifest(duplicate),'Duplicate content-manifest ID','Content manifest must list every documented feature article exactly once.');
  const incomplete=structuredClone(manifest);
  incomplete.articles[0].title.en='';
  expectFailure(()=>validateContentManifest(incomplete),'Empty localized title','Content manifest localized titles must be non-empty.');
  const fabricated=structuredClone(manifest);
  fabricated.articles[0].evidence.path='docs/features/../outside.md';
  expectFailure(()=>validateContentManifest(fabricated),'Escaping evidence provenance','Content manifest evidence provenance is invalid.');
  const mutableRef=structuredClone(manifest);
  mutableRef.articles[0].evidence.ref='main';
  mutableRef.articles[0].evidence.url=`${documentationRoot}${mutableRef.articles[0].article}`;
  expectFailure(()=>validateContentManifest(mutableRef),'Mutable evidence ref','Content manifest evidence provenance is invalid.');
  const mutableUrl=structuredClone(manifest);
  mutableUrl.articles[0].evidence.url=`${documentationRoot}${mutableUrl.articles[0].article}`;
  expectFailure(()=>validateContentManifest(mutableUrl),'Mutable evidence URL','Content manifest evidence provenance is invalid.');
  const duplicateFeatureId=structuredClone(manifest);
  duplicateFeatureId.articles[0].statusProvenance.featureIds.push(duplicateFeatureId.articles[0].statusProvenance.featureIds[0]);
  expectFailure(()=>validateContentManifest(duplicateFeatureId),'Duplicate status provenance feature ID','Content manifest status provenance is invalid.');
  expectFailure(()=>assertEvidenceAncestry(manifest.evidenceCommit,buildSourceCommit,()=>false),'Unrelated evidence history','Evidence commit is not an ancestor of the build source commit.');
  assertEvidenceAncestry(manifest.evidenceCommit,buildSourceCommit,()=>true);
  const duplicateInventory=structuredClone(readGitJson(manifest.evidenceCommit,manifestInventoryPath));
  duplicateInventory.rows.push(structuredClone(duplicateInventory.rows.find(row=>row.article?.file===manifest.articles[0].article)));
  expectFailure(()=>validateContentManifest(manifest,documentationDirectory,duplicateInventory),'Duplicate mapped inventory ID','Feature inventory contains duplicate mapped IDs for one surface.');
  const duplicateAcrossArticles=structuredClone(manifest);
  const firstFeatureId=duplicateAcrossArticles.articles[0].statusProvenance.featureIds[0];
  duplicateAcrossArticles.articles[1].statusProvenance.featureIds.push(firstFeatureId);
  expectFailure(()=>validateContentManifest(duplicateAcrossArticles),'Cross-record duplicate status provenance feature ID','Content manifest maps one status provenance feature ID to multiple article records.');
  const changedCommit='a30d109626b35fff6331e5c4450b3f98da5d8837';
  const changedPath='docs/features/delivery/repair-verification-2026-09.md';
  requireValue(isAncestor(changedCommit,buildSourceCommit)&&blobAt(changedCommit,changedPath)!==blobAt(buildSourceCommit,changedPath),'Changed article fixture no longer has distinct evidence bytes.');
  const changedArticle=structuredClone(manifest);
  changedArticle.evidenceCommit=changedCommit;
  changedArticle.articles.forEach(entry=>{entry.evidence.ref=changedCommit;entry.evidence.url=`https://github.com/${repositoryName}/blob/${changedCommit}/${entry.article}`;});
  expectFailure(()=>validateContentManifest(changedArticle,documentationDirectory,readGitJson(manifest.evidenceCommit,manifestInventoryPath)),'Changed article bytes','Current article bytes differ from immutable evidence.');
  validateContentManifest(manifest);
  validateContentManifest(manifest);
}
const buildProbe=process.env.KPXC_BUILD_PROBE;
if(buildProbe==='nested-article')runNestedArticleProbe();
else if(buildProbe==='stale-output')runStaleOutputProbe();
else if(buildProbe&&buildProbe!=='published-release'&&buildProbe!=='manifest-schema')throw Error(`Unknown build probe: ${buildProbe}`);

recreateOutputDirectory(outputDirectory);
const bundled=await build({absWorkingDir:root,entryPoints:['app.js','search-worker.js'],outdir:'dist',bundle:true,format:'esm',target:'es2022',minify:true,metafile:true});
for(const file of ['index.html','styles.css','release.json','content-manifest.json'])copyFileSync(new URL(file,import.meta.url),new URL(`dist/${file}`,import.meta.url));
if(process.env.KPXC_REFRESH_RELEASE==='1')execFileSync(process.execPath,[fileURLToPath(new URL('../scripts/fetch-site-release-data.mjs',import.meta.url)),fileURLToPath(new URL('dist/release.json',import.meta.url))],{cwd:root,stdio:'inherit',timeout:100000});
const release=validateRelease(readJson(resolve(outputDirectory,'release.json')));
if(buildProbe==='published-release'){
  const fabricated=structuredClone(release);
  fabricated.installer.sha256='0'.repeat(64);
  expectFailure(()=>validatePublishedRelease(fabricated),'Fabricated release metadata','Published installer asset differs from tracked metadata.');
}
validatePublishedRelease(release);
const contentManifest=readJson(resolve(root,'content-manifest.json'));
validateContentManifest(contentManifest);
if(buildProbe==='manifest-schema')runManifestSchemaProbe(contentManifest);
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
writeFileSync(new URL('dist/build-provenance.json',import.meta.url),JSON.stringify({schemaVersion:1,version:release.version,sourceCommit:buildSourceCommit,productReleaseSourceCommit:release.sourceCommit,updatedAtUtc:release.updatedAtUtc,updatedAtSource:release.updatedAtSource},null,2)+'\n');
console.log(`Built website ${release.version} from source ${buildSourceCommit} with published release provenance ${release.sourceCommit}.`);
