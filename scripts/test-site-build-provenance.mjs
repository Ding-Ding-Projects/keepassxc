import assert from 'node:assert/strict';
import {createWebsiteProvenance,validateWebsiteProvenance} from '../site/build-provenance.mjs';
const timestamp='2026-10-11T06:45:00.000Z';
const value=createWebsiteProvenance('2.8.1','a'.repeat(40),'b'.repeat(40),timestamp);
assert.equal(value.updatedAtUtc,timestamp);
assert.equal(value.sourceCommit,'a'.repeat(40));
assert.equal(value.productReleaseSourceCommit,'b'.repeat(40));
assert.equal(value.buildIdentity,'2.8.1+'+'a'.repeat(12));
for(const [key,replacement] of [['updatedAtSource','build-provenance.generatedAtUtc'],['updatedAtUtc','2026-02-30T00:00:00.000Z'],['sourceCommit','invalid'],['buildIdentity','2.8.1+'+'b'.repeat(12)],['productReleaseSourceCommit',null]]){
  assert.throws(()=>validateWebsiteProvenance({...value,[key]:replacement}));
}
for(const key of Object.keys(value)){const omitted={...value};delete omitted[key];assert.throws(()=>validateWebsiteProvenance(omitted));}
for(const key of ['version','sourceCommit','productReleaseSourceCommit']){
  for(const replacement of [[value[key]],{value:value[key]},null,42]){
    assert.throws(()=>validateWebsiteProvenance({...value,[key]:replacement}));
  }
}
assert.equal(validateWebsiteProvenance(value),value);
console.log('PASS website build provenance: source and release boundaries, recorded build time, identity mismatch, invalid date and every missing field.');
