'use strict';
const vm=require('vm'),fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..');
function makeClassList(){const s=new Set();return {add:x=>s.add(x),remove:x=>s.delete(x),toggle:(x,on)=>on?s.add(x):s.delete(x)}}
class El{
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
const canvasCtx={fillRect(){},beginPath(){},ellipse(){},fill(){},fillText(){},save(){},restore(){},translate(){},scale(){},drawImage(){}};
els.stage.getContext=()=>canvasCtx;
let raf;
const env={window:{addEventListener(){}},document:{getElementById:id=>els[id],createElement:tag=>new El(tag),querySelectorAll:query=>query==='.tab'?tabs:[]},Image:class {set src(v){this._src=v;this.complete=true;this.naturalWidth=24;this.naturalHeight=70}get src(){return this._src}},requestAnimationFrame:f=>raf=f,console,Math,Set};
vm.createContext(env);
const html=fs.readFileSync(path.join(root,'app/src/main/assets/index.html'),'utf8');
const scripts=[...html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)].map(x=>x[1]);
if(scripts.length!==2)throw Error('Expected exactly two game scripts');
scripts.forEach(s=>new vm.Script(s));
scripts.forEach(s=>vm.runInContext(s,env));
const get=()=>env.window.poseWalkDebug();
if(get().total!==28)throw Error('Not 28 poses');
if(els['pose-strip'].children.length!==28)throw Error('Wrong tile count');
for(let t=0;t<5;t++){const old=raf;old((t+1)*100)}
if(get().pose!==0)throw Error('Startup pose mismatch');
els.next.dispatch('click');if(get().pose!==1)throw Error('Next pose failed');
els.previous.dispatch('click');if(get().pose!==0)throw Error('Previous pose failed');
els['move-right'].dispatch('pointerdown');for(let t=6;t<16;t++){const old=raf;old(t*100)}
if(get().x<=192||get().moving!==1)throw Error('Walk control failed');
els['move-right'].dispatch('pointerup');if(get().moving!==0)throw Error('Walk release failed');
tabs[2].dispatch('click');if(get().pose!==8)throw Error('Sit filter failed');
els.reset.dispatch('click');if(get().pose!==0||get().x!==192)throw Error('Reset failed');
console.log('PASS: 28 sprites, drawing loop, next/prev, walking/release, category select, reset');
