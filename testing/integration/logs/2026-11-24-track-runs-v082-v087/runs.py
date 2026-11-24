"""Summarise each section of a follow serial log: distinct runs, stop info, BEND trace."""
import re
import sys

lines = open(sys.argv[1], encoding='utf-8', errors='replace').read().splitlines()
sections = []
for i, ln in enumerate(lines):
    if re.fullmatch(r'[A-Za-z0-9 _.-]+:\s*', ln):
        sections.append((ln.strip(), i))
sections.append(('END', len(lines)))

FLAGS = ['blue_exit', 'red_grace', 'blue_after_recovery', 'red_exit', 'red_pivot', 'red_slow',
         'red_return_grace', 'red_exit_turn', 'red_release', 'red_clear_pivot', 'red_exit_align',
         'red_align', 'red_first', 'red_handoff']

for (name, a), (_, b) in zip(sections, sections[1:]):
    seg = lines[a:b]
    ts = [int(m.group(1)) for ln in seg for m in [re.search(r'(?:^|:)t=(\d+)', ln)] if m]
    print(f'===== {name} lines {a+1}-{b}  t={min(ts) if ts else "?"}..{max(ts) if ts else "?"}')
    stops = {}
    for ln in seg:
        if ln.startswith('FOLLOW['):
            f = dict(re.findall(r'(\w+)=([^ \]]+)', ln))
            key = f.get('stop_t')
            on = [k for k in FLAGS if f.get(k) == '1']
            stops[key] = (f.get('reason'), f.get('elapsed'), f.get('stop_rgb'), f.get('stop_ir'),
                          f.get('stop_join_ms'), ' '.join(on))
    for k, v in stops.items():
        print(f'  STOP stop_t={k} reason={v[0]} elapsed={v[1]} rgb={v[2]} ir={v[3]} join_ms={v[4]}')
        print(f'       flags: {v[5]}')
    bends = [ln for ln in seg if ln.startswith('BEND[')]
    print(f'  BEND lines: {len(bends)}')
    for ln in bends:
        f = dict(re.findall(r'(\w+)=([^ \]]+)', ln))
        on = [k for k in FLAGS if f.get(k) == '1']
        print(f"   el={f['elapsed']:>5} ir={f['ir']:>9} rgb={f['rgb']:<18} rgbc={f['rgbc']:<16} "
              f"turn={f['turn']:>3} duty={f['duty_half']:>8} join={f['join']} enc={f['enc']:<12} {' '.join(on)}")
