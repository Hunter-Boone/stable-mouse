// SPDX-License-Identifier: GPL-3.0-only
// Browser port of src/filter.h and src/center_tracker.h. Keep the arithmetic and
// state transitions aligned; tests/web/check-parity.mjs checks the C++ output.
const clamp = (v, low, high) => Math.min(high, Math.max(low, v));
class CenterTracker {
  constructor() {
    for (const key of ['time', 'raw', 'stage', 'fallback', 'peak', 'previousPeak',
      'priorCenter', 'priorCenterStep', 'lastTurn', 'previousInterval', 'previousSpan',
      'center', 'centerSmooth', 'blend', 'direction', 'turns', 'consistent']) this[key] = 0;
    this.locked = false;
  }
  step(delta, dt, tau) {
    this.time += dt; this.raw += delta;
    const h = dt / tau, decay = Math.exp(-h), alpha = -Math.expm1(-h);
    const pending = this.raw - this.stage, second = this.stage - this.fallback;
    this.fallback += second * alpha + pending * (alpha - h * decay);
    this.stage += pending * alpha;
    if (!this.direction && Math.abs(this.raw - this.peak) >= 1) this.direction = this.raw > this.peak ? 1 : -1;
    if ((this.direction > 0 && this.raw > this.peak) || (this.direction < 0 && this.raw < this.peak)) this.peak = this.raw;
    if (this.direction && (this.raw - this.peak) * this.direction <= -Math.max(4, Math.min(12, this.previousSpan * .06))) {
      const interval = this.time - this.lastTurn, span = Math.abs(this.peak - this.previousPeak);
      const candidate = (this.peak + this.previousPeak) / 2, centerStep = candidate - this.priorCenter;
      const plausible = this.turns >= 3 && interval >= .03 && interval <= .6
        && interval / this.previousInterval >= .4 && interval / this.previousInterval <= 2.5
        && span >= 2 && span / this.previousSpan >= .35 && span / this.previousSpan <= 2.8
        && Math.abs(centerStep - this.priorCenterStep) <= Math.max(4, span * .3);
      const wasLocked = this.locked;
      this.consistent = plausible ? Math.min(this.consistent + 1, 5) : Math.max(0, this.consistent - 2);
      this.locked = this.consistent >= (wasLocked ? 2 : 3);
      if (this.locked && plausible) this.center = wasLocked ? this.center + .3 * (candidate - this.center) : candidate;
      this.priorCenter = candidate; this.priorCenterStep = centerStep;
      this.previousPeak = this.peak; this.previousInterval = interval; this.previousSpan = span;
      this.lastTurn = this.time; this.turns = Math.min(this.turns + 1, 4);
      this.direction = -this.direction; this.peak = this.raw;
    }
    if (this.locked && this.time - this.lastTurn > Math.min(.7, this.previousInterval * 2.5)) {
      this.locked = false; this.consistent = 0;
    }
    this.blend += -Math.expm1(-dt / .08) * ((this.locked ? 1 : 0) - this.blend);
    this.centerSmooth += -Math.expm1(-dt / .08) * ((this.locked ? this.center : this.fallback) - this.centerSmooth);
    const output = this.fallback * (1 - this.blend) + this.centerSmooth * this.blend;
    for (const key of ['raw', 'stage', 'fallback', 'peak', 'previousPeak', 'priorCenter', 'center', 'centerSmooth']) this[key] -= output;
    return output;
  }
}
export class Stabilizer {
  constructor() { this.config = { strength: 55, speed: 1, centerTracking: false }; this.reset(); }
  configure(c) {
    if (c.centerTracking !== this.config.centerTracking || (this.config.centerTracking && (c.strength === 0) !== (this.config.strength === 0))) this.reset();
    this.config = {
      strength: Number.isFinite(c.strength) ? clamp(c.strength, 0, 100) : 55,
      speed: Number.isFinite(c.speed) ? clamp(c.speed, .25, 2) : 1,
      centerTracking: c.centerTracking,
    };
  }
  add(x, y) {
    if (!Number.isFinite(x) || !Number.isFinite(y)) return;
    this.pending.x += x * this.config.speed; this.pending.y += y * this.config.speed;
  }
  step(seconds) {
    if (!Number.isFinite(seconds) || seconds <= 0) return { x: 0, y: 0 };
    if (this.config.strength === 0) {
      const out = { x: this.pending.x + this.second.x, y: this.pending.y + this.second.y };
      this.pending = { x: 0, y: 0 }; this.second = { x: 0, y: 0 }; return out;
    }
    const tau = .012 + .000022 * this.config.strength * this.config.strength;
    if (this.config.centerTracking) {
      const out = { x: this.centerX.step(this.pending.x, Math.min(seconds, .1), tau), y: this.centerY.step(this.pending.y, Math.min(seconds, .1), tau) };
      this.pending = { x: 0, y: 0 }; return out;
    }
    const h = Math.min(seconds, .1) / tau, decay = Math.exp(-h), alpha = -Math.expm1(-h);
    const out = { x: this.second.x * alpha + this.pending.x * (alpha - h * decay), y: this.second.y * alpha + this.pending.y * (alpha - h * decay) };
    this.second = { x: (this.second.x + this.pending.x * h) * decay, y: (this.second.y + this.pending.y * h) * decay };
    this.pending.x *= decay; this.pending.y *= decay; return out;
  }
  reset() {
    this.pending = { x: 0, y: 0 }; this.second = { x: 0, y: 0 }; this.fraction = { x: 0, y: 0 };
    this.centerX = new CenterTracker(); this.centerY = new CenterTracker();
  }
  pixels(seconds) {
    const m = this.step(seconds); this.fraction.x += m.x; this.fraction.y += m.y;
    const out = { x: Math.trunc(this.fraction.x), y: Math.trunc(this.fraction.y) };
    this.fraction.x -= out.x; this.fraction.y -= out.y; return out;
  }
}
