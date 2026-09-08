// SPDX-License-Identifier: GPL-3.0-only
// Content-address every module, including transitive imports, so cached scripts
// cannot initialize a newer page. No external build dependencies are required.
import { createHash } from 'node:crypto';
import { readFile, writeFile, mkdir, copyFile } from 'node:fs/promises';
import { dirname, resolve, basename, extname } from 'node:path';
import { fileURLToPath } from 'node:url';
const source = dirname(fileURLToPath(import.meta.url));
if (!process.argv[2]) throw new Error('Usage: node website/build.mjs OUTPUT_DIRECTORY');
const output = resolve(process.argv[2]);
if (output === source || output === resolve(source, '..')) throw new Error('Use a separate output directory');
await mkdir(output, { recursive: true });
const built = new Map(), visiting = new Set();
async function asset(name) {
  if (built.has(name)) return built.get(name);
  if (visiting.has(name)) throw new Error(`Cyclic module import: ${name}`);
  if (basename(name) !== name) throw new Error(`Expected a flat asset path: ${name}`);
  visiting.add(name);
  let text = await readFile(resolve(source, name), 'utf8');
  if (name.endsWith('.mjs')) {
    for (const match of [...text.matchAll(/(['"])(\.\/[^'"\s]+\.mjs)\1/g)]) {
      const dependency = await asset(match[2].slice(2));
      text = text.replaceAll(match[0], `${match[1]}./${dependency}${match[1]}`);
    }
  }
  const hash = createHash('sha256').update(text).digest('hex').slice(0, 16);
  const extension = extname(name);
  const filename = `${basename(name, extension)}.${hash}${extension}`;
  await writeFile(resolve(output, filename), text);
  built.set(name, filename); visiting.delete(name);
  return filename;
}
let html = await readFile(resolve(source, 'index.html'), 'utf8');
html = html.replace('src="boot.mjs"', `src="${await asset('boot.mjs')}"`);
html = html.replace('href="style.css"', `href="${await asset('style.css')}"`);
await writeFile(resolve(output, 'index.html'), html);
await copyFile(resolve(source, 'icon.svg'), resolve(output, 'icon.svg'));
console.log(`Built website in ${output}`);
