const limits=Object.freeze({
    './build-provenance.json':32768,
    './release.json':32768,
    './content-manifest.json':262144,
});

export async function readMetadataJson(path,fetcher=globalThis.fetch){
    if(!Object.hasOwn(limits,path))throw Error('Unsupported metadata route');
    const controller=new AbortController();
    let reader;
    const deadline=setTimeout(()=>{controller.abort();if(reader)void reader.cancel().catch(()=>{});},15000);
    try{
        const response=await fetcher(path,{cache:'no-cache',redirect:'error',signal:controller.signal});
        if(!response.ok||!response.body)throw Error('Metadata unavailable');
        reader=response.body.getReader();
        const chunks=[];let total=0;
        while(true){
            const {done,value}=await reader.read();
            if(done)break;
            total+=value.byteLength;
            if(total>limits[path]){await reader.cancel();throw Error('Metadata too large');}
            chunks.push(value);
        }
        const bytes=new Uint8Array(total);let offset=0;
        for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}
        return JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes));
    }finally{clearTimeout(deadline);reader?.releaseLock();}
}
