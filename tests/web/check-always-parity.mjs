// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {AlwaysCenter} from '../../website/always-center.mjs';
const ops=[], expected=[];
const f=new AlwaysCenter();
function config(strength,speed,windowSeconds){ops.push(`always ${strength} ${speed} ${windowSeconds}`);f.configure({strength,speed,windowSeconds});}
function reset(){ops.push('reset');f.reset();}
function add(x,y){ops.push(`add ${x} ${y}`);f.add(x,y);}
function step(dt,mode){ops.push(`${mode} ${dt}`);expected.push({mode,...f[mode](dt)});}
for(const mode of ['step','pixels']) for(const rate of [60,125,1000]) for(const window of [.1,.25,.6]) {
  for(const strength of [0,55,85]) for(const speed of [.25,1,2]){
    reset();config(strength,speed,window);let previous={x:0,y:0};
    for(let i=0;i<rate*4;i++){
      const t=i/rate, envelope=.7+.3*Math.sin(t*2.1);
      const x=t<2?envelope*65*Math.sin(t*25.4):160;
      const y=t<2?envelope*23*Math.sin(t*33.8):-12;
      add(x-previous.x,y-previous.y);previous={x,y};step(1/rate,mode);
    }
    // Settings changes, resets and bypass must not restore stale centers.
    config(0,speed,window);step(.008,mode);
    config(55,speed,.25);add(12,-6);step(.008,mode);
    reset();step(.008,mode);
  }
}
const native=spawnSync(process.argv[2],[],{input:ops.join('\n')+'\n',encoding:'utf8',maxBuffer:64*1024*1024});
assert.equal(native.status,0,native.stderr || String(native.error));
const lines=native.stdout.trim().split('\n');assert.equal(lines.length,expected.length);
let largest=0;
for(let i=0;i<lines.length;i++){
  const [x,y]=lines[i].split(' ').map(Number), e=expected[i];
  const error=Math.max(Math.abs(x-e.x),Math.abs(y-e.y));largest=Math.max(largest,error);
  assert.ok(error<=(e.mode==='pixels'?0:1e-8),`sample ${i}: ${lines[i]} vs ${JSON.stringify(e)}`);
}
console.log(`PASS: ${lines.length} native/browser always-center pairs; largest difference ${largest}`);
