import {execFileSync} from 'node:child_process';
import {writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {buildReleaseData,parseReleaseJson} from './site-release-data.mjs';

const destination=process.argv[2];
const tag=process.argv[3]||'';
if(!destination|| (tag&&!/^v\d+\.\d+\.\d+$/.test(tag)))throw Error('Usage: node scripts/fetch-site-release-data.mjs OUTPUT_JSON [vMAJOR.MINOR.PATCH]');
const repository='Ding-Ding-Projects/keepassxc';
const request=(args)=>parseReleaseJson(execFileSync('gh',args,{timeout:30000,maxBuffer:1024*1024}));
const raw=request(['api',`repos/${repository}/releases/${tag?'tags/'+tag:'latest'}`]);
const release={tagName:raw.tag_name,isDraft:raw.draft,isPrerelease:raw.prerelease,publishedAt:raw.published_at,assets:raw.assets.map(asset=>({name:asset.name,url:asset.browser_download_url,size:asset.size}))};
if(!/^v\d+\.\d+\.\d+$/.test(release.tagName))throw Error('Invalid published release tag.');
let object=request(['api',`repos/${repository}/git/ref/tags/${release.tagName}`]).object;
for(let depth=0;object?.type==='tag'&&depth<4;depth++){
  if(!/^[a-f0-9]{40}$/i.test(object.sha))throw Error('Invalid annotated tag object.');
  object=request(['api',`repos/${repository}/git/tags/${object.sha}`]).object;
}
if(object?.type!=='commit'||!/^[a-f0-9]{40}$/i.test(object.sha))throw Error('Release tag does not resolve to a commit within four annotations.');
release.targetCommit=object.sha;
const assetJson=(name)=>{
  const matches=raw.assets.filter(asset=>asset.name===name);
  if(matches.length!==1||!Number.isSafeInteger(matches[0].id)||matches[0].id<=0||!Number.isSafeInteger(matches[0].size)||matches[0].size<=0||matches[0].size>1024*1024)throw Error('Missing, duplicated, or oversized release metadata: '+name);
  return request(['api','-H','Accept: application/octet-stream',`repos/${repository}/releases/assets/${matches[0].id}`]);
};
const projected=buildReleaseData(release,assetJson('build-provenance.json'),assetJson('update-manifest-v1.json'),assetJson('artifact-receipt.json'));
writeFileSync(resolve(destination),JSON.stringify(projected,null,2)+'\n');
console.log('Validated published release '+projected.tag+' for website assembly.');
