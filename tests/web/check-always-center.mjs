// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import { AlwaysCenter } from '../../website/always-center.mjs';
import { Stabilizer } from '../../website/filter.mjs';

// Prove the order: an immediate 20px step first becomes a 10px midpoint, then
// takes exactly the same path through the app's ordinary smoother.
const actual = new AlwaysCenter(), reference = new Stabilizer();
actual.configure({ strength:55, speed:1, windowSeconds:.25 });
reference.configure({ strength:55, speed:1, centerTracking:false });
actual.add(20, -10); reference.add(10, -5);
assert.deepEqual(actual.step(.008), reference.step(.008));
for (let i=0; i<20; i++) assert.deepEqual(actual.step(.008), reference.step(.008));

// No reversal is needed to finish any size of move. Check expiry, both axes,
// speed, fractions, timer rates, and the longest selectable center window.
for (const rate of [60,125,1000]) for (const windowSeconds of [.1,.25,.6]) {
  for (const speed of [.25,1,2]) for (const distance of [1,12,160]) {
    const f = new AlwaysCenter(); f.configure({ strength:85, speed, windowSeconds });
    f.add(distance,-distance/2); let output = {x:0,y:0};
    for (let i=0; i<rate*4; i++) { const d=f.step(1/rate); output.x+=d.x; output.y+=d.y; }
    assert.ok(Math.abs(output.x-distance*speed)<.001);
    assert.ok(Math.abs(output.y+distance*speed/2)<.001);
    assert.ok(f.samples.length<=Math.ceil(windowSeconds*rate)+2, 'old samples expire');
    f.reset(); assert.deepEqual(f.step(.008),{x:0,y:0});
  }
}
const bypass = new AlwaysCenter(); bypass.configure({strength:0,speed:.25});
let pixels=0; for (let i=0;i<40;i++){ bypass.add(1,0); pixels+=bypass.pixels(.008).x; }
assert.equal(pixels,10);
bypass.configure({strength:85,speed:1}); assert.deepEqual(bypass.step(.008),{x:0,y:0});
bypass.add(NaN,10); assert.deepEqual(bypass.step(.008),{x:0,y:0});
bypass.add(10,0); assert.deepEqual(bypass.step(0),{x:0,y:0});
assert.ok(bypass.step(.008).x>0);
bypass.configure({strength:0,speed:1}); assert.deepEqual(bypass.step(.008),{x:0,y:0});

// Uneven engineering input, not a model of a medical condition. Also measure
// a clean reach so extra delay is visible instead of hidden by a steadiness score.
const results=[];
for (const kind of ['uneven','clean reach']) for (const mode of ['ordinary','detected','always']) {
  const f=mode==='always'?new AlwaysCenter():new Stabilizer();
  f.configure({strength:55,speed:1,centerTracking:mode==='detected',windowSeconds:.25});
  let seed=937, time=0, duration=.125, from=0, to=65, previous=0, output=0, squared=0, count=0, settledAt=null;
  const random=()=>((seed=(Math.imul(seed,1664525)+1013904223)>>>0)/2**32);
  for(let i=0;i<1000;i++) {
    const t=i*.008;
    while(t>=time+duration){time+=duration;from=to;to=-Math.sign(to)*65*(1+.35*(2*random()-1));duration=.125*(1+.35*(2*random()-1));}
    const x=kind==='uneven' && t<6 ? Math.round(from+(to-from)*(.5-.5*Math.cos(Math.PI*(t-time)/duration)))
      : kind==='clean reach' ? 160*Math.min(1,t/.4) : 12;
    f.add(x-previous,0);previous=x;output+=f.step(.008).x;
    if(kind==='uneven' && t>=2 && t<6){squared+=output*output;count++;}
    if(kind==='clean reach' && t>=.4 && settledAt===null && Math.abs(output-160)<2)settledAt=t;
  }
  assert.ok(Math.abs(output-(kind==='uneven'?12:160))<.01);
  results.push({kind,mode,rms:count?Math.sqrt(squared/count):null,settledAt});
}
assert.ok(results[2].rms<results[0].rms*.85, JSON.stringify(results));
assert.ok(results[5].settledAt>results[3].settledAt, 'the midpoint window adds reach delay');
assert.ok(results[5].settledAt<1.5, 'a clean reach must finish');
console.log('Always-center experiment:',results);
console.log('PASS: center before smoothing, expiry, travel, small corrections, reset, bypass, and uneven input');
