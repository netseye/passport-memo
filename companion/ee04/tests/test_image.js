const fs=require('fs'),vm=require('vm'),assert=require('assert');
const portal=fs.readFileSync(require('path').join(__dirname,'../firmware/EE04_Demo/PortalPage.h'),'utf8');
const script=portal.match(/<script>([\s\S]*)<\/script>/)[1];
new vm.Script(script);
const code=script.slice(script.indexOf('function monochrome('),script.indexOf('function paintBitmap('));
const ctx={};vm.createContext(ctx);vm.runInContext(code,ctx);
function rgba(values,alpha=255){return Uint8ClampedArray.from(values.flatMap(v=>[v,v,v,alpha]));}
const mono=ctx.monochrome;
assert.deepEqual(Array.from(mono(rgba([0,255,255,255,255,255,255,255]),8,1,0,false,false)),[128]);
assert.deepEqual(Array.from(mono(rgba([255,255,255,255,255,255,255,0]),8,1,0,false,false)),[1]);
assert.deepEqual(Array.from(mono(rgba(Array(16).fill(0)),8,2,0,true)),[255,255]);
assert.deepEqual(Array.from(mono(rgba(Array(16).fill(255)),8,2,0,true)),[0,0]);
assert.deepEqual(Array.from(mono(rgba(Array(16).fill(0),0),8,2,0,false)),[0,0]); // transparent black becomes white
assert.deepEqual(Array.from(mono(rgba([127,128,127,128,127,128,127,128]),8,1,0,false)),[170]);
assert.deepEqual(Array.from(mono(rgba(Array(8).fill(120)),8,1,10,false)),[0]);
assert.deepEqual(Array.from(mono(rgba(Array(8).fill(140)),8,1,-20,false)),[255]);
const input=rgba(Array.from({length:296*128},(_,i)=>i%256));
const positive=mono(input,296,128,0,true),negative=mono(input,296,128,0,true,true);
assert.equal(positive.length,4736);for(let i=0;i<4736;i++)assert.equal(positive[i]^negative[i],255);
const half=mono(rgba(Array(296*128).fill(128)),296,128,0,true);
const black=Array.from(half).reduce((n,b)=>n+b.toString(2).replace(/0/g,'').length,0);
assert(black>18000&&black<20000);

console.log('PASS client pixel packing, endpoints, threshold, brightness, alpha, inversion and gray dithering');
