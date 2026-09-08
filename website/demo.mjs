// SPDX-License-Identifier: GPL-3.0-only
import { Stabilizer } from './filter.mjs';
import { AlwaysCenter } from './always-center.mjs';
const section = document.querySelector('#try-it');
const area = section.querySelector('.demo-area');
const ghost = section.querySelector('.demo-cursor');
const mark = section.querySelector('.demo-click');
const strength = section.querySelector('#demo-strength');
const speed = section.querySelector('#demo-speed');
const center = section.querySelector('#demo-center');
const method = section.querySelector('#demo-center-method');
const centerWindow = section.querySelector('#demo-center-window');
const centerHelp = section.querySelector('#demo-center-help');
const toggle = section.querySelector('#demo-toggle');
const status = section.querySelector('#demo-status');
const centerStatus = section.querySelector('#demo-center-status');
const hint = section.querySelector('.demo-hint');
const targets = [...area.querySelectorAll('.demo-target')];
let filter = new Stabilizer(), mode = 'off';
let enabled = true, active = false, previous = null, output = null, timer = null, lastTime = 0;
let width = 0, height = 0;
const clamp = (v, max) => Math.max(0, Math.min(max, v));
function showCenterStatus() {
  const axes = mode === 'always' ? [] : [filter.centerX.locked && 'horizontal', filter.centerY.locked && 'vertical'].filter(Boolean);
  const message = !center.checked ? 'Center tracking is off.'
    : !enabled ? 'Center tracking is paused.'
    : Number(strength.value) === 0 ? 'Increase smoothing strength to use center tracking.'
    : mode === 'always' ? 'Always estimating the center, then smoothing.'
    : axes.length ? `Tracking the ${axes.join(' and ')} center.`
    : 'Waiting for repeated reversals. Using ordinary smoothing.';
  if (centerStatus.textContent !== message) centerStatus.textContent = message;
}
function paint() {
  if (!output) return;
  ghost.style.transform = `translate(${output.x}px, ${output.y}px)`;
  for (const target of targets) {
    const x = target.offsetLeft + target.offsetWidth / 2, y = target.offsetTop + target.offsetHeight / 2;
    target.classList.toggle('is-hit', Math.hypot(output.x - x, output.y - y) <= target.offsetWidth / 2);
  }
}
function tick() {
  if (!active || !output) return;
  const now = performance.now();
  // A background tab or long stall starts a fresh comparison instead of replaying
  // queued motion when the user returns. Ordinary ticks follow the app's clock.
  if (now - lastTime > 250) { leave(); return; }
  if (enabled) {
    const delta = filter.pixels((now - lastTime) / 1000);
    output.x = clamp(output.x + delta.x, width - 1); output.y = clamp(output.y + delta.y, height - 1);
  }
  lastTime = now; paint(); showCenterStatus();
}
function leave() {
  active = false; previous = null; output = null; filter.reset();
  clearInterval(timer); timer = null; ghost.setAttribute('hidden', ''); hint.hidden = false;
  targets.forEach(target => target.classList.remove('is-hit'));
  showCenterStatus();
}
function point(event) {
  const rect = area.getBoundingClientRect();
  return { x: clamp(event.clientX - rect.left, width - 1), y: clamp(event.clientY - rect.top, height - 1) };
}
function enter(event) {
  // Touch scrolling remains ordinary page scrolling. A mouse, trackpad, or pen
  // gives a meaningful comparison with the visible system pointer.
  if (event.pointerType === 'touch') return;
  leave(); width = area.clientWidth; height = area.clientHeight;
  previous = point(event); output = { ...previous }; active = true;
  ghost.removeAttribute('hidden'); hint.hidden = true; lastTime = performance.now();
  timer = setInterval(tick, 8); paint();
}
function move(event) {
  if (event.pointerType === 'touch') return;
  if (!active) enter(event);
  if (!active) return;
  const samples = event.getCoalescedEvents?.();
  for (const sample of samples?.length ? samples : [event]) {
    const next = point(sample);
    if (enabled) filter.add(next.x - previous.x, next.y - previous.y);
    else output = { ...next };
    previous = next;
  }
  if (!enabled) paint();
}
function configure() {
  const nextMode = center.checked ? method.value : 'off';
  if (nextMode !== mode) {
    leave();
    filter = nextMode === 'always' ? new AlwaysCenter() : new Stabilizer();
    mode = nextMode;
  }
  filter.configure({ strength: Number(strength.value), speed: Number(speed.value) / 100, centerTracking: center.checked,
    windowSeconds: Number(centerWindow.value) / 1000 });
  section.querySelector('#demo-strength-value').textContent = `${strength.value}%`;
  section.querySelector('#demo-speed-value').textContent = `${speed.value}%`;
  status.textContent = enabled ? 'Demo smoothing is on' : 'Paused · Both cursors move together';
  toggle.textContent = enabled ? 'Pause smoothing' : 'Enable smoothing';
  toggle.setAttribute('aria-pressed', String(enabled));
  section.querySelector('.demo-window-control').hidden = method.value !== 'always';
  section.querySelector('#demo-center-window-value').textContent = `${centerWindow.value} ms`;
  centerHelp.textContent = method.value === 'always'
    ? 'Browser experiment: take the midpoint of the recent movement range, then smooth it. A longer center window can steady the pointer more, but delays deliberate movement too.'
    : 'More smoothing adds delay. Center tracking allows uneven back-and-forth movement, but needs several reversals to engage. Very small movements use ordinary smoothing.';
  showCenterStatus();
}
area.addEventListener('pointerenter', enter);
area.addEventListener('pointermove', move);
area.addEventListener('pointerleave', leave);
area.addEventListener('pointercancel', leave);
area.addEventListener('pointerdown', event => {
  if (!active || event.pointerType === 'touch') return;
  // Keep queued movement: discarding it permanently offsets a relative-motion
  // comparison from the real cursor. This demo marks clicks without applying
  // the desktop app's click-tail cancellation policy.
  mark.style.transform = `translate(${output.x}px, ${output.y}px)`; mark.hidden = false;
});
for (const control of [strength, speed, center]) control.addEventListener('input', configure);
method.addEventListener('change', () => { center.checked = true; configure(); });
centerWindow.addEventListener('input', () => { leave(); configure(); });
section.querySelectorAll('[data-strength]').forEach(button => button.addEventListener('click', () => {
  strength.value = button.dataset.strength; configure();
}));
toggle.addEventListener('click', () => { enabled = !enabled; leave(); configure(); });
section.querySelector('#demo-reset').addEventListener('click', () => { leave(); mark.hidden = true; });
section.addEventListener('keydown', event => {
  if (event.key === 'Escape') { enabled = false; leave(); configure(); }
});
window.addEventListener('blur', leave);
window.addEventListener('resize', leave);
document.addEventListener('visibilitychange', () => { if (document.hidden) leave(); });
configure();
section.querySelector('fieldset').disabled = false;
section.querySelector('.demo-loading').hidden = true;
