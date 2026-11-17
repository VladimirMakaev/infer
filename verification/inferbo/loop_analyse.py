#!/usr/bin/env python3
"""Verdicts for the loop grid.

  genuinely out of bounds + no report -> MISSED BUFFER OVERRUN (soundness)
  in bounds + a report                -> false positive (precision; never a bug claim)
A missed verdict only counts if the configuration's reach sibling fired, otherwise the
function was not visible and the silence says nothing.
"""
import json

man = json.load(open('manifest_loop.json'))
fired = {r['procedure'] for r in json.load(open('out_loop/report.json'))}

reaches = [f for f, m in man.items() if m['kind'] == 'reach']
print('reach controls: %d/%d fired' % (sum(1 for r in reaches if r in fired), len(reaches)))

missed, blind, fp, ok_rep, ok_sil = [], [], [], 0, 0
for f, m in man.items():
    if m['kind'] != 'probe':
        continue
    rep = f in fired
    if m['oob']:
        if rep:
            ok_rep += 1
        elif m['reach'] in fired:
            missed.append((f, m))
        else:
            blind.append((f, m))
    else:
        if rep:
            fp.append((f, m))
        else:
            ok_sil += 1

print('out-of-bounds configs: %d reported, %d MISSED, %d inconclusive'
      % (ok_rep, len(missed), len(blind)))
print('in-bounds configs:     %d silent, %d false positives' % (ok_sil, len(fp)))

fmt = ('   %-7s size=%-3d start=%-3d limit=%-3d step=%d  touches [%d,%d]  %s')
for f, m in sorted(missed, key=lambda t: (t[1]['size'], t[1]['start'], t[1]['step'])):
    print(fmt % ('MISSED', m['size'], m['start'], m['limit'], m['step'], m['lo'],
                 m['hi'], f))
for f, m in sorted(fp, key=lambda t: (t[1]['size'], t[1]['start'], t[1]['step']))[:12]:
    print(fmt % ('FP', m['size'], m['start'], m['limit'], m['step'], m['lo'], m['hi'], f))
json.dump({f: m for f, m in missed}, open('missed_loop.json', 'w'), indent=1)
