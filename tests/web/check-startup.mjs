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

assert.ok(process.argv[2], 'Pass the built website URL');
const base = process.argv[2];
let staleRequests = 0;
const scriptUrls = [];
await send('Network.enable');
await send('Network.setCacheDisabled', {cacheDisabled:true});
ws.addEventListener('message', async e => {
  const m=JSON.parse(e.data);
  if(m.method==='Network.requestWillBeSent' && m.params.type==='Script') scriptUrls.push(m.params.request.url);
  if(m.method==='Fetch.requestPaused') {
    if(new URL(m.params.request.url).pathname.endsWith('/demo.mjs')) {
      staleRequests++;
      // Reproduce the previous script's access to the removed speed control.
      await send('Fetch.fulfillRequest',{requestId:m.params.requestId,responseCode:200,
        responseHeaders:[{name:'Content-Type',value:'text/javascript'}],
        body:Buffer.from("document.querySelector('#demo-speed').addEventListener('input',()=>{});").toString('base64')});
    } else await send('Fetch.failRequest',{requestId:m.params.requestId,errorReason:'Failed'});
  }
});
await send('Fetch.enable',{patterns:[{urlPattern:'*/demo.mjs'}]});
await send('Page.navigate',{url:base});await wait(1500);
assert.equal(await evaluate("document.querySelector('.demo-controls').disabled"),false);
assert.equal(staleRequests,0,'production must not request a cached unversioned entry');
assert.ok(scriptUrls.length>=4,JSON.stringify(scriptUrls));
assert.ok(scriptUrls.every(url=>/\.[a-f0-9]{16}\.mjs$/.test(new URL(url).pathname)),JSON.stringify(scriptUrls));
// A failed transitive module should produce a recoverable error, not suggest
// that JavaScript is disabled. Then follow the exact recovery link.
await send('Fetch.enable',{patterns:[{urlPattern:'*/always-center.*.mjs'}]});
await send('Page.navigate',{url:base+'?startup-test=blocked'});await wait(1500);
assert.equal(await evaluate("document.querySelector('.demo-controls').disabled"),true);
assert.match(await evaluate("document.querySelector('.demo-loading').textContent"),/could not start/);
const recovery=await evaluate("document.querySelector('.demo-loading a').href");
assert.ok(new URL(recovery).searchParams.has('reload'));
await send('Fetch.disable');
await send('Page.navigate',{url:recovery});await wait(1500);
assert.equal(await evaluate("document.querySelector('.demo-controls').disabled"),false);
assert.equal(await evaluate("document.querySelector('.demo-loading').hidden"),true);
await send('Network.setCacheDisabled',{cacheDisabled:false});
console.log('PASS: versioned module graph bypasses stale script; dependency failure has a working reload link');
ws.close();
