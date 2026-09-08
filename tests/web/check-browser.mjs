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
if(process.argv[2]){errors.length=0;await send('Page.navigate',{url:process.argv[2]});await wait(2000)}
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
const clicked=await state();assert.equal(clicked.mark,true);await wait(1100);s=await state();assert.ok(Math.abs(s.x-430)<=1 && Math.abs(s.y-200)<=1, `Click lost movement: ${JSON.stringify(s)}`);
// Hold the button while moving, then release. Neither may discard displacement.
await mouse(470,210,'mousePressed');
await send('Input.dispatchMouseEvent',{type:'mouseMoved',x:rect.x+540,y:rect.y+260,button:'left',buttons:1});
await mouse(540,260,'mouseReleased');await wait(1100);s=await state();
assert.ok(Math.abs(s.x-540)<=1 && Math.abs(s.y-260)<=1, `Drag lost movement: ${JSON.stringify(s)}`);
await mouse(-10,-10);assert.equal((await state()).hidden,true);await mouse(110,90);s=await state();assert.equal(s.x,110);assert.equal(s.y,90);
await evaluate("document.querySelector('[data-strength=\"85\"]').click();document.querySelector('#demo-center-method').value='detected';document.querySelector('#demo-center-method').dispatchEvent(new Event('change'))");
assert.equal(await evaluate("document.querySelector('#demo-strength-value').textContent"),'85%');assert.equal(await evaluate("document.querySelector('#demo-center-method').value"),'detected');
await evaluate("document.querySelector('#demo-toggle').click()");await mouse(140,160);await mouse(230,250);s=await state();assert.equal(s.x,230);assert.equal(s.y,250);
// Reset and keyboard pause.
await evaluate("document.querySelector('#demo-reset').click()");assert.equal((await state()).mark,false);
await evaluate("document.querySelector('#demo-toggle').click();document.querySelector('.demo-area').focus()");
await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});
await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});
assert.equal(await evaluate("document.querySelector('#demo-toggle').getAttribute('aria-pressed')"),'false');
// Demo speed is fixed; the smoothing slider still supports keyboard operation.
assert.equal(await evaluate("document.querySelector('#demo-speed')"),null);
assert.equal(await evaluate("document.querySelector('.demo-more').open"),false);
await evaluate("document.querySelector('.demo-more summary').focus()");
await send('Input.dispatchKeyEvent',{type:'keyDown',text:'\r',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
assert.equal(await evaluate("document.querySelector('.demo-more').open"),true);
await evaluate("document.querySelector('#demo-strength').focus()");
await send('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39});
await send('Input.dispatchKeyEvent',{type:'keyUp',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39});
assert.equal(await evaluate("document.querySelector('#demo-strength-value').textContent"),'86%');
for(const width of [1440,390,320]){
 await send('Emulation.setDeviceMetricsOverride',{width,height:1000,deviceScaleFactor:1,mobile:false});
 const check=await evaluate("({width:innerWidth,overflow:document.documentElement.scrollWidth>innerWidth,minButton:Math.min(...[...document.querySelectorAll('.demo-controls button')].map(a=>a.getBoundingClientRect().height))})");
 assert.equal(check.overflow,false);assert.ok(check.minButton>=48);console.log(check);
}
await send('Emulation.setDeviceMetricsOverride',{width:1440,height:1000,deviceScaleFactor:1,mobile:false});
await evaluate("document.querySelector('#demo-toggle').click();document.querySelector('#try-it').scrollIntoView({behavior:'instant'})");
await mouse(170,210);await mouse(350,245);await wait(50);
fs.writeFileSync('/tmp/stable-mouse-demo-desktop.png',Buffer.from((await send('Page.captureScreenshot',{format:'png'})).data,'base64'));
// Feed identical samples through the actual DOM handlers and timer callback.
// A controlled clock makes recognition tests independent of CI scheduling.
const {identifier: clockScript} = await send('Page.addScriptToEvaluateOnNewDocument', {source: `
  window.demoTestClock = {now: 0, tick: null};
  performance.now = () => window.demoTestClock.now;
  const originalInterval = window.setInterval, originalClear = window.clearInterval;
  window.setInterval = (fn, ms, ...args) => {
    if (ms === 8) { window.demoTestClock.tick = () => fn(...args); return -1; }
    return originalInterval(fn, ms, ...args);
  };
  window.clearInterval = id => {
    if (id === -1) window.demoTestClock.tick = null;
    else originalClear(id);
  };
`});
await send('Page.reload', {ignoreCache: true});await wait(1500);
await send('Page.removeScriptToEvaluateOnNewDocument', {identifier: clockScript});
const comparison = await evaluate(`(() => {
  const area = document.querySelector('.demo-area');
  const center = document.querySelector('#demo-center-method');
  const label = document.querySelector('#demo-center-status');
  const rect = area.getBoundingClientRect();
  const pointer = (type, x) => area.dispatchEvent(new PointerEvent(type, {
    pointerType: 'mouse', clientX: rect.left + 200 + x, clientY: rect.top + 100,
    bubbles: true, button: 0, buttons: type === 'pointerup' ? 0 : 1
  }));
  document.querySelector('[data-strength="85"]').click();
  const results = [];
  for (const pattern of ['regular', 'uneven']) for (const checked of [false, true]) {
    pointer('pointerleave', 0);
    center.value = checked ? 'detected' : 'off'; center.dispatchEvent(new Event('change'));
    pointer('pointerenter', 0);
    const positions = []; let recognized = false;
    let seed = 937, time = 0, duration = .125, from = 0, to = 65;
    const random = () => ((seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0) / 2 ** 32);
    for (let i = 0; i < 875; i++) {
      // Five seconds of 4Hz shaking, then two seconds motionless.
      const t = i * .008;
      while (t >= time + duration) {
        time += duration; from = to;
        to = -Math.sign(to) * 65 * (1 + .35 * (2 * random() - 1));
        duration = .125 * (1 + .35 * (2 * random() - 1));
      }
      const wave = pattern === 'regular' ? 65 * Math.sin(t * 2 * Math.PI * 4)
        : Math.round(from + (to - from) * (.5 - .5 * Math.cos(Math.PI * (t - time) / duration)));
      const x = i < 625 ? wave : 0;
      pointer('pointermove', x);
      // A held click must not break recognition or lose movement.
      if (i === 400) pointer('pointerdown', x);
      if (i === 600) pointer('pointerup', x);
      window.demoTestClock.now += 8; window.demoTestClock.tick();
      const position = new DOMMatrix(getComputedStyle(document.querySelector('.demo-cursor')).transform).m41;
      if (i >= 375 && i < 625) positions.push(position);
      recognized ||= label.textContent === 'Tracking the horizontal center.';
    }
    const mean = positions.reduce((a, b) => a + b) / positions.length;
    const rms = Math.sqrt(positions.reduce((a, b) => a + (b - mean) ** 2, 0) / positions.length);
    results.push({pattern, checked, rms, recognized, finalStatus: label.textContent,
      finalX: new DOMMatrix(getComputedStyle(document.querySelector('.demo-cursor')).transform).m41});
  }
  return results;
})()`);
assert.ok(comparison[0].rms > 1, JSON.stringify(comparison));
assert.ok(comparison[1].rms < comparison[0].rms * .4, JSON.stringify(comparison));
assert.ok(comparison[3].rms < comparison[2].rms * .9, JSON.stringify(comparison));
assert.equal(comparison[3].recognized, true);
assert.equal(comparison[0].recognized, false);
assert.equal(comparison[1].recognized, true);
assert.equal(comparison[1].finalStatus, 'Waiting for repeated reversals. Using ordinary smoothing.');
for (const result of comparison) assert.ok(Math.abs(result.finalX - 200) <= 1, JSON.stringify(result));
console.log('Regular/uneven-shake comparison and release to ordinary smoothing:', comparison);
// The experiment runs immediately, without reversal recognition, and keeps
// clicks/drags continuous. Exercise its controls and actual rendered cursor.
const experiment = await evaluate(`(() => {
  const area = document.querySelector('.demo-area');
  const method = document.querySelector('#demo-center-method');
  const windowInput = document.querySelector('#demo-center-window');
  document.querySelector('.demo-more').open=true;
  const rect = area.getBoundingClientRect();
  const pointer = (type, x, y) => area.dispatchEvent(new PointerEvent(type, {
    pointerType:'mouse', clientX:rect.left+x, clientY:rect.top+y, bubbles:true,
    button:0, buttons:type==='pointerup'?0:1
  }));
  method.value='always'; method.dispatchEvent(new Event('change'));
  pointer('pointerenter',80,80);
  const immediateStatus=document.querySelector('#demo-center-status').textContent;
  pointer('pointermove',160,120); pointer('pointerdown',160,120);
  pointer('pointermove',240,160); pointer('pointerup',240,160);
  for(let i=0;i<500;i++){window.demoTestClock.now+=8;window.demoTestClock.tick();}
  const transform=new DOMMatrix(getComputedStyle(document.querySelector('.demo-cursor')).transform);
  const enabled=document.querySelector('#demo-center-method').value==='always';
  const visible=!document.querySelector('.demo-window-control').hidden;
  windowInput.value='500';windowInput.dispatchEvent(new Event('input'));
  const label=document.querySelector('#demo-center-window-value').textContent;
  const reset=document.querySelector('.demo-cursor').hasAttribute('hidden');
  document.querySelector('#demo-toggle').click();
  pointer('pointerenter',90,90);pointer('pointermove',200,170);
  const paused=new DOMMatrix(getComputedStyle(document.querySelector('.demo-cursor')).transform);
  document.querySelector('#demo-toggle').click();
  return {immediateStatus,x:transform.m41,y:transform.m42,enabled,visible,label,reset,
    pausedX:paused.m41,pausedY:paused.m42};
})()`);
assert.equal(experiment.immediateStatus,'Always estimating the center, then smoothing.');
assert.ok(Math.abs(experiment.x-240)<=1 && Math.abs(experiment.y-160)<=1, JSON.stringify(experiment));
assert.ok(experiment.enabled && experiment.visible && experiment.reset);
assert.equal(experiment.label,'500 ms');
assert.equal(experiment.pausedX,200);assert.equal(experiment.pausedY,170);
for(const width of [1440,390,320]){
  await send('Emulation.setDeviceMetricsOverride',{width,height:1000,deviceScaleFactor:1,mobile:false});
  assert.equal(await evaluate('document.documentElement.scrollWidth>innerWidth'),false);
}
await send('Emulation.setDeviceMetricsOverride',{width:1440,height:1200,deviceScaleFactor:1,mobile:false});
await evaluate("document.querySelector('#try-it').scrollIntoView({behavior:'instant'})");
fs.writeFileSync('/tmp/stable-mouse-always-center.png',Buffer.from((await send('Page.captureScreenshot',{format:'png'})).data,'base64'));
console.log('Always-center controls, click/drag, pause, reset and responsive layout:',experiment);
assert.deepEqual(errors,[]);
console.log('PASS: smoothing/settling, click/drag alignment, re-entry, presets, center toggle, pause, reset, Escape, keyboard slider, responsive layout; no JS exceptions.');
ws.close();
