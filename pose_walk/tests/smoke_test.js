'use strict';
const vm=require('vm'),fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..');
function makeClassList(){const s=new Set();return {add:x=>s.add(x),remove:x=>s.delete(x),toggle:(x,on)=>on?s.add(x):s.delete(x)}}
class El {
  constructor(id){this.id=id;this.listeners={};this.dataset={};this.classList=makeClassList();this.children=[];this.hidden=false;this.scrollLeft=0;}
  addEventListener(k,fn){(this.listeners[k]??=[]).push(fn)}
  append(...children){this.children.push(...children)}
  dispatch(k,v={}){(this.listeners[k]||[]).forEach(fn=>fn({preventDefault:()=>{},pointerId:3,...v}));}
  setPointerCapture(){}
  setAttribute(k,v){this[k]=v}
  scrollIntoView(){}
}
const els={};for(const id of ['stage','pose-strip','frame-status','pose-number','previous','next','random','reset','move-left','move-right'])els[id]=new El(id);
const tabs=['all','stand','sit','floor','walk'].map(name=>{const e=new El('tab-'+name);e.dataset.filter=name;return e});
const render={flipped:false,lastImage:null,samples:[]};
const canvasCtx={
  fillRect(){},beginPath(){},ellipse(){},fill(){},fillText(){},save(){},restore(){render.flipped=false},translate(){},
  scale(x,y){if(x===-1 && y===1) render.flipped=true},
  drawImage(im){render.lastImage=im.src;render.samples.push({src:im.src,flipped:render.flipped})}
};
els.stage.getContext=()=>canvasCtx;
let raf;
const env={
  window:{addEventListener(){}},
  document:{getElementById:id=>els[id],createElement:tag=>new El(tag),querySelectorAll:query=>query==='.tab'?tabs:[]},
  Image:class{set src(v){this._src=v;this.complete=true; const bin=Buffer.from(v.split(',')[1]||'','base64'); this.naturalWidth=bin.readUInt32BE(16);this.naturalHeight=bin.readUInt32BE(20)} get src(){return this._src}},
  requestAnimationFrame:f=>raf=f, console,Math,Set
};
vm.createContext(env);
const html=fs.readFileSync(path.join(root,'app/src/main/assets/index.html'),'utf8');
const scripts=[...html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)].map(x=>x[1]);
if(scripts.length!==3) throw Error('Expected exactly three game scripts');
scripts.forEach(s=>new vm.Script(s));scripts.forEach(s=>vm.runInContext(s,env));
const get=()=>env.window.poseWalkDebug();
const walk=env.window.WALK_CYCLE;
if(get().total!==28 || get().walkFrames!==12 || walk.length!==12)throw Error('Missing 28 poses or 12 coherent frames');
if(els['pose-strip'].children.length!==28)throw Error('Wrong pose count');
let now=0;
function step(){now+=50;const f=raf;f(now);return render.samples.at(-1)}
for(let t=0;t<5;t++)step();
if(get().pose!==0)throw Error('Startup pose mismatch');
els.next.dispatch('click');if(get().pose!==1)throw Error('Next pose failed');
els.previous.dispatch('click');if(get().pose!==0)throw Error('Previous pose failed');
const xStart=get().x;
els['move-right'].dispatch('pointerdown');
let r=[];for(let t=0;t<35;t++)r.push({...step(), frame:get().walkFrame});
if(get().x<=xStart||get().moving!==1||get().dir!==1)throw Error('Right walk control failed');
if(r.some(x=>x.flipped))throw Error('Moving RIGHT must never mirror gait frames');
if(r.some(x=>!walk.some(p=>p.src===x.src)))throw Error('Unknown RIGHT walking frame');
if(new Set(r.map(x=>x.frame)).size!==12 || new Set(r.map(x=>x.src)).size!==12)throw Error('12-frame gait timing or 12 unique images missing RIGHT');
els['move-right'].dispatch('pointerup');if(get().moving!==0)throw Error('Right walk release failed');
els['move-left'].dispatch('pointerdown');
let l=[];for(let t=0;t<35;t++)l.push({...step(), frame:get().walkFrame});
if(get().moving!==-1||get().dir!==-1)throw Error('Left walk control failed');
if(l.some(x=>!x.flipped))throw Error('Moving LEFT must mirror every gait frame');
if(l.some(x=>!walk.some(p=>p.src===x.src)))throw Error('Unknown LEFT walking frame');
if(new Set(l.map(x=>x.frame)).size!==12 || new Set(l.map(x=>x.src)).size!==12)throw Error('12-frame gait timing or 12 unique images missing LEFT');
els['move-left'].dispatch('pointerup');if(get().moving!==0)throw Error('Left walk release failed');
tabs[2].dispatch('click');if(get().pose!==8)throw Error('Sit filter failed');
els.reset.dispatch('click');if(get().pose!==0||get().x!==192)throw Error('Reset failed');
console.log('PASS: 28 originals + 12 coherent frames, right all unflipped, left all mirrored, controls, reset');
