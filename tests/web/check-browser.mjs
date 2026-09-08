// SPDX-License-Identifier: GPL-3.0-only
// Checks the real page through a headless Chrome debugging port.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const tabs=await (await fetch('http://127.0.0.1:9227/json')).json();
const ws=new WebSocket(tabs.find(t=>t.type==='page').webSocketDebuggerUrl);
await new Promise(r=>ws.addEventListener('open',r,{once:true}));
let id=0;const pending=new Map(),errors=[];
ws.addEventListener('message',e=>{const m=JSON.parse(e.data);if(m.method==='Runtime.exceptionThrown')errors.push(m.params);if(m.id){const p=pending.get(m.id);pending.delete(m.id);m.error?p.reject(m.error):p.resolve(m.result)}});
const send=(method,params={})=>new Promise((resolve,reject)=>{const n=++id;pending.set(n,{resolve,reject});ws.send(JSON.stringify({id:n,method,params}))});
const evaluate=async expression=>{const result=await send('Runtime.evaluate',{expression,returnByValue:true});if(result.exceptionDetails)throw Error(JSON.stringify(result.exceptionDetails));return result.result.value};
const wait=ms=>new Promise(r=>setTimeout(r,ms));
await send('Runtime.enable');
await send('Page.enable');
if(process.argv[2]){await send('Page.navigate',{url:process.argv[2]});await wait(2000)}
await send('Emulation.setDeviceMetricsOverride',{width:1440,height:1000,deviceScaleFactor:1,mobile:false});
await evaluate("document.querySelector('#try-it').scrollIntoView({behavior:'instant'})");
assert.equal(await evaluate("document.querySelector('.demo-controls').disabled"),false);
const rect=await evaluate("(()=>{const r=document.querySelector('.demo-area').getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height}})()");
const mouse=async(x,y,type='mouseMoved')=>send('Input.dispatchMouseEvent',{type,x:rect.x+x,y:rect.y+y,...(type==='mouseMoved'?{}:{button:'left',clickCount:1})});
const state=()=>evaluate("(()=>{const g=document.querySelector('.demo-cursor');const t=new DOMMatrix(getComputedStyle(g).transform);return {hidden:g.hasAttribute('hidden'),x:t.m41,y:t.m42,locked:!!document.pointerLockElement,cursor:getComputedStyle(document.querySelector('.demo-area')).cursor,mark:!document.querySelector('.demo-click').hidden}})()");
await mouse(40,50);await mouse(300,100);await wait(20);
let s=await state();assert.ok(!s.hidden && s.x<250 && s.x>=40,JSON.stringify(s));assert.equal(s.locked,false);assert.notEqual(s.cursor,'none');
await wait(1100);s=await state();assert.ok(Math.abs(s.x-300)<=1 && Math.abs(s.y-100)<=1,JSON.stringify(s));
await mouse(430,200);await mouse(430,200,'mousePressed');await mouse(430,200,'mouseReleased');
const clicked=await state();assert.equal(clicked.mark,true);await wait(300);s=await state();assert.equal(s.x,clicked.x);assert.equal(s.y,clicked.y);
await mouse(-10,-10);assert.equal((await state()).hidden,true);await mouse(110,90);s=await state();assert.equal(s.x,110);assert.equal(s.y,90);
await evaluate("document.querySelector('[data-strength=\"85\"]').click();document.querySelector('#demo-center').click()");
assert.equal(await evaluate("document.querySelector('#demo-strength-value').textContent"),'85%');assert.equal(await evaluate("document.querySelector('#demo-center').checked"),true);
await evaluate("document.querySelector('#demo-toggle').click()");await mouse(140,160);await mouse(230,250);s=await state();assert.equal(s.x,230);assert.equal(s.y,250);
// Reset and keyboard pause.
await evaluate("document.querySelector('#demo-reset').click()");assert.equal((await state()).mark,false);
await evaluate("document.querySelector('#demo-toggle').click();document.querySelector('.demo-area').focus()");
await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});
await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});
assert.equal(await evaluate("document.querySelector('#demo-toggle').getAttribute('aria-pressed')"),'false');
// Slider keyboard operation is native, with live label updates.
await evaluate("document.querySelector('#demo-speed').focus()");
await send('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39});
await send('Input.dispatchKeyEvent',{type:'keyUp',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39});
assert.equal(await evaluate("document.querySelector('#demo-speed-value').textContent"),'101%');
for(const width of [1440,390,320]){
 await send('Emulation.setDeviceMetricsOverride',{width,height:1000,deviceScaleFactor:1,mobile:false});
 const check=await evaluate("({width:innerWidth,overflow:document.documentElement.scrollWidth>innerWidth,minButton:Math.min(...[...document.querySelectorAll('.demo-controls button')].map(a=>a.getBoundingClientRect().height))})");
 assert.equal(check.overflow,false);assert.ok(check.minButton>=48);console.log(check);
}
await send('Emulation.setDeviceMetricsOverride',{width:1440,height:1000,deviceScaleFactor:1,mobile:false});
await evaluate("document.querySelector('#demo-toggle').click();document.querySelector('#try-it').scrollIntoView({behavior:'instant'})");
await mouse(170,210);await mouse(350,245);await wait(50);
fs.writeFileSync('/tmp/stable-mouse-demo-desktop.png',Buffer.from((await send('Page.captureScreenshot',{format:'png'})).data,'base64'));
assert.deepEqual(errors,[]);
console.log('PASS: smoothing/settling, click reset, re-entry, presets, center toggle, pause, reset, Escape, keyboard slider, responsive layout; no JS exceptions.');
ws.close();
