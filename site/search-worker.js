self.onmessage=({data})=>{try{
  if(typeof data.pattern!=='string'||data.pattern.length>300||typeof data.flags!=='string'||!/^[dgimsuvy]*$/.test(data.flags)||new Set(data.flags).size!==data.flags.length||!Array.isArray(data.items)||data.items.length>100||data.items.some(item=>typeof item!=='string'||item.length>1000))throw Error('Invalid input');
  const pattern=new RegExp(data.pattern,data.flags);
  const indices=data.items.flatMap((item,index)=>{pattern.lastIndex=0;return pattern.test(item)?[index]:[];});
  self.postMessage({indices});
}catch{self.postMessage({error:true});}};
