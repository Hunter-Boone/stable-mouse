// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { Stabilizer } from '../../website/filter.mjs';

assert.ok(process.argv[2], 'Pass the compiled filter_reference executable');
const operations = [];
const op = (...args) => operations.push(args);
let seed = 73521;
const random = () => ((seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0) / 2 ** 32 - .5);
for (const mode of ['step', 'pixels']) {
  for (const center of [0, 1]) {
    for (const strength of [0, 25, 55, 85, 100]) {
      for (const speed of [.25, 1, 2]) {
        for (const hz of [60, 125, 1000]) {
          op('reset'); op('config', strength, speed, center);
          let previousX = 0, previousY = 0;
          for (let i = 0; i < hz * 4; i++) {
            const t = i / hz;
            // Regular reversals, then irregular movement, then a stationary hold.
            const x = t < 2 ? 600 * Math.sin(2 * Math.PI * 4 * t) + t * 37
              : t < 3 ? previousX + random() * 83 : previousX;
            const y = t < 2 ? 43 * Math.sin(2 * Math.PI * 6 * t)
              : t < 3 ? previousY + random() * 29 : previousY;
            op('add', x - previousX, y - previousY);
            op(mode, 1 / hz);
            previousX = x; previousY = y;
          }
          // Mid-motion configuration changes, click/pause reset, and clock gaps.
          op('add', 137.375, -63.125); op(mode, .008);
          op('config', 0, .5, center); op(mode, .008);
          op('config', 85, 1, 1 - center); op('add', -20, 30);
          op(mode, 0); op(mode, -.1); op(mode, .7);
          op('reset'); op(mode, .008);
        }
      }
    }
  }
}
// Unequal half-cycles must exercise recognition too, not only the fallback.
for (const mode of ['step', 'pixels']) for (const hz of [60, 125, 1000]) {
  op('reset'); op('config', 85, 1, 1);
  let time=0, duration=.125, from=0, to=65, previous=0;
  for (let i=0; i<hz*8; i++) {
    const t=i/hz;
    while (t>=time+duration) {
      time+=duration; from=to;
      to=-Math.sign(to)*65*(1+.7*random());
      duration=.125*(1+.7*random());
    }
    const x=t<6 ? Math.round(from+(to-from)*(.5-.5*Math.cos(Math.PI*(t-time)/duration))) : 0;
    op('add', x-previous, 0); previous=x; op(mode, 1/hz);
  }
}
const result = spawnSync(process.argv[2], [], {
  input: operations.map(args => args.join(' ')).join('\n') + '\n',
  encoding: 'utf8', maxBuffer: 64 * 1024 * 1024,
});
assert.equal(result.status, 0, result.stderr || String(result.error));
const expected = result.stdout.trim().split('\n');
const filter = new Stabilizer();
let index = 0, worst = 0;
for (const [name, a, b, c] of operations) {
  if (name === 'config') filter.configure({ strength: a, speed: b, centerTracking: Boolean(c) });
  else if (name === 'add') filter.add(a, b);
  else if (name === 'reset') filter.reset();
  else {
    const actual = filter[name](a), reference = expected[index++].split(' ').map(Number);
    for (const [axis, value] of [['x', reference[0]], ['y', reference[1]]]) {
      const error = Math.abs(actual[axis] - value);
      worst = Math.max(worst, error);
      assert.ok(error <= (name === 'pixels' ? 0 : 1e-8),
        `${name} sample ${index}, ${axis}: JS ${actual[axis]}, C++ ${value}`);
    }
  }
}
assert.equal(index, expected.length);
console.log(`PASS: ${index} C++/browser output pairs; largest difference ${worst}`);
