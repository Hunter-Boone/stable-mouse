#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render generated engineering traces as a standalone, offline HTML replay."""
import argparse
import csv
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('trace', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--label', required=True, help='Identify portable simulation or Windows replay')
args = parser.parse_args()
groups = {}
with args.trace.open(newline='') as source:
    for row in csv.DictReader(source):
        key = row['scenario'] + ' | strength ' + row['strength']
        groups.setdefault(key, []).append([float(row[k]) for k in
            ('t', 'raw_x', 'raw_y', 'intended_x', 'intended_y', 'output_x', 'output_y')])
if not groups:
    raise SystemExit('No trace samples')
data = json.dumps({'label': args.label, 'groups': groups}).replace('<', '\\u003c')
page = '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stable Mouse motion replay</title>
<style>body{font:17px system-ui;margin:32px auto;max-width:1100px;padding:0 20px;color:#172235;background:#f6f8fb}select,button,input{font:inherit;padding:8px;max-width:100%}canvas{width:100%;background:white;border:1px solid #ccd3dd;margin:12px 0}label{display:block;margin:12px 0}#metrics{padding:16px;background:#fff;line-height:1.7}p{line-height:1.5}.raw{color:#b42318}.out{color:#175cd3}</style>
<h1>Stable Mouse motion replay</h1><p id="label"></p>
<p>Generated stress tests, not recordings of hand tremor. Speed 100%. Distances are input pixels, regardless of this plot's display scale.</p>
<label>Scenario and smoothing <select id="scenario"></select></label>
<button id="play">Pause</button> <label>Time <input id="time" type="range" min="0" max="6" step="0.008" value="0"> <output id="seconds">0.00 s</output></label>
<p><span class="raw">Red circle: raw input</span> · <span class="out">Blue cross: filtered output</span> · Black ring: intended target</p>
<canvas id="path" width="1100" height="430" aria-label="Animated two dimensional pointer positions"></canvas>
<div id="metrics"></div>
<p>Horizontal position over time, with the same scale for raw and filtered movement.</p>
<canvas id="chart" width="1100" height="300" aria-label="Raw, intended and filtered horizontal position over six seconds"></canvas>
<p>Error includes delay during the reach. Target dwell means time within a 12-pixel radius during the last two seconds; it is not a measured click success rate. Smoother motion does not establish better usability. No user statistics are collected.</p>
<script>
const data=__DATA__, select=document.querySelector('#scenario'), slider=document.querySelector('#time'), button=document.querySelector('#play');
document.querySelector('#label').textContent=data.label;
for(const key of Object.keys(data.groups)){const option=document.createElement('option');option.textContent=key;select.append(option)}
select.value=Object.keys(data.groups).find(k=>k.includes('large_4Hz')&&k.includes('85'))||select.value;
let playing=true,last=0,t=0,rows;
function line(ctx,x,y,color){ctx.strokeStyle=color;ctx.beginPath();rows.forEach((r,i)=>i?ctx.lineTo(x(r),y(r)):ctx.moveTo(x(r),y(r)));ctx.stroke()}
function change(){rows=data.groups[select.value];t=0;const measured=rows.filter(r=>r[0]>=2),tail=rows.filter(r=>r[0]>=4);
 const rms=(a,b)=>Math.sqrt(measured.reduce((s,r)=>s+(r[a]-r[3])**2+(r[b]-r[4])**2,0)/measured.length);
 const raw=rms(1,2),out=rms(5,6),dwell=tail.filter(r=>Math.hypot(r[5]-r[3],r[6]-r[4])<=12).length/tail.length;
 document.querySelector('#metrics').textContent=`Raw RMS error: ${raw.toFixed(1)} px. Filtered RMS error: ${out.toFixed(1)} px. `+(select.value.includes('without_shake')?'':`Error reduction: ${(100*(1-out/raw)).toFixed(1)}%. `)+`Target dwell: ${(100*dwell).toFixed(1)}%.`;
 const c=document.querySelector('#chart').getContext('2d');c.clearRect(0,0,1100,300);c.lineWidth=2;
 const max=Math.max(100,...rows.flatMap(r=>[Math.abs(r[1]),Math.abs(r[3]),Math.abs(r[5])]));
 const x=r=>50+r[0]/6*1000,y=v=>150-v/max*120;
 c.fillStyle='#172235';c.font='16px system-ui';c.fillText(`${max.toFixed(0)} px`,2,20);c.fillText(`−${max.toFixed(0)} px`,2,285);c.fillText('0 s',50,295);c.fillText('6 s',1020,295);
 line(c,x,r=>y(r[1]),'#b42318');line(c,x,r=>y(r[3]),'#172235');line(c,x,r=>y(r[5]),'#175cd3');draw();}
function draw(){const r=rows.reduce((a,b)=>Math.abs(b[0]-t)<Math.abs(a[0]-t)?b:a),c=document.querySelector('#path').getContext('2d');c.clearRect(0,0,1100,430);c.save();c.translate(500,215);c.scale(.55,.55);c.lineWidth=4;
 c.strokeStyle='#172235';c.beginPath();c.arc(r[3],r[4],12,0,2*Math.PI);c.stroke();c.strokeStyle='#b42318';c.beginPath();c.arc(r[1],r[2],14,0,2*Math.PI);c.stroke();c.strokeStyle='#175cd3';c.beginPath();c.moveTo(r[5]-14,r[6]);c.lineTo(r[5]+14,r[6]);c.moveTo(r[5],r[6]-14);c.lineTo(r[5],r[6]+14);c.stroke();c.restore();slider.value=t;document.querySelector('#seconds').textContent=t.toFixed(2)+' s';}
select.onchange=change;slider.oninput=()=>{playing=false;button.textContent='Play';t=+slider.value;draw()};button.onclick=()=>{playing=!playing;button.textContent=playing?'Pause':'Play'};
function frame(now){if(playing){t=(t+Math.min((now-last)/1000,.05))%6;draw()}last=now;requestAnimationFrame(frame)}change();requestAnimationFrame(frame);
</script></html>'''
args.output.write_text(page.replace('__DATA__', data), encoding='utf-8')
print(args.output)
