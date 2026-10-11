const keys=['schemaVersion','version','sourceCommit','productReleaseSourceCommit','buildIdentity','updatedAtUtc','updatedAtSource'];
export function validateWebsiteProvenance(value){
  if(!value||typeof value!=='object'||Array.isArray(value)||Object.keys(value).sort().join('|')!==[...keys].sort().join('|')
     ||value.schemaVersion!==1||typeof value.version!=='string'||!/^\d+\.\d+\.\d+$/.test(value.version)
     ||typeof value.sourceCommit!=='string'||!/^[a-f0-9]{40}$/.test(value.sourceCommit)
     ||typeof value.productReleaseSourceCommit!=='string'||!/^[a-f0-9]{40}$/.test(value.productReleaseSourceCommit)
     ||value.buildIdentity!==`${value.version}+${value.sourceCommit.slice(0,12)}`
     ||value.updatedAtSource!=='website-build.generatedAtUtc'
     ||typeof value.updatedAtUtc!=='string'||!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/.test(value.updatedAtUtc)
     ||!Number.isFinite(Date.parse(value.updatedAtUtc))||new Date(value.updatedAtUtc).toISOString()!==value.updatedAtUtc)
    throw Error('Invalid website build provenance');
  return value;
}
export function createWebsiteProvenance(version,sourceCommit,productReleaseSourceCommit,generatedAtUtc){
  return validateWebsiteProvenance({schemaVersion:1,version,sourceCommit,productReleaseSourceCommit,
    buildIdentity:`${version}+${sourceCommit.slice(0,12)}`,updatedAtUtc:generatedAtUtc,updatedAtSource:'website-build.generatedAtUtc'});
}
