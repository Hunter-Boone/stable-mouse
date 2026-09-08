// SPDX-License-Identifier: GPL-3.0-only
// Browser experiment: continuously estimate the midpoint of the recent input
// range, then pass movement of that midpoint through the ordinary app smoother.
// There is no reversal detector, confidence threshold, or lock/unlock transition.
import { Stabilizer } from './filter.mjs';

export class AlwaysCenter {
  constructor() {
    this.smoother = new Stabilizer();
    this.windowSeconds = .25;
    this.speed = 1;
    this.reset();
  }
  configure(c) {
    const windowSeconds = Number.isFinite(c.windowSeconds) ? Math.max(.1, Math.min(.6, c.windowSeconds)) : .25;
    const oldStrength = this.smoother.config.strength;
    this.smoother.configure({ strength: c.strength, speed: 1, centerTracking: false });
    if (windowSeconds !== this.windowSeconds || (this.smoother.config.strength === 0) !== (oldStrength === 0)) this.reset();
    this.windowSeconds = windowSeconds;
    this.speed = Number.isFinite(c.speed) ? Math.max(.25, Math.min(2, c.speed)) : 1;
  }
  reset() {
    this.smoother.reset();
    this.time = 0;
    this.pending = { x: 0, y: 0 };
    this.raw = { x: 0, y: 0 };
    this.previousCenter = { x: 0, y: 0 };
    this.fraction = { x: 0, y: 0 };
    this.samples = [{ time: 0, x: 0, y: 0 }];
  }
  add(x, y) {
    if (!Number.isFinite(x) || !Number.isFinite(y)) return;
    this.pending.x += x * this.speed;
    this.pending.y += y * this.speed;
  }
  step(seconds) {
    if (!Number.isFinite(seconds) || seconds <= 0) return { x: 0, y: 0 };
    if (this.smoother.config.strength === 0) {
      const out = this.pending; this.pending = { x: 0, y: 0 }; return out;
    }
    const dt = Math.min(seconds, .1);
    this.time += dt;
    this.raw.x += this.pending.x; this.raw.y += this.pending.y;
    this.pending = { x: 0, y: 0 };
    // Sample even when stationary: old extrema must expire so a clean reach or
    // tiny correction can settle fully instead of remaining at its midpoint.
    this.samples.push({ time: this.time, ...this.raw });
    while (this.samples.length > 1 && this.samples[1].time <= this.time - this.windowSeconds) this.samples.shift();
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const sample of this.samples) {
      minX = Math.min(minX, sample.x); maxX = Math.max(maxX, sample.x);
      minY = Math.min(minY, sample.y); maxY = Math.max(maxY, sample.y);
    }
    const center = { x: (minX + maxX) / 2, y: (minY + maxY) / 2 };
    this.smoother.add(center.x - this.previousCenter.x, center.y - this.previousCenter.y);
    this.previousCenter = center;
    return this.smoother.step(dt);
  }
  pixels(seconds) {
    const m = this.step(seconds); this.fraction.x += m.x; this.fraction.y += m.y;
    const out = { x: Math.trunc(this.fraction.x), y: Math.trunc(this.fraction.y) };
    this.fraction.x -= out.x; this.fraction.y -= out.y; return out;
  }
}
