import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {readMetadataJson} from '../site/metadata.mjs';

const manifest=readFileSync(new URL('../site/content-manifest.json',import.meta.url));
assert(manifest.length>32768,'The regression input must cross the old shared limit');
let requests=0,cancelled=false;
const response=(bytes,chunkSize=1024)=>{
    let offset=0;
    return new Response(new ReadableStream({
        pull(controller){if(offset>=bytes.length){controller.close();return;}controller.enqueue(bytes.subarray(offset,offset+chunkSize));offset+=chunkSize;},
        cancel(){cancelled=true;},
    }));
};
const fetcher=bytes=>async(path,options)=>{requests++;assert.equal(options.redirect,'error');assert(options.signal instanceof AbortSignal);return response(bytes);};
const loaded=await readMetadataJson('./content-manifest.json',fetcher(manifest));
assert.equal(loaded.articles.length,JSON.parse(manifest).articles.length);
const padded=Buffer.from(JSON.stringify({value:'x'.repeat(32768)}));
await assert.rejects(readMetadataJson('./release.json',fetcher(padded)),/too large/);
await assert.rejects(readMetadataJson('./build-provenance.json',fetcher(padded)),/too large/);
const oversized=Buffer.from(JSON.stringify({value:'x'.repeat(270000)}));
await assert.rejects(readMetadataJson('./content-manifest.json',fetcher(oversized)),/too large/);
assert(cancelled,'Oversized input must cancel the stream');
await assert.rejects(readMetadataJson('./content-manifest.json',fetcher(Buffer.from([0xff]))));
await assert.rejects(readMetadataJson('./content-manifest.json',fetcher(Buffer.from('{'))));
const before=requests;
await assert.rejects(readMetadataJson('./unknown.json',fetcher(manifest)),/Unsupported/);
assert.equal(requests,before,'Unknown routes must fail before a request');
await assert.rejects(readMetadataJson('./release.json',async()=>new Response(null,{status:503})),/unavailable/);
console.log('PASS route-owned metadata bounds: real large manifest, release/build limits, oversized cancellation, invalid UTF-8/JSON, unknown route and HTTP failure.');
