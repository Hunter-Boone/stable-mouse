// SPDX-License-Identifier: GPL-3.0-only
// Keep startup outside the demo's import graph so failed downloads or startup
// exceptions produce a useful message instead of looking like disabled JS.
import('./demo.mjs').catch(error => {
  const message = document.querySelector('.demo-loading');
  message.hidden = false;
  message.textContent = 'The cursor demo could not start. ';
  const retry = document.createElement('a');
  const url = new URL(location.href);
  url.searchParams.set('reload', Date.now());
  url.hash = 'try-it';
  retry.href = url.href;
  retry.textContent = 'Reload the demo';
  message.append(retry);
  document.querySelector('.demo-controls').disabled = true;
  document.querySelector('.demo-area').inert = true;
  console.error('Cursor demo startup failed:', error);
});
