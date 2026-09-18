self.onmessage=({data})=>{try{
  if(typeof data.pattern!=='string'||data.pattern.length>300||typeof data.flags!=='string'||!/^[dgimsuvy]*$/.test(data.flags)||new Set(data.flags).size!==data.flags.length||!Array.isArray(data.items)||data.items.length>100||data.items.some(item=>typeof item!=='string'||item.length>1000))throw Error('Invalid input');
  const pattern=new RegExp(data.pattern,data.flags);
  if(typeof data.sample==='string'){
    if(data.sample.length>2000)throw Error('Invalid sample');
    pattern.lastIndex=0;const match=pattern.exec(data.sample);
    self.postMessage({preview:match?{match:match[0],captures:match.slice(1).map(value=>value??'')}:{match:'',captures:[]}});return;
  }
  const indices=data.items.flatMap((item,index)=>{pattern.lastIndex=0;return pattern.test(item)?[index]:[];});
  self.postMessage({indices});
}catch{self.postMessage({error:true});}};
