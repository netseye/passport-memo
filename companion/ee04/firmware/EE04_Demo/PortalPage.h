#pragma once
static const char EE04_PORTAL[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>EE04 桌面信息屏</title>
<style>
:root{color-scheme:light;font-family:system-ui,sans-serif;color:#20332c;background:#eef1e9}*{box-sizing:border-box}body{max-width:650px;margin:auto;padding:24px 16px 40px}header{display:flex;justify-content:space-between;align-items:center}h1{font-size:24px;margin:8px 0}h2{font-size:18px;margin:0 0 14px}.tag{font-size:12px;letter-spacing:1px;color:#657568}section,details{background:#fffef8;border:1px solid #d7dfd1;border-radius:16px;padding:20px;margin:18px 0}.status{font-size:13px;white-space:pre-wrap;line-height:1.7;color:#506452}label{display:block;font-size:14px}input,textarea,select,button{font:inherit;width:100%;padding:12px;border:1px solid #cbd5c6;border-radius:8px;margin:7px 0 15px}textarea{resize:vertical;line-height:1.7;min-height:150px}button{border:0;background:#244a3a;color:white;cursor:pointer}button:disabled{opacity:.45;cursor:wait}button.secondary{background:#e6eddf;color:#284534}small,.hint{font-size:12px;color:#627260;line-height:1.7}summary{cursor:pointer;font-weight:600}details form{margin-top:18px}.row{display:flex;gap:12px;align-items:center}.row>*{flex:1}.count{text-align:right;font-size:12px;color:#60705c}.message{white-space:pre-wrap;font-size:14px;line-height:1.6}canvas{width:100%;height:auto;image-rendering:pixelated;background:white;border:1px solid #9fae99;border-radius:4px}.preview{padding:10px;background:#e0e6d9;border-radius:10px}.checkbox{display:flex;align-items:center;gap:8px}.checkbox input{width:auto;margin:8px 0}.error{color:#aa332c}a{color:#285844}
fieldset{border:0;padding:0;margin:18px 0 0;min-width:0}legend{font-size:14px;color:#627260;margin-bottom:8px}input[type=range]{padding:0;accent-color:#244a3a}.row>*{min-width:0}output{float:right;color:#627260}
</style></head><body>
<header><div><span class="tag">EE04 · V1.7</span><h1>桌面信息屏</h1></div><span class="tag">本地管理</span></header>
<p id="status" class="status">正在连接开发板…</p>
<section><h2>写一张便签</h2><p class="hint">支持中文、英文和换行，最多240个字符。长内容自动分张，在便签页按 K2 翻张；少见汉字或表情可能显示为问号。</p>
<form id="noteForm"><label for="note">便签内容</label><textarea id="note" placeholder="今天的三件小事&#10;读书30分钟&#10;记得带伞"></textarea><div class="count" id="count">0 / 240</div><button id="publish" disabled>保存并显示到屏幕</button></form><p class="hint">留空后保存可清空便签。便签保存在开发板，断网、重启后仍保留。</p><p id="noteMessage" class="message" role="status"></p></section>
<section><h2>图片展示 · 8/8</h2><p class="hint">选择 JPG、PNG 或浏览器支持的其他图片，最大10MB / 3200万像素。原图只在手机本地处理，开发板保存一张296×128黑白图；新图会替换旧图。</p>
<label for="imageFile">选择图片</label><input id="imageFile" type="file" accept="image/*">
<div class="preview"><canvas id="imagePreview" width="296" height="128" aria-label="图片黑白预览"></canvas></div>
<p class="hint" id="imageInfo">尚未选择图片</p>
<fieldset id="imageControls" disabled><legend>调整画面</legend>
<div class="row"><label for="imageFit">构图<select id="imageFit"><option value="contain">完整显示（留白）</option><option value="cover">裁切铺满</option></select></label><button id="imageRotate" class="secondary" type="button">顺时针旋转90°</button></div>
<label for="imageZoom">放大 <output id="zoomValue">1.0×</output></label><input id="imageZoom" type="range" min="1" max="3" step="0.1" value="1">
<div class="row"><label for="imageX">左右位置<input id="imageX" type="range" min="-100" max="100" value="0"></label><label for="imageY">上下位置<input id="imageY" type="range" min="-100" max="100" value="0"></label></div>
<label for="imageLight">亮度 <output id="lightValue">0</output></label><input id="imageLight" type="range" min="-100" max="100" value="0">
<label for="imageMethod">黑白转换<select id="imageMethod"><option value="dither">照片：抖动模拟灰阶</option><option value="threshold">文字 / 线稿：直接黑白</option></select></label>
<label class="checkbox"><input id="imageInvert" type="checkbox">反色（黑白互换）</label>
<button id="imageReset" class="secondary" type="button">重置调整</button></fieldset>
<button id="imagePublish" type="button" disabled>保存图片并显示</button>
<div class="row"><button id="imageShow" class="secondary" type="button" disabled>显示已保存图片</button><button id="imageClear" class="secondary" type="button" disabled>清除已保存图片</button></div>
<p id="imageMessage" class="message" role="status"></p><small>全屏图片不叠加页码和按钮提示。短按K1前页、K3首页，长按K2再次进入手机管理。</small></section>
<section><h2>Passport 语音备忘录</h2><p class="hint">启用后，Passport 可通过内网直接同步备忘录。管理窗口关闭后仍能接收，配对码只在生成时显示。</p><button type="button" id="passportPair">启用 / 重新生成配对码</button><button type="button" id="passportOff" class="secondary">关闭联动</button><p id="passportMessage" class="message"></p></section>
<section><h2>屏幕预览</h2><div class="preview"><canvas id="screen" width="296" height="128"></canvas></div><button class="secondary" id="preview" type="button">读取当前画面</button><small>显示最近一次成功更新的画面，支持局刷同步。首页/时钟小范围变化使用局刷；换页与换图全刷约2秒，连续全刷另有3秒间隔。</small></section>
<details><summary>Wi-Fi 与天气城市</summary><p class="hint">先保存2.4GHz Wi-Fi，联网校时后再搜索城市。用热点连接时，手机请选择保持连接。</p>
<form id="form"><label>Wi-Fi 名称<input id="ssid" name="ssid" required maxlength="32"></label><label>Wi-Fi 密码<input id="password" name="password" type="password" maxlength="63" autocomplete="new-password" placeholder="原网络不改密码可留空"></label><label class="checkbox"><input type="checkbox" name="open" value="1">这是无密码网络</label><label>固定 UTC 时差（中国为8）<input id="offset" name="offset" type="number" step="0.25" min="-12" max="14" value="8" required></label>
<label>搜索城市<input id="query" placeholder="如 北京 / 上海 / Hangzhou"></label><button id="search" type="button" disabled>联网并校时后可搜索城市</button><select id="results"><option>尚未选择城市</option></select>
<input type="hidden" id="selected" name="selected" value="0"><input type="hidden" id="city" name="city"><input type="hidden" id="lat" name="lat"><input type="hidden" id="lon" name="lon"><button>保存网络设置</button></form><p id="message" class="message" role="status"></p></details>
<p class="hint">长按 K2 开启管理，窗口开放10分钟。也可在同一局域网打开屏幕显示的IP，用户名 <b>ee04</b>，密码与屏幕上的热点密码相同。天气使用 Open-Meteo，地名由 GeoNames 提供；旧天气会标注缓存。屏幕没有断电走时的独立时钟，重启后需联网校时。</p>
<script>
const $=id=>document.getElementById(id);let choices=[],loaded=false,busySearch=false,token='',polling=false;
const count=()=>Array.from($('note').value.replace(/\r\n?/g,'\n')).length;
function updateCount(){const n=count();$('count').textContent=`${n} / 240`;$('count').classList.toggle('error',n>240);$('publish').disabled=!token||n>240;}
async function request(url,options={}){const r=await fetch(url,{cache:'no-store',...options});if(!r.ok)throw Error(await r.text());return r;}
async function state(){if(polling)return;polling=true;try{const s=await(await request('/state')).json();token=s.token;
$('status').textContent=`${s.online?'Wi-Fi 已连接':'离线运行'} · ${s.clockReady?'时钟已同步':'等待校时'} · 管理剩余 ${Math.ceil(s.apSeconds/60)} 分钟\n天气城市：${s.city}${s.hasWeather?(s.weatherStale?' · 缓存数据':' · 已更新'):''}${s.storageError?'\n'+s.storageError:''}`;
if(!busySearch){$('search').disabled=!(s.online&&s.clockReady);$('search').textContent=s.online&&s.clockReady?'搜索城市':'联网并校时后可搜索城市';}
if(!imageBusy)hasSavedImage=!!s.hasImage;updateImageButtons();
if(!loaded&&s.hasImage)loadSavedImage();
if(!loaded){$('ssid').value=s.ssid;$('offset').value=s.offset;$('note').value=s.note;loaded=true;}updateCount();
}catch(e){$('status').textContent='连接中断：请长按 K2 开启管理，再刷新本页。';token='';updateCount();updateImageButtons();}finally{polling=false;}}
$('note').oninput=updateCount;
$('noteForm').onsubmit=async e=>{e.preventDefault();if(count()>240)return;$('publish').disabled=true;try{const r=await request('/note',{method:'POST',headers:{'X-EE04-Token':token},body:new URLSearchParams({text:$('note').value})});const s=await r.json();$('noteMessage').textContent=`已保存（第${s.revision}版），正在更新屏幕。`;setTimeout(preview,5500);}catch(e){$('noteMessage').textContent='保存失败：'+e.message;}finally{updateCount();}};
async function preview(){try{const data=new Uint8Array(await(await request('/screen')).arrayBuffer());if(data.length!==4736)throw Error('图像尚未准备好');const ctx=$('screen').getContext('2d'),im=ctx.createImageData(296,128);for(let y=0;y<128;y++)for(let x=0;x<296;x++){const px=127-y,py=x;const white=data[py*16+(px>>3)]&(128>>(px&7));const i=(y*296+x)*4;im.data[i]=im.data[i+1]=im.data[i+2]=white?255:0;im.data[i+3]=255;}ctx.putImageData(im,0,0);}catch(e){$('noteMessage').textContent='预览：'+e.message;}}
$('preview').onclick=preview;
$('search').onclick=async()=>{busySearch=true;$('search').disabled=true;try{$('message').textContent='搜索中…';const r=await request('/search?q='+encodeURIComponent($('query').value));choices=(await r.json()).results||[];$('results').replaceChildren(new Option('请选择城市',''));choices.forEach((c,i)=>$('results').add(new Option([c.name,c.admin1,c.country].filter(Boolean).join(' / '),String(i))));$('selected').value='0';$('message').textContent=choices.length?'选择匹配城市后保存':'没有找到城市';}catch(e){$('message').textContent=e.message;}finally{busySearch=false;state();}};
$('results').onchange=()=>{const c=choices[$('results').value];$('selected').value=c?'1':'0';if(c){$('city').value=c.name;$('lat').value=c.latitude;$('lon').value=c.longitude;}};
$('form').onsubmit=async e=>{e.preventDefault();try{await request('/save',{method:'POST',headers:{'X-EE04-Token':token},body:new URLSearchParams(new FormData($('form')))});$('message').textContent='已保存，等待联网、校时和天气更新。';$('password').value='';$('selected').value='0';}catch(e){$('message').textContent='保存失败：'+e.message;}};
// One bit per pixel, landscape row-major, MSB first, 1 = black.
function monochrome(rgba,width,height,brightness=0,dither=true,invert=false){
  const gray=new Float32Array(width*height),bits=new Uint8Array(Math.ceil(width*height/8));
  for(let i=0;i<gray.length;i++){const a=rgba[i*4+3]/255;gray[i]=Math.max(0,Math.min(255,(.2126*rgba[i*4]+.7152*rgba[i*4+1]+.0722*rgba[i*4+2])*a+255*(1-a)+brightness));}
  for(let y=0;y<height;y++)for(let x=0;x<width;x++){
    const i=y*width+x,black=gray[i]<128;
    if(black!==invert)bits[i>>3]|=128>>(i&7);
    if(dither){const e=gray[i]-(black?0:255);if(x+1<width)gray[i+1]+=e*7/16;if(y+1<height){if(x>0)gray[i+width-1]+=e*3/16;gray[i+width]+=e*5/16;if(x+1<width)gray[i+width+1]+=e/16;}}
  }return bits;
}
function paintBitmap(canvas,bits){const ctx=canvas.getContext('2d'),im=ctx.createImageData(296,128);for(let i=0;i<296*128;i++){const v=bits[i>>3]&(128>>(i&7))?0:255;im.data[i*4]=im.data[i*4+1]=im.data[i*4+2]=v;im.data[i*4+3]=255;}ctx.putImageData(im,0,0);}
let imageSource=null,imageTurn=0,imageHex='',imageBusy=false,imageLoading=false,hasSavedImage=false,imageGeneration=0;
function updateImageButtons(){
  $('imagePublish').disabled=!token||!imageHex||imageBusy||imageLoading;
  $('imageShow').disabled=$('imageClear').disabled=!token||!hasSavedImage||imageBusy||imageLoading;
  $('imageControls').disabled=!imageSource||imageBusy||imageLoading;$('imageFile').disabled=imageBusy;
}
function resetImageControls(){imageTurn=0;$('imageFit').value='contain';$('imageZoom').value='1';$('imageX').value=$('imageY').value=$('imageLight').value='0';$('imageMethod').value='dither';$('imageInvert').checked=false;}
function renderImage(){
  if(!imageSource)return;
  const rotated=document.createElement('canvas'),quarter=imageTurn%2;
  rotated.width=quarter?imageSource.height:imageSource.width;rotated.height=quarter?imageSource.width:imageSource.height;
  const rc=rotated.getContext('2d');rc.translate(rotated.width/2,rotated.height/2);rc.rotate(imageTurn*Math.PI/2);rc.drawImage(imageSource,-imageSource.width/2,-imageSource.height/2);
  const canvas=document.createElement('canvas');canvas.width=296;canvas.height=128;
  const ctx=canvas.getContext('2d');ctx.fillStyle='white';ctx.fillRect(0,0,296,128);ctx.imageSmoothingEnabled=true;ctx.imageSmoothingQuality='high';
  const scale=($('imageFit').value==='cover'?Math.max:Math.min)(296/rotated.width,128/rotated.height)*Number($('imageZoom').value),w=rotated.width*scale,h=rotated.height*scale;
  const x=(296-w)/2+Number($('imageX').value)/100*Math.abs(296-w)/2,y=(128-h)/2+Number($('imageY').value)/100*Math.abs(128-h)/2;
  ctx.drawImage(rotated,x,y,w,h);
  const bits=monochrome(ctx.getImageData(0,0,296,128).data,296,128,Number($('imageLight').value),$('imageMethod').value==='dither',$('imageInvert').checked);
  imageHex=Array.from(bits,b=>b.toString(16).padStart(2,'0')).join('');paintBitmap($('imagePreview'),bits);
  $('zoomValue').textContent=Number($('imageZoom').value).toFixed(1)+'×';$('lightValue').textContent=$('imageLight').value;updateImageButtons();
}
$('imageFile').onchange=async()=>{
  const file=$('imageFile').files[0];if(!file)return;
  const generation=++imageGeneration;imageSource=null;imageHex='';imageLoading=true;updateImageButtons();
  $('imageInfo').textContent='正在读取图片…';$('imageMessage').textContent='';let url;
  try{
    if(file.size>10*1024*1024)throw Error('文件超过10MB，请先缩小图片');
    url=URL.createObjectURL(file);const im=new Image();im.src=url;await im.decode();
    if(generation!==imageGeneration)return;
    if(!im.naturalWidth||!im.naturalHeight||im.naturalWidth*im.naturalHeight>32000000)throw Error('图片尺寸过大或无效，请缩小到3200万像素以内');
    const source=document.createElement('canvas'),scale=Math.min(1,2048/Math.max(im.naturalWidth,im.naturalHeight));
    source.width=Math.max(1,Math.round(im.naturalWidth*scale));source.height=Math.max(1,Math.round(im.naturalHeight*scale));source.getContext('2d').drawImage(im,0,0,source.width,source.height);
    imageSource=source;resetImageControls();renderImage();$('imageInfo').textContent=`${file.name} · ${im.naturalWidth}×${im.naturalHeight} → 296×128`;
  }catch(e){if(generation===imageGeneration){$('imageInfo').textContent='图片读取失败';$('imageMessage').textContent=e.message+'；可尝试JPG或PNG。';}}
  finally{if(url)URL.revokeObjectURL(url);if(generation===imageGeneration){imageLoading=false;updateImageButtons();}}
};
for(const id of ['imageFit','imageZoom','imageX','imageY','imageLight','imageMethod','imageInvert'])$(id).oninput=renderImage;
$('imageRotate').onclick=()=>{imageTurn=(imageTurn+1)%4;renderImage();};
$('imageReset').onclick=()=>{resetImageControls();renderImage();};
async function loadSavedImage(){
  const generation=imageGeneration;
  try{const bits=new Uint8Array(await(await request('/image')).arrayBuffer());if(bits.length!==4736)throw Error('保存的图片不完整');if(generation===imageGeneration&&!imageSource&&!imageLoading){paintBitmap($('imagePreview'),bits);$('imageInfo').textContent='开发板已保存的图片 · 296×128；选择新图片即可编辑';}}
  catch(e){$('imageMessage').textContent='读取图片：'+e.message;}
}
async function imageAction(path,data,message){
  if(imageBusy||imageLoading||!token)return;
  imageBusy=true;updateImageButtons();$('imageMessage').textContent='正在处理…';
  try{await request(path,{method:'POST',headers:{'X-EE04-Token':token},body:new URLSearchParams(data)});
    if(path==='/image')hasSavedImage=true;
    if(path==='/image/clear'){hasSavedImage=false;if(!imageSource){paintBitmap($('imagePreview'),new Uint8Array(4736));$('imageInfo').textContent='尚未选择图片';}}
    $('imageMessage').textContent=message;setTimeout(preview,5500);
  }catch(e){$('imageMessage').textContent='操作失败：'+e.message;}
  finally{imageBusy=false;updateImageButtons();state();}
}
$('imagePublish').onclick=()=>{if(imageHex)imageAction('/image',{hex:imageHex},'图片已保存，正在切到第8页。重启后仍可查看。');};
$('imageShow').onclick=()=>imageAction('/image/show',{},'已切换到图片页，正在刷新屏幕。');
$('imageClear').onclick=()=>imageAction('/image/clear',{},'已清除保存的图片。');

state().then(preview);setInterval(state,6000);
$('passportPair').onclick=async()=>{try{const r=await request('/passport/pair',{method:'POST',headers:{'X-EE04-Token':token},body:new URLSearchParams({enable:'1'})});const s=await r.json();$('passportMessage').textContent='请将此配对码填写到 Passport：\n'+s.key;}catch(e){$('passportMessage').textContent=e.message;}};
$('passportOff').onclick=async()=>{try{await request('/passport/pair',{method:'POST',headers:{'X-EE04-Token':token},body:new URLSearchParams({enable:'0'})});$('passportMessage').textContent='联动已关闭';}catch(e){$('passportMessage').textContent=e.message;}};
</script></body></html>)HTML";
