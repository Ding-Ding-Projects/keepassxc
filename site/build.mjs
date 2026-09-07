import {build} from 'esbuild';
import {mkdirSync,copyFileSync,readFileSync,writeFileSync,readdirSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('.',import.meta.url));
const sourceCommit=execFileSync('git',['rev-parse','HEAD'],{cwd:root,encoding:'utf8'}).trim();
const changes=execFileSync('git',['status','--porcelain','--untracked-files=normal','--','site'],{cwd:fileURLToPath(new URL('../',import.meta.url)),encoding:'utf8'}).trim();
if(changes)throw Error('Commit website source before producing its versioned build.');
const version=JSON.parse(readFileSync(new URL('package.json',import.meta.url),'utf8')).version;
mkdirSync(new URL('dist/',import.meta.url),{recursive:true});
const bundled=await build({absWorkingDir:root,entryPoints:['app.js','search-worker.js'],outdir:'dist',bundle:true,format:'esm',target:'es2022',minify:true,metafile:true});
for(const file of ['index.html','styles.css','release.json'])copyFileSync(new URL(file,import.meta.url),new URL('dist/'+file,import.meta.url));
if(process.env.KPXC_REFRESH_RELEASE==='1')execFileSync(process.execPath,[fileURLToPath(new URL('../scripts/fetch-site-release-data.mjs',import.meta.url)),fileURLToPath(new URL('dist/release.json',import.meta.url))],{cwd:root,stdio:'inherit',timeout:100000});
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
writeFileSync(new URL('dist/build-provenance.json',import.meta.url),JSON.stringify({schemaVersion:1,version,sourceCommit,updatedAtUtc:new Date().toISOString()},null,2)+'\n');
console.log(`Built website ${version} from ${sourceCommit}.`);
