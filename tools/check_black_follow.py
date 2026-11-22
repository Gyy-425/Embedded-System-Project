"""Strict local follow checks and optional separate firmware builds; never flashes."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


LOOP_DIRECTIONS = (('Clockwise', 1), ('Anticlockwise', -1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', required=True)
    parser.add_argument('--firmware', action='store_true')
    parser.add_argument('--corners', action='store_true', help='Build left and right corner test pairs')
    parser.add_argument('--clipped-loop', action='store_true', help='Build the continuous clipped-loop test pair')
    parser.add_argument('--blue-bend-right', action='store_true',
                        help='Build the clockwise blue bend trial with --clipped-loop --home-rgb')
    parser.add_argument('--loop-recovery', action='store_true',
                        help='Build both directional recovery pairs with --clipped-loop --home-rgb')
    parser.add_argument('--early-bend', action='store_true',
                        help='Build only the powered clockwise early bend trial with --loop-recovery')
    parser.add_argument('--home-track', action='store_true',
                        help='Use the new home track calibration with --early-bend')
    parser.add_argument('--blue-steer-ramp', action='store_true',
                        help='Ramp the strongest blue corrections with --home-track')
    parser.add_argument('--blue-ramp-start', type=int, choices=(5, 8), default=8,
                        help='Initial steering limit for --blue-steer-ramp')
    parser.add_argument('--two-turn', action='store_true',
                        help='Build the powered clockwise 30%% two-turn trial with --blue-ramp-start 5')
    parser.add_argument('--yellow-exit-cue', action='store_true',
                        help='Build the earlier yellow approach trial at 27.5%%')
    parser.add_argument('--yellow-lead-cue', action='store_true',
                        help='Test an earlier correction while yellow still matches')
    parser.add_argument('--blue-reversal-damping', action='store_true',
                        help='Slow the blue steering ramp after a rapid reversal')
    parser.add_argument('--blue-steer-wide-range', action='store_true',
                        help='Use a wider blue-only IR range for steering')
    parser.add_argument('--stop-trace', action='store_true',
                        help='Save the last sensor inputs used by the controller at a stop')
    parser.add_argument('--blue-red-handoff', action='store_true',
                        help='Follow straight through the measured blue-to-red blend with weak IR')
    parser.add_argument('--blue-second-boost', action='store_true',
                        help='Sharpen the next strong outside cue after recovering blue')
    parser.add_argument('--blue-outer-edge', action='store_true',
                        help='Increase the right correction on a sustained outside edge while blue still matches')
    parser.add_argument('--blue-outer-recovery', action='store_true',
                        help='Keep a tighter right turn when the outside edge returns during blue recovery')
    parser.add_argument('--blue-bend-slow', action='store_true',
                        help='Hold lower forward duty briefly after the blue outside edge')
    parser.add_argument('--blue-confirm-grace', action='store_true',
                        help='Allow a late blue match one additional frame to confirm')
    parser.add_argument('--bend-trace', action='store_true',
                        help='Report controller sensor inputs and wheel commands during the run')
    parser.add_argument('--blue-exit-lead', action='store_true',
                        help='Start a brief right turn when the measured blue exit cue appears')
    parser.add_argument('--blue-exit-tight', action='store_true',
                        help='Increase the steering differential on the blue exit cue')
    parser.add_argument('--blue-exit-early', action='store_true',
                        help='Start the tighter turn on fading blue while the outside edge is still visible')
    parser.add_argument('--blue-red-confirm-grace', action='store_true',
                        help='Allow a bounded slow handoff when the blue exit progresses toward red')
    parser.add_argument('--blue-exit-after-recovery', action='store_true',
                        help='Use a fading-blue cue soon after recovering blue')
    parser.add_argument('--red-exit-lead', action='store_true',
                        help='Start the right turn on fading red after seeing the outside edge')
    parser.add_argument('--red-exit-early', action='store_true',
                        help='Include the earlier red fade reading seen at the bend')
    parser.add_argument('--blue-red-bend-trial', action='store_true',
                        help='Recognise two more blue exit readings and tighten the red exit turn')
    parser.add_argument('--blue-red-bend-cues', action='store_true',
                        help='Recognise the brighter bend readings and tighten the blue exit turn')
    parser.add_argument('--red-second-pivot', action='store_true',
                        help='Briefly reverse the inside wheel on the second red turn cue')
    parser.add_argument('--red-second-slow', action='store_true',
                        help='Use a 25%% cap and slow the approach and turn at the second red bend')
    parser.add_argument('--red-bend-after-handoff', action='store_true',
                        help='Arm the slower red turn after a confirmed late blue-to-red handoff')
    parser.add_argument('--red-first-bend-boost', action='store_true',
                        help='Tighten the first red bend after the blue-to-red handoff')
    parser.add_argument('--red-first-bend-tighter', action='store_true',
                        help='Increase the first-red pivot and enable it for red-only starts')
    parser.add_argument('--blue-red-join-refinement', action='store_true',
                        help='Use the recorded late blue cue and mixed blue-to-red join samples')
    parser.add_argument('--bend-cue-refinement', action='store_true',
                        help='Use the measured blue edge crossing and offset second-red fade cues')
    parser.add_argument('--red-second-exit-refinement', action='store_true',
                        help='Recover the second-red cue after a short loss and tighten its pivot')
    parser.add_argument('--blue-diagonal-exit', action='store_true',
                        help='Use the measured blue exit cues, black-border hold, and slow turn into the blend')
    parser.add_argument('--red-handoff-align', action='store_true',
                        help='Give the confirmed blue-to-red handoff a short forward alignment')
    parser.add_argument('--red-exit-align', action='store_true',
                        help='Add a short forward arc after the second red pivot')
    parser.add_argument('--red-between-bends', action='store_true',
                        help='Use the clear-channel drop after bright red to cue the second red pivot')
    parser.add_argument('--red-inside-release', action='store_true',
                        help='Release the second red pivot at the measured inside-edge cue')
    parser.add_argument('--red-exit-turn', action='store_true',
                        help='Add a short forward right arc on the fading-red exit cue')
    parser.add_argument('--red-exit-turn-early', action='store_true',
                        help='Allow the extra red exit arc in the last 100 ms of alignment')
    parser.add_argument('--red-exit-turn-tighter', action='store_true',
                        help='Reduce inside-wheel duty for a tighter red exit arc')
    parser.add_argument('--red-return-grace', action='store_true',
                        help='Allow one more fresh red frame after a late return from the second red bend')
    parser.add_argument('--red-exit-turn-strong', action='store_true',
                        help='Start the last red arc earlier and increase wheel-duty differential')
    parser.add_argument('--red-exit-turn-hold', action='store_true',
                        help='Extend the last red arc while RGB and both IR sensors support it')
    parser.add_argument('--blue-exit-pivot', action='store_true',
                        help='Use a brief pivot at the qualified blue exit while blue still matches')
    parser.add_argument('--blue-red-late-confirm', action='store_true',
                        help='Allow one more frame when red matches late at the blue exit')
    parser.add_argument('--blue-red-blend-grace', action='store_true',
                        help='Also allow the measured late mixed frame at the blue-to-red joint')
    parser.add_argument('--blue-red-blend-margin', action='store_true',
                        help='Use the expanded near-red cue for the bounded blue-to-red handoff')
    parser.add_argument('--blue-exit-left-boost', action='store_true',
                        help='Use 30 percent left duty during the existing blue exit pivot')
    parser.add_argument('--red-first-left-boost', action='store_true',
                        help='Use 30 percent left duty during the first red pivot')
    parser.add_argument('--red-second-arc-boost', action='store_true',
                        help='Strengthen the forward arc immediately after the second red pivot')
    parser.add_argument('--blue-second-left-boost', action='store_true',
                        help='Increase left duty for the second blue bend with the two IR edge cues')
    parser.add_argument('--red-final-left-boost', action='store_true',
                        help='Use 30 percent left duty only during the final red right arc')
    parser.add_argument('--blue-exit-edge-confirm', action='store_true',
                        help='Require the ordered left-to-right IR cue before the blue exit pivot')
    parser.add_argument('--blue-red-late-approach', action='store_true',
                        help='Allow the measured weaker near-red cue and more time to process confirmation')
    parser.add_argument('--red-diagonal-reacquire', action='store_true',
                        help='Retain the second red bend cue after the first pivot returns to solid red')
    parser.add_argument('--red-final-arc-extend', action='store_true',
                        help='Hold the final red arc for another 100 ms at 35 percent left duty when red and IR allow')
    parser.add_argument('--red-final-arc-strong', action='store_true',
                        help='v0.81: final red arc left wheel 35 percent, extension 40 percent')
    parser.add_argument('--blue-red-join-extend', action='store_true',
                        help='v0.82: +250 ms blue/red join while the reading stays red-leading')
    parser.add_argument('--red-pivot-inside-release', action='store_true',
                        help='v0.83: release the second red pivot on the inside IR edge alone')
    parser.add_argument('--red-outside-recovery', action='store_true',
                        help='v0.84: spin right when the right IR follows the left IR over the outer red border')
    parser.add_argument('--red-right-edge-outside', action='store_true',
                        help='v0.85: in the red corner a right IR edge spins right instead of steering left')
    parser.add_argument('--red-corner-refine', action='store_true',
                        help='v0.86: right-edge spin after first red pivot, red rejoin grace, outer-border arc hold')
    parser.add_argument('--red-spin-hold', action='store_true',
                        help='v0.87: hold the red right spin until a new red frame, left edge or 350 ms')
    parser.add_argument('--home-rgb', action='store_true', help='Use the provisional home RGB profiles')
    parser.add_argument('--room2-rgb', action='store_true',
                        help='Firmware only: use the ROOM2 lighting BLUE/BACKGROUND boxes (home track)')
    parser.add_argument('--sdk')
    parser.add_argument('--duty', type=float, choices=(25,27.5,30,35,40,45,50))
    parser.add_argument('--track', choices=('black','yellow','red','blue','multi'), default='black')
    parser.add_argument('--turn', type=int, choices=(8,20), default=8)
    args = parser.parse_args()
    if args.duty is None:
        args.duty = 30 if args.two_turn else (25 if args.track == 'multi' else 35)
    elif args.duty.is_integer():
        args.duty = int(args.duty)
    if args.two_turn and not (args.track == 'multi' and args.blue_ramp_start == 5):
        parser.error('--two-turn requires --track multi --blue-ramp-start 5')
    if args.track == 'multi':
        valid_duties = (30,) if args.two_turn else (25,)
        if args.blue_ramp_start == 5 and not args.two_turn:
            valid_duties = (25, 27.5, 30)
        if args.duty not in valid_duties:
            parser.error('Use 30% for two-turn, 25%, 27.5% or 30% for gentle blue, or 25% otherwise')
    elif args.duty == 27.5:
        parser.error('27.5% requires the gentle blue follower')
    regression_duty = 27.5 if args.red_second_slow else args.duty
    if args.yellow_exit_cue and regression_duty != 27.5:
        parser.error('--yellow-exit-cue requires --duty 27.5 or --red-second-slow at 25%')
    if args.yellow_lead_cue and not args.yellow_exit_cue:
        parser.error('--yellow-lead-cue requires --yellow-exit-cue')
    if args.blue_reversal_damping and not args.yellow_lead_cue:
        parser.error('--blue-reversal-damping requires --yellow-lead-cue')
    if args.blue_steer_wide_range and not args.blue_reversal_damping:
        parser.error('--blue-steer-wide-range requires --blue-reversal-damping')
    if args.stop_trace and not args.blue_steer_wide_range:
        parser.error('--stop-trace requires --blue-steer-wide-range')
    if args.blue_red_handoff and not args.stop_trace:
        parser.error('--blue-red-handoff requires --stop-trace')
    if args.blue_second_boost and not args.blue_red_handoff:
        parser.error('--blue-second-boost requires --blue-red-handoff')
    if args.blue_outer_edge and (not args.blue_red_handoff or args.blue_second_boost):
        parser.error('--blue-outer-edge requires --blue-red-handoff without --blue-second-boost')
    if args.blue_outer_recovery and not args.blue_outer_edge:
        parser.error('--blue-outer-recovery requires --blue-outer-edge')
    if args.blue_bend_slow and not args.blue_outer_recovery:
        parser.error('--blue-bend-slow requires --blue-outer-recovery')
    if args.blue_confirm_grace and not args.blue_bend_slow:
        parser.error('--blue-confirm-grace requires --blue-bend-slow')
    if args.bend_trace and not args.blue_confirm_grace:
        parser.error('--bend-trace requires --blue-confirm-grace')
    if args.blue_exit_lead and not args.bend_trace:
        parser.error('--blue-exit-lead requires --bend-trace')
    if (args.blue_exit_tight or args.blue_exit_early) and not args.blue_exit_lead:
        parser.error('--blue-exit-tight and --blue-exit-early require --blue-exit-lead')
    if args.blue_red_confirm_grace and not args.blue_exit_early:
        parser.error('--blue-red-confirm-grace requires --blue-exit-early')
    if args.blue_exit_after_recovery and not args.blue_red_confirm_grace:
        parser.error('--blue-exit-after-recovery requires --blue-red-confirm-grace')
    if args.red_exit_lead and not args.blue_exit_after_recovery:
        parser.error('--red-exit-lead requires --blue-exit-after-recovery')
    if args.red_exit_early and not args.red_exit_lead:
        parser.error('--red-exit-early requires --red-exit-lead')
    if args.blue_red_bend_trial and not args.red_exit_early:
        parser.error('--blue-red-bend-trial requires --red-exit-early')
    if args.blue_red_bend_cues and not args.blue_red_bend_trial:
        parser.error('--blue-red-bend-cues requires --blue-red-bend-trial')
    if args.red_second_pivot and not args.blue_red_bend_cues:
        parser.error('--red-second-pivot requires --blue-red-bend-cues')
    if args.red_second_slow and not (args.red_second_pivot and args.duty == 25):
        parser.error('--red-second-slow requires --red-second-pivot and --duty 25')
    if args.red_bend_after_handoff and not args.red_second_slow:
        parser.error('--red-bend-after-handoff requires --red-second-slow')
    if args.red_first_bend_boost and not args.red_bend_after_handoff:
        parser.error('--red-first-bend-boost requires --red-bend-after-handoff')
    if args.red_first_bend_tighter and not args.red_first_bend_boost:
        parser.error('--red-first-bend-tighter requires --red-first-bend-boost')
    if args.blue_red_join_refinement and not args.red_first_bend_tighter:
        parser.error('--blue-red-join-refinement requires --red-first-bend-tighter')
    if args.bend_cue_refinement and not args.blue_red_join_refinement:
        parser.error('--bend-cue-refinement requires --blue-red-join-refinement')
    if args.red_second_exit_refinement and not args.bend_cue_refinement:
        parser.error('--red-second-exit-refinement requires --bend-cue-refinement')
    if args.blue_diagonal_exit and not args.red_second_exit_refinement:
        parser.error('--blue-diagonal-exit requires --red-second-exit-refinement')
    if args.red_handoff_align and not args.blue_diagonal_exit:
        parser.error('--red-handoff-align requires --blue-diagonal-exit')
    if args.red_exit_align and not args.red_handoff_align:
        parser.error('--red-exit-align requires --red-handoff-align')
    if args.red_between_bends and not args.red_exit_align:
        parser.error('--red-between-bends requires --red-exit-align')
    if args.red_inside_release and not args.red_between_bends:
        parser.error('--red-inside-release requires --red-between-bends')
    if args.red_exit_turn and not args.red_inside_release:
        parser.error('--red-exit-turn requires --red-inside-release')
    if args.red_exit_turn_early and not args.red_exit_turn:
        parser.error('--red-exit-turn-early requires --red-exit-turn')
    if args.red_exit_turn_tighter and not args.red_exit_turn_early:
        parser.error('--red-exit-turn-tighter requires --red-exit-turn-early')
    if args.red_return_grace and not args.red_exit_turn_tighter:
        parser.error('--red-return-grace requires --red-exit-turn-tighter')
    if args.red_exit_turn_strong and not args.red_return_grace:
        parser.error('--red-exit-turn-strong requires --red-return-grace')
    if args.red_exit_turn_hold and not args.red_exit_turn_strong:
        parser.error('--red-exit-turn-hold requires --red-exit-turn-strong')
    if args.blue_exit_pivot and not args.red_exit_turn_hold:
        parser.error('--blue-exit-pivot requires --red-exit-turn-hold')
    if args.blue_red_late_confirm and not args.blue_exit_pivot:
        parser.error('--blue-red-late-confirm requires --blue-exit-pivot')
    if args.blue_red_blend_grace and not args.blue_red_late_confirm:
        parser.error('--blue-red-blend-grace requires --blue-red-late-confirm')
    if args.blue_red_blend_margin and not args.blue_red_blend_grace:
        parser.error('--blue-red-blend-margin requires --blue-red-blend-grace')
    if args.blue_exit_left_boost and not args.blue_red_blend_margin:
        parser.error('--blue-exit-left-boost requires --blue-red-blend-margin')
    if args.red_first_left_boost and not args.blue_exit_left_boost:
        parser.error('--red-first-left-boost requires --blue-exit-left-boost')
    if args.red_second_arc_boost and not args.red_first_left_boost:
        parser.error('--red-second-arc-boost requires --red-first-left-boost')
    if args.blue_second_left_boost and not args.red_second_arc_boost:
        parser.error('--blue-second-left-boost requires --red-second-arc-boost')
    if args.red_final_left_boost and not args.blue_second_left_boost:
        parser.error('--red-final-left-boost requires --blue-second-left-boost')
    if args.blue_exit_edge_confirm and not args.red_final_left_boost:
        parser.error('--blue-exit-edge-confirm requires --red-final-left-boost')
    if args.blue_red_late_approach and not args.blue_exit_edge_confirm:
        parser.error('--blue-red-late-approach requires --blue-exit-edge-confirm')
    if args.red_diagonal_reacquire and not args.blue_red_late_approach:
        parser.error('--red-diagonal-reacquire requires --blue-red-late-approach')
    if args.red_spin_hold and not args.red_corner_refine:
        parser.error('--red-spin-hold requires --red-corner-refine')
    if args.red_corner_refine and not args.red_right_edge_outside:
        parser.error('--red-corner-refine requires --red-right-edge-outside')
    if args.red_right_edge_outside and not args.red_outside_recovery:
        parser.error('--red-right-edge-outside requires --red-outside-recovery')
    if args.red_outside_recovery and not args.red_pivot_inside_release:
        parser.error('--red-outside-recovery requires --red-pivot-inside-release')
    if args.red_pivot_inside_release and not args.blue_red_join_extend:
        parser.error('--red-pivot-inside-release requires --blue-red-join-extend')
    if args.blue_red_join_extend and not args.red_final_arc_strong:
        parser.error('--blue-red-join-extend requires --red-final-arc-strong')
    if args.red_final_arc_strong and not args.red_final_arc_extend:
        parser.error('--red-final-arc-strong requires --red-final-arc-extend')
    if args.red_final_arc_extend and not args.red_diagonal_reacquire:
        parser.error('--red-final-arc-extend requires --red-diagonal-reacquire')
    if args.corners and args.clipped_loop:
        parser.error('Select --corners or --clipped-loop')
    if (args.corners or args.clipped_loop) and (args.track != 'multi' or not args.firmware):
        parser.error('--corners and --clipped-loop require --track multi --firmware')
    if args.clipped_loop and args.turn != 8:
        parser.error('--clipped-loop uses T8 centring with a T14 peak')
    if args.blue_bend_right and not (args.clipped_loop and args.home_rgb):
        parser.error('--blue-bend-right requires --clipped-loop --home-rgb')
    if args.loop_recovery and not (args.clipped_loop and args.home_rgb and not args.blue_bend_right):
        parser.error('--loop-recovery requires --clipped-loop --home-rgb without --blue-bend-right')
    if args.early_bend and not args.loop_recovery:
        parser.error('--early-bend requires --loop-recovery')
    if args.home_track and not args.early_bend:
        parser.error('--home-track requires --early-bend')
    if args.blue_steer_ramp and not args.home_track:
        parser.error('--blue-steer-ramp requires --home-track')
    if args.blue_ramp_start != 8 and not args.blue_steer_ramp:
        parser.error('--blue-ramp-start 5 requires --blue-steer-ramp')
    if args.room2_rgb and not (args.home_track and args.home_rgb and args.firmware):
        parser.error('--room2-rgb requires --home-track --home-rgb --firmware')
    if args.home_rgb and not (args.corners or args.clipped_loop):
        parser.error('--home-rgb requires --corners or --clipped-loop')
    if not re.fullmatch(r'[A-Za-z0-9_-]+', args.label):
        parser.error('Use a new alphanumeric label')
    repo = Path(__file__).resolve().parents[1]
    logs = repo / 'testing/integration/logs'
    if list(logs.glob(args.label + '-*')):
        parser.error('Label already exists')
    artifacts = repo / 'output/firmware'
    before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in artifacts.rglob('*.uf2')}
    evidence = dict(status='RUNNING', hardware='NOT RUN',
                    rgb_profile='home' if args.home_rgb else 'lab', results=[])
    env = dict(os.environ, LC_ALL='C')

    def run(command, name, cwd=repo, reject=False):
        log = logs / (args.label + '-' + name + '.log')
        with log.open('x', encoding='utf-8') as output:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=output,
                                    stderr=subprocess.STDOUT, text=True)
        content = log.read_text(errors='replace')
        warnings = [s for s in content.splitlines() if 'warning:' in s.lower()]
        evidence['results'].append(dict(name=name, exit_code=result.returncode,
            expected_rejection=reject, warnings=warnings, log=str(log.relative_to(repo))))
        if bool(result.returncode) != reject or (warnings and not reject):
            print(content[-6000:], flush=True)
            raise RuntimeError(name + ' failed/warned')
        print(name + (': rejected' if reject else ': PASS'), flush=True)

    try:
        strict = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wconversion',
                  '-Wshadow', '-Wstrict-prototypes']
        core = ['app_program/subsystem2/black_follow.c']
        colour = ['app_program/subsystem1/rgb_classifier.c',
                  'app_program/subsystem1/rgb_calibration.c']
        with tempfile.TemporaryDirectory(prefix='aai05-follow-') as scratch:
            vectors = []
            for label, sign in [('centred', 0), ('robot-left', 1), ('robot-right', -1)]:
                path = repo / ('testing/calibration/2026-11-17-black-line-' + label + '-serial.txt')
                text = path.read_text(encoding='utf-8-sig')
                rgb = {}
                for match in re.finditer(r'^RGB\[([^\]]+)\]', text, re.M):
                    row = dict(re.findall(r'(\w+)=([^\s\]]+)', match[1]))
                    rgb[int(row['t'])] = row
                for match in re.finditer(r'^t=(\d+) IR\[L=(\d+) R=(\d+) valid=(\d+) err=(-?\d+) age=(\d+)\]', text, re.M):
                    now, left, right, valid, error, age = map(int, match.groups())
                    if valid != 1 or error != 0 or age > 100:
                        raise RuntimeError('Invalid measured IR in position fixture')
                    row = rgb[now]
                    snapshot = {k: int(row[v]) for k, v in [('red','R'),('green','G'),('blue','B'),('clear','C'),('sequence','seq'),('valid','valid'),('has_sample','has'),('ready','ready'),('status','status'),('saturated','sat')]}
                    snapshot.update(sampled_ms=now-int(row['age']), device_id=int(row['id'],16))
                    fields = ','.join('.'+k+'='+str(v)+'U' for k,v in snapshot.items())
                    vectors.append('{'+f'{now}U,{left}U,{right}U,{sign},'+'{'+fields+'}}')
            (Path(scratch)/'black_follow_vectors.h').write_text(
                'static const follow_vector_t g_vectors[] = {\n'+',\n'.join(vectors)+'\n};\n')
            provenance = json.loads((repo/'testing/host/fixtures/rgb_2026_11_17_yellow_corridor_provenance.json').read_text())
            for source in provenance['sources']:
                path = repo / source['archive'].replace('\\','/')
                if hashlib.sha256(path.read_bytes()).hexdigest() != source['sha256']:
                    raise RuntimeError('Corridor source changed without provenance update')
            yellow_vectors = []
            for label, sign in [('centre',0),('robot-left',1),('robot-right',-1)]:
                path = repo / ('testing/calibration/2026-11-17-yellow-corridor-' + label + '-serial.txt')
                content = path.read_text(encoding='utf-8-sig')
                rgb = {int(row['t']): row for row in
                    (dict(re.findall(r'(\w+)=([^\s\]]+)', match[1])) for match in
                     re.finditer(r'^RGB\[([^\]]+)\]', content, re.M))}
                for match in re.finditer(r'^t=(\d+) IR\[L=(\d+) R=(\d+) valid=(\d+) err=(-?\d+) age=(\d+)\]', content, re.M):
                    now,left,right,valid,error,age = map(int,match.groups())
                    if valid != 1 or error != 0 or age > 100:
                        raise RuntimeError('Invalid measured corridor IR')
                    row = rgb[now]
                    snapshot = {k:int(row[v]) for k,v in [('red','R'),('green','G'),('blue','B'),('clear','C'),('sequence','seq'),('valid','valid'),('has_sample','has'),('ready','ready'),('status','status'),('saturated','sat')]}
                    snapshot.update(sampled_ms=now-int(row['age']),device_id=int(row['id'],16))
                    fields = ','.join('.'+k+'='+str(v)+'U' for k,v in snapshot.items())
                    yellow_vectors.append('{'+f'{now}U,{left}U,{right}U,{sign},'+'{'+fields+'}}')
            (Path(scratch)/'yellow_follow_vectors.h').write_text(
                'static const follow_vector_t g_vectors[] = {\n'+',\n'.join(yellow_vectors)+'\n};\n')
            provenance = json.loads((repo/'testing/host/fixtures/rgb_2026_11_17_red_corridor_provenance.json').read_text())
            for source in provenance['sources']:
                path = repo / source['archive'].replace('\\','/')
                if hashlib.sha256(path.read_bytes()).hexdigest() != source['sha256']:
                    raise RuntimeError('Corridor source changed without provenance update')
            red_vectors = []
            for label, sign in [('centre',0),('robot-left',1),('robot-right',-1)]:
                path = repo / ('testing/calibration/2026-11-17-red-corridor-' + label + '-serial.txt')
                content = path.read_text(encoding='utf-8-sig')
                rgb = {int(row['t']): row for row in
                    (dict(re.findall(r'(\w+)=([^\s\]]+)', match[1])) for match in
                     re.finditer(r'^RGB\[([^\]]+)\]', content, re.M))}
                for match in re.finditer(r'^t=(\d+) IR\[L=(\d+) R=(\d+) valid=(\d+) err=(-?\d+) age=(\d+)\]', content, re.M):
                    now,left,right,valid,error,age = map(int,match.groups())
                    if valid != 1 or error != 0 or age > 100:
                        raise RuntimeError('Invalid measured corridor IR')
                    row = rgb[now]
                    snapshot = {k:int(row[v]) for k,v in [('red','R'),('green','G'),('blue','B'),('clear','C'),('sequence','seq'),('valid','valid'),('has_sample','has'),('ready','ready'),('status','status'),('saturated','sat')]}
                    snapshot.update(sampled_ms=now-int(row['age']),device_id=int(row['id'],16))
                    fields = ','.join('.'+k+'='+str(v)+'U' for k,v in snapshot.items())
                    red_vectors.append('{'+f'{now}U,{left}U,{right}U,{sign},'+'{'+fields+'}}')
            (Path(scratch)/'red_follow_vectors.h').write_text(
                'static const follow_vector_t g_vectors[] = {\n'+',\n'.join(red_vectors)+'\n};\n')
            fixture = repo/'testing/host/fixtures/rgb_2026_11_19_blue_corridor.csv'
            provenance = json.loads(fixture.with_name(fixture.stem+'_provenance.json').read_text())
            for path, digest in [(fixture, provenance['fixture_sha256']),
                                 *((repo/source['archive'], source['sha256'])
                                   for source in provenance['sources'])]:
                if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                    raise RuntimeError('Blue replay data changed without provenance update')
            blue_vectors = []
            positions = ('blue-centre', 'blue-robot-left', 'blue-robot-right')
            with fixture.open(newline='', encoding='utf-8') as stream:
                for row in csv.DictReader(stream):
                    position = positions.index(row['split'])
                    now, age, left, right = (int(row[k]) for k in
                                             ('now_ms', 'ir_age_ms', 'left_ir', 'right_ir'))
                    fields = ('sampled_ms', 'sequence', 'red', 'green', 'blue', 'clear',
                              'valid', 'saturated', 'status', 'device_id', 'ready', 'has_sample')
                    snapshot = ','.join(f'.{k}={int(row[k])}U' for k in fields)
                    blue_vectors.append(f'{{{now}U,{now-age}U,{left}U,{right}U,{position}U,'
                                        f'{{{snapshot}}}}}')
            (Path(scratch)/'blue_follow_vectors.h').write_text(
                'static const follow_vector_t g_vectors[] = {\n'+',\n'.join(blue_vectors)+'\n};\n')
            cases = [
                ('core', ['testing/host/test_black_follow.c', *core], []),
                ('owner', ['testing/host/test_black_follow_owner.c', *core,
                           'app_program/subsystem2/motion_control.c',
                           'app_program/subsystem2/motion_chassis.c', *colour], []),
                ('snapshot', ['testing/host/test_follow_snapshot.c'], ['-pthread']),
                ('startup', ['testing/host/test_follow_startup.c'], []),
                ('replay', ['testing/host/test_black_follow_replay.c', *core, *colour], []),
                ('rgb-report-powered', ['testing/host/test_rgb_diag.c',
                    'app_program/subsystem1/rgb_sensor.c', *colour],
                    ['-DAAI05_FOLLOW_APP=1', '-DAAI05_RGB_DIAG=0']),
            ]
            cases.extend([
                ('yellow-core',['testing/host/test_yellow_follow.c',*core],['-DFOLLOW_YELLOW_CORRIDOR=1']),
                ('yellow-replay',['testing/host/test_yellow_follow_replay.c',*core,*colour],['-DFOLLOW_YELLOW_CORRIDOR=1']),
            ])
            owner_files = next(files for name,files,extra in cases if name=='owner')
            for duty in (30,40,45,50):
                cases.append(('owner-duty'+str(duty), owner_files,
                              ['-DFOLLOW_DUTY_LIMIT_PCT='+str(duty)]))
            cases.append(('yellow-owner',owner_files,['-DFOLLOW_YELLOW_CORRIDOR=1']))
            cases.append(('yellow-startup',['testing/host/test_follow_startup.c'],['-DFOLLOW_YELLOW_CORRIDOR=1']))
            cases.extend([
                ('red-core',['testing/host/test_yellow_follow.c',*core],['-DFOLLOW_RED_CORRIDOR=1']),
                ('red-replay',['testing/host/test_red_follow_replay.c',*core,*colour],['-DFOLLOW_RED_CORRIDOR=1']),
                ('red-owner35',owner_files,['-DFOLLOW_RED_CORRIDOR=1','-DFOLLOW_DUTY_LIMIT_PCT=35']),
                ('red-startup',['testing/host/test_follow_startup.c'],['-DFOLLOW_RED_CORRIDOR=1']),
                ('blue-replay',['testing/host/test_blue_follow_replay.c',*core,*colour],['-DFOLLOW_BLUE_CORRIDOR=1']),
                ('blue-owner35',owner_files,['-DFOLLOW_BLUE_CORRIDOR=1','-DFOLLOW_DUTY_LIMIT_PCT=35']),
                ('blue-startup',['testing/host/test_follow_startup.c'],['-DFOLLOW_BLUE_CORRIDOR=1']),
            ])
            multi = ['-DFOLLOW_MULTICOLOUR_CORRIDOR=1', '-DFOLLOW_DUTY_LIMIT_PCT=25']
            cases.extend([
                ('multi-owner25', owner_files, multi),
                ('multi-startup', ['testing/host/test_follow_startup.c'], multi),
            ])
            for name, direction in [('left', -1), ('right', 1)]:
                config = [*multi, f'-DFOLLOW_CORNER_DIRECTION={direction}']
                cases.extend([
                    (f'corner-{name}-owner25', owner_files, config),
                    (f'corner-{name}-startup', ['testing/host/test_follow_startup.c'], config),
                ])
                if args.home_rgb and args.corners:
                    cases.append((f'home-corner-{name}-owner25', owner_files,
                                  [*config, '-DAAI05_RGB_HOME_PROFILE=1']))
            clipped_config = [*multi, '-DFOLLOW_CLIPPED_LOOP=1',
                              f'-DAAI05_RGB_HOME_PROFILE={int(args.home_rgb)}']
            cases.extend([
                ('clipped-owner25', owner_files, clipped_config),
                ('clipped-startup', ['testing/host/test_follow_startup.c'], clipped_config),
            ])
            if args.blue_bend_right or args.loop_recovery:
                bend_config = [*clipped_config, '-DFOLLOW_BLUE_BEND_RIGHT=1']
                cases.extend([
                    ('blue-bend-right-owner25', owner_files, bend_config),
                    ('blue-bend-right-startup', ['testing/host/test_follow_startup.c'], bend_config),
                ])
            if args.loop_recovery:
                for direction_name, direction in LOOP_DIRECTIONS:
                    name = 'loop-' + direction_name.lower()
                    loop_config = [*clipped_config, f'-DFOLLOW_LOOP_RECOVERY_DIRECTION={direction}']
                    cases.extend([
                        (name+'-owner25', owner_files, loop_config),
                        (name+'-startup', ['testing/host/test_follow_startup.c'], loop_config),
                    ])
            if args.early_bend:
                early_config = [*clipped_config, '-DFOLLOW_LOOP_RECOVERY_DIRECTION=1',
                                '-DFOLLOW_EARLY_BEND=1']
                cases.extend([
                    ('early-bend-owner25', owner_files, early_config),
                    ('early-bend-startup', ['testing/host/test_follow_startup.c'], early_config),
                ])
                if args.home_track:
                    track_config = [*early_config, '-DFOLLOW_HOME_TRACK=1']
                    cases.extend([
                        ('home-track-owner25', owner_files, track_config),
                        ('home-track-startup', ['testing/host/test_follow_startup.c'], track_config),
                    ])
                    if args.blue_steer_ramp:
                        ramp_config = [*track_config, '-DFOLLOW_BLUE_STEER_RAMP=1']
                        cases.extend([
                            ('blue-steer-ramp-owner25', owner_files, ramp_config),
                            ('blue-steer-ramp-startup', ['testing/host/test_follow_startup.c'], ramp_config),
                        ])
                        if args.blue_ramp_start == 5:
                            gentle_config = [*ramp_config, '-DFOLLOW_BLUE_RAMP_START=5']
                            cases.extend([
                                ('blue-gentle-owner25', owner_files, gentle_config),
                                ('blue-gentle-startup', ['testing/host/test_follow_startup.c'], gentle_config),
                            ])
                            if regression_duty in (27.5, 30):
                                duty_config = [flag for flag in gentle_config
                                               if flag != '-DFOLLOW_DUTY_LIMIT_PCT=25']
                                duty_config.append(f'-DFOLLOW_DUTY_LIMIT_PCT={int(regression_duty)}')
                                variant = 'blue-gentle-30'
                                if regression_duty == 27.5:
                                    duty_config.append('-DFOLLOW_DUTY_HALF_STEP=1')
                                    variant = 'blue-gentle-27p5'
                                if args.two_turn:
                                    duty_config.append('-DFOLLOW_TWO_TURN=1')
                                    variant = 'two-turn'
                                cases.extend([
                                    (variant+'-owner', owner_files, duty_config),
                                    (variant+'-startup', ['testing/host/test_follow_startup.c'], duty_config),
                                ])
                                if args.yellow_exit_cue:
                                    cue_config = [*duty_config, '-DFOLLOW_YELLOW_EXIT_CUE=1']
                                    cases.extend([
                                        ('yellow-approach-owner', owner_files, cue_config),
                                        ('yellow-approach-startup', ['testing/host/test_follow_startup.c'], cue_config),
                                    ])
                                    if args.yellow_lead_cue:
                                        lead_config = [*cue_config, '-DFOLLOW_YELLOW_LEAD_CUE=1']
                                        cases.extend([
                                            ('yellow-lead-owner', owner_files, lead_config),
                                            ('yellow-lead-startup', ['testing/host/test_follow_startup.c'], lead_config),
                                        ])
                                        if args.blue_reversal_damping:
                                            damped_config = [*lead_config, '-DFOLLOW_BLUE_REVERSAL_DAMPING=1']
                                            cases.extend([
                                                ('blue-damped-owner', owner_files, damped_config),
                                                ('blue-damped-startup', ['testing/host/test_follow_startup.c'], damped_config),
                                            ])
                                            if args.blue_steer_wide_range:
                                                wide_config = [*damped_config, '-DFOLLOW_BLUE_STEER_WIDE_RANGE=1']
                                                cases.extend([
                                                    ('blue-wide-owner', owner_files, wide_config),
                                                    ('blue-wide-startup', ['testing/host/test_follow_startup.c'], wide_config),
                                                ])
                                                if args.stop_trace:
                                                    trace_config = [*wide_config, '-DFOLLOW_STOP_TRACE=1']
                                                    cases.extend([
                                                        ('stop-trace-owner', owner_files, trace_config),
                                                        ('stop-trace-startup', ['testing/host/test_follow_startup.c'], trace_config),
                                                    ])
                                                    if args.blue_red_handoff:
                                                        handoff_config = [*trace_config, '-DFOLLOW_BLUE_RED_HANDOFF=1']
                                                        cases.extend([
                                                            ('blue-red-handoff-owner', owner_files, handoff_config),
                                                            ('blue-red-handoff-startup',
                                                             ['testing/host/test_follow_startup.c'], handoff_config),
                                                        ])
                                                        if args.blue_second_boost:
                                                            boost_config = [*handoff_config, '-DFOLLOW_BLUE_SECOND_BOOST=1']
                                                            cases.extend([
                                                                ('blue-second-owner', owner_files, boost_config),
                                                                ('blue-second-startup',
                                                                 ['testing/host/test_follow_startup.c'], boost_config),
                                                            ])
                                                        if args.blue_outer_edge:
                                                            outer_config = [*handoff_config, '-DFOLLOW_BLUE_OUTER_EDGE=1']
                                                            cases.extend([
                                                                ('blue-outer-owner', owner_files, outer_config),
                                                                ('blue-outer-startup',
                                                                 ['testing/host/test_follow_startup.c'], outer_config),
                                                            ])
                                                            if args.blue_outer_recovery:
                                                                recovery_config = [*outer_config, '-DFOLLOW_BLUE_OUTER_RECOVERY=1']
                                                                cases.extend([
                                                                    ('blue-outer-recovery-owner', owner_files, recovery_config),
                                                                    ('blue-outer-recovery-startup',
                                                                     ['testing/host/test_follow_startup.c'], recovery_config),
                                                                ])
                                                                if args.blue_bend_slow:
                                                                    bend_slow_config = [*recovery_config, '-DFOLLOW_BLUE_BEND_SLOW=1']
                                                                    cases.extend([
                                                                        ('blue-bend-slow-owner', owner_files, bend_slow_config),
                                                                        ('blue-bend-slow-startup',
                                                                         ['testing/host/test_follow_startup.c'], bend_slow_config),
                                                                    ])
                                                                    if args.blue_confirm_grace:
                                                                        confirm_config = [*bend_slow_config, '-DFOLLOW_BLUE_CONFIRM_GRACE=1']
                                                                        cases.extend([
                                                                            ('blue-confirm-owner', owner_files, confirm_config),
                                                                            ('blue-confirm-startup',
                                                                             ['testing/host/test_follow_startup.c'], confirm_config),
                                                                        ])
                                                                        if args.bend_trace:
                                                                            bend_trace_config = [*confirm_config, '-DFOLLOW_BEND_TRACE=1']
                                                                            cases.extend([
                                                                                ('bend-trace-owner', owner_files, bend_trace_config),
                                                                                ('bend-trace-startup',
                                                                                 ['testing/host/test_follow_startup.c'], bend_trace_config),
                                                                            ])
                                                                            if args.blue_exit_lead:
                                                                                exit_config = [*bend_trace_config, '-DFOLLOW_BLUE_EXIT_LEAD=1']
                                                                                cases.extend([
                                                                                    ('blue-exit-lead-owner', owner_files, exit_config),
                                                                                    ('blue-exit-lead-startup',
                                                                                     ['testing/host/test_follow_startup.c'], exit_config),
                                                                                ])
                                                                                if args.blue_exit_tight or args.blue_exit_early:
                                                                                    tight_config = [*exit_config, '-DFOLLOW_BLUE_EXIT_TURN=18']
                                                                                    cases.extend([
                                                                                        ('blue-exit-tight-owner', owner_files, tight_config),
                                                                                        ('blue-exit-tight-startup',
                                                                                         ['testing/host/test_follow_startup.c'], tight_config),
                                                                                    ])
                                                                                if args.blue_exit_early:
                                                                                    early_config = [*exit_config, '-DFOLLOW_BLUE_EXIT_TURN=22']
                                                                                    cases.extend([
                                                                                        ('blue-exit-early-owner', owner_files, early_config),
                                                                                        ('blue-exit-early-startup',
                                                                                         ['testing/host/test_follow_startup.c'], early_config),
                                                                                    ])
                                                                                    if args.blue_red_confirm_grace:
                                                                                        red_confirm_config = [*early_config, '-DFOLLOW_BLUE_RED_CONFIRM_GRACE=1']
                                                                                        cases.extend([
                                                                                            ('blue-red-confirm-owner', owner_files, red_confirm_config),
                                                                                            ('blue-red-confirm-startup',
                                                                                             ['testing/host/test_follow_startup.c'], red_confirm_config),
                                                                                        ])
                                                                                        if args.blue_exit_after_recovery:
                                                                                            recovered_config = [*red_confirm_config, '-DFOLLOW_BLUE_EXIT_AFTER_RECOVERY=1']
                                                                                            cases.extend([
                                                                                                ('blue-exit-after-recovery-owner', owner_files, recovered_config),
                                                                                                ('blue-exit-after-recovery-startup',
                                                                                                 ['testing/host/test_follow_startup.c'], recovered_config),
                                                                                            ])
                                                                                            if args.red_exit_lead:
                                                                                                red_exit_config = [*recovered_config, '-DFOLLOW_RED_EXIT_LEAD=1']
                                                                                                cases.extend([
                                                                                                    ('red-exit-lead-owner', owner_files, red_exit_config),
                                                                                                    ('red-exit-lead-startup',
                                                                                                     ['testing/host/test_follow_startup.c'], red_exit_config),
                                                                                                ])
                                                                                                if args.red_exit_early:
                                                                                                    red_early_config = [*red_exit_config, '-DFOLLOW_RED_EXIT_EARLY=1']
                                                                                                    cases.extend([
                                                                                                        ('red-exit-early-owner', owner_files, red_early_config),
                                                                                                        ('red-exit-early-startup',
                                                                                                         ['testing/host/test_follow_startup.c'], red_early_config),
                                                                                                    ])
                                                                                                    if args.blue_red_bend_trial:
                                                                                                        bend_trial_config = [*red_early_config, '-DFOLLOW_BLUE_RED_BEND_TRIAL=1']
                                                                                                        cases.extend([
                                                                                                            ('blue-red-bends-owner', owner_files, bend_trial_config),
                                                                                                            ('blue-red-bends-startup',
                                                                                                             ['testing/host/test_follow_startup.c'], bend_trial_config),
                                                                                                        ])
                                                                                                        if args.blue_red_bend_cues:
                                                                                                            bend_cues_config = [*bend_trial_config, '-DFOLLOW_BLUE_RED_BEND_CUES=1']
                                                                                                            cases.extend([
                                                                                                                ('blue-red-bend-cues-owner', owner_files, bend_cues_config),
                                                                                                                ('blue-red-bend-cues-startup',
                                                                                                                 ['testing/host/test_follow_startup.c'], bend_cues_config),
                                                                                                            ])
                                                                                                            if args.red_second_pivot:
                                                                                                                second_pivot_config = [*bend_cues_config, '-DFOLLOW_RED_SECOND_PIVOT=1']
                                                                                                                cases.extend([
                                                                                                                    ('red-second-pivot-owner', owner_files, second_pivot_config),
                                                                                                                    ('red-second-pivot-startup',
                                                                                                                     ['testing/host/test_follow_startup.c'], second_pivot_config),
                                                                                                                ])
                                                                                                                if args.red_second_slow:
                                                                                                                    slow_config = [
                                                                                                                        flag for flag in second_pivot_config
                                                                                                                        if flag not in ('-DFOLLOW_DUTY_LIMIT_PCT=27',
                                                                                                                                        '-DFOLLOW_DUTY_HALF_STEP=1')
                                                                                                                    ]
                                                                                                                    slow_config.extend(['-DFOLLOW_DUTY_LIMIT_PCT=25',
                                                                                                                                        '-DFOLLOW_RED_SECOND_SLOW=1'])
                                                                                                                    cases.extend([
                                                                                                                        ('red-second-slow-owner', owner_files, slow_config),
                                                                                                                        ('red-second-slow-startup',
                                                                                                                         ['testing/host/test_follow_startup.c'], slow_config),
                                                                                                                    ])
                                                                                                                    if args.red_bend_after_handoff:
                                                                                                                        handoff_bend_config = [*slow_config, '-DFOLLOW_RED_BEND_AFTER_HANDOFF=1']
                                                                                                                        cases.extend([
                                                                                                                            ('red-bend-after-handoff-owner', owner_files, handoff_bend_config),
                                                                                                                            ('red-bend-after-handoff-startup',
                                                                                                                             ['testing/host/test_follow_startup.c'], handoff_bend_config),
                                                                                                                        ])
                                                                                                                    if args.red_first_bend_boost:
                                                                                                                        boost_config = [*handoff_bend_config, '-DFOLLOW_RED_FIRST_BEND_BOOST=1']
                                                                                                                        cases.extend([
                                                                                                                            ('red-first-bend-boost-owner', owner_files, boost_config),
                                                                                                                            ('red-first-bend-boost-startup',
                                                                                                                             ['testing/host/test_follow_startup.c'], boost_config),
                                                                                                                        ])
                                                                                                                        if args.red_first_bend_tighter:
                                                                                                                            tight_config = [*boost_config, '-DFOLLOW_RED_FIRST_BEND_TIGHTER=1']
                                                                                                                            cases.extend([
                                                                                                                                ('red-first-bend-tighter-owner', owner_files, tight_config),
                                                                                                                                ('red-first-bend-tighter-startup',
                                                                                                                                 ['testing/host/test_follow_startup.c'], tight_config),
                                                                                                                            ])
                                                                                                                            if args.blue_red_join_refinement:
                                                                                                                                join_refine_config = [*tight_config, '-DFOLLOW_BLUE_RED_JOIN_REFINEMENT=1']
                                                                                                                                cases.extend([
                                                                                                                                    ('blue-red-join-refinement-owner', owner_files, join_refine_config),
                                                                                                                                    ('blue-red-join-refinement-startup',
                                                                                                                                     ['testing/host/test_follow_startup.c'], join_refine_config),
                                                                                                                                ])
                                                                                                                                if args.bend_cue_refinement:
                                                                                                                                    bend_refine_config = [*join_refine_config, '-DFOLLOW_BEND_CUE_REFINEMENT=1']
                                                                                                                                    cases.extend([
                                                                                                                                        ('bend-cue-refinement-owner', owner_files, bend_refine_config),
                                                                                                                                        ('bend-cue-refinement-startup',
                                                                                                                                         ['testing/host/test_follow_startup.c'], bend_refine_config),
                                                                                                                                    ])
                                                                                                                                    if args.red_second_exit_refinement:
                                                                                                                                        red_second_config = [*bend_refine_config, '-DFOLLOW_RED_SECOND_EXIT_REFINEMENT=1']
                                                                                                                                        cases.extend([
                                                                                                                                            ('red-second-exit-refinement-owner', owner_files, red_second_config),
                                                                                                                                            ('red-second-exit-refinement-startup',
                                                                                                                                             ['testing/host/test_follow_startup.c'], red_second_config),
                                                                                                                                        ])
                                                                                                                                        if args.blue_diagonal_exit:
                                                                                                                                            blue_exit_config = [*red_second_config, '-DFOLLOW_BLUE_DIAGONAL_EXIT=1']
                                                                                                                                            cases.extend([
                                                                                                                                                ('blue-diagonal-exit-owner', owner_files, blue_exit_config),
                                                                                                                                                ('blue-diagonal-exit-startup',
                                                                                                                                                 ['testing/host/test_follow_startup.c'], blue_exit_config),
                                                                                                                                            ])
                                                                                                                                            if args.red_handoff_align:
                                                                                                                                                red_align_config = [*blue_exit_config, '-DFOLLOW_RED_HANDOFF_ALIGN=1']
                                                                                                                                                cases.extend([
                                                                                                                                                    ('red-handoff-align-owner', owner_files, red_align_config),
                                                                                                                                                    ('red-handoff-align-startup',
                                                                                                                                                     ['testing/host/test_follow_startup.c'], red_align_config),
                                                                                                                                                ])
                                                                                                                                                if args.red_exit_align:
                                                                                                                                                    red_exit_align_config = [*red_align_config, '-DFOLLOW_RED_EXIT_ALIGN=1']
                                                                                                                                                    cases.extend([
                                                                                                                                                        ('red-exit-align-owner', owner_files, red_exit_align_config),
                                                                                                                                                        ('red-exit-align-startup',
                                                                                                                                                         ['testing/host/test_follow_startup.c'], red_exit_align_config),
                                                                                                                                                    ])
                                                                                                                                                    if args.red_between_bends:
                                                                                                                                                        red_between_config = [*red_exit_align_config, '-DFOLLOW_RED_BETWEEN_BENDS=1']
                                                                                                                                                        cases.extend([
                                                                                                                                                            ('red-between-bends-owner', owner_files, red_between_config),
                                                                                                                                                            ('red-between-bends-startup',
                                                                                                                                                             ['testing/host/test_follow_startup.c'], red_between_config),
                                                                                                                                                        ])
                                                                                                                                                        if args.red_inside_release:
                                                                                                                                                            red_release_config = [*red_between_config, '-DFOLLOW_RED_INSIDE_RELEASE=1']
                                                                                                                                                            cases.extend([
                                                                                                                                                                ('red-inside-release-owner', owner_files, red_release_config),
                                                                                                                                                                ('red-inside-release-startup',
                                                                                                                                                                 ['testing/host/test_follow_startup.c'], red_release_config),
                                                                                                                                                            ])
                                                                                                                                                            if args.red_exit_turn:
                                                                                                                                                                red_exit_turn_config = [*red_release_config, '-DFOLLOW_RED_EXIT_TURN=1']
                                                                                                                                                                cases.extend([
                                                                                                                                                                    ('red-exit-turn-owner', owner_files, red_exit_turn_config),
                                                                                                                                                                    ('red-exit-turn-startup',
                                                                                                                                                                     ['testing/host/test_follow_startup.c'], red_exit_turn_config),
                                                                                                                                                                ])
                                                                                                                                                                if args.red_exit_turn_early:
                                                                                                                                                                    red_exit_early_config = [*red_exit_turn_config, '-DFOLLOW_RED_EXIT_TURN_EARLY=1']
                                                                                                                                                                    cases.extend([
                                                                                                                                                                        ('red-exit-turn-early-owner', owner_files, red_exit_early_config),
                                                                                                                                                                        ('red-exit-turn-early-startup',
                                                                                                                                                                         ['testing/host/test_follow_startup.c'], red_exit_early_config),
                                                                                                                                                                    ])
                                                                                                                                                                    if args.red_exit_turn_tighter:
                                                                                                                                                                        red_exit_tight_config = [*red_exit_early_config, '-DFOLLOW_RED_EXIT_TURN_TIGHTER=1']
                                                                                                                                                                        cases.extend([
                                                                                                                                                                            ('red-exit-turn-tighter-owner', owner_files, red_exit_tight_config),
                                                                                                                                                                            ('red-exit-turn-tighter-startup',
                                                                                                                                                                             ['testing/host/test_follow_startup.c'], red_exit_tight_config),
                                                                                                                                                                        ])
                                                                                                                                                                        if args.red_return_grace:
                                                                                                                                                                            red_return_config = [*red_exit_tight_config, '-DFOLLOW_RED_RETURN_GRACE=1']
                                                                                                                                                                            cases.extend([
                                                                                                                                                                                ('red-return-grace-owner', owner_files, red_return_config),
                                                                                                                                                                                ('red-return-grace-startup',
                                                                                                                                                                                 ['testing/host/test_follow_startup.c'], red_return_config),
                                                                                                                                                                            ])
                                                                                                                                                                            if args.red_exit_turn_strong:
                                                                                                                                                                                red_exit_strong_config = [*red_return_config, '-DFOLLOW_RED_EXIT_TURN_STRONG=1']
                                                                                                                                                                                cases.extend([
                                                                                                                                                                                    ('red-exit-turn-strong-owner', owner_files, red_exit_strong_config),
                                                                                                                                                                                    ('red-exit-turn-strong-startup',
                                                                                                                                                                                     ['testing/host/test_follow_startup.c'], red_exit_strong_config),
                                                                                                                                                                                ])
                                                                                                                                                                                if args.red_exit_turn_hold:
                                                                                                                                                                                    red_exit_hold_config = [*red_exit_strong_config, '-DFOLLOW_RED_EXIT_TURN_HOLD=1']
                                                                                                                                                                                    cases.extend([
                                                                                                                                                                                        ('red-exit-turn-hold-owner', owner_files, red_exit_hold_config),
                                                                                                                                                                                        ('red-exit-turn-hold-startup',
                                                                                                                                                                                         ['testing/host/test_follow_startup.c'], red_exit_hold_config),
                                                                                                                                                                                    ])
                                                                                                                                                                                    if args.blue_exit_pivot:
                                                                                                                                                                                        blue_exit_pivot_config = [*red_exit_hold_config, '-DFOLLOW_BLUE_EXIT_PIVOT=1']
                                                                                                                                                                                        cases.extend([
                                                                                                                                                                                            ('blue-exit-pivot-owner', owner_files, blue_exit_pivot_config),
                                                                                                                                                                                            ('blue-exit-pivot-startup',
                                                                                                                                                                                             ['testing/host/test_follow_startup.c'], blue_exit_pivot_config),
                                                                                                                                                                                        ])
            if args.blue_red_late_confirm:
                late_confirm_config = [*blue_exit_pivot_config, '-DFOLLOW_BLUE_RED_LATE_CONFIRM=1']
                cases.extend([
                    ('blue-red-late-confirm-owner', owner_files, late_confirm_config),
                    ('blue-red-late-confirm-startup',
                     ['testing/host/test_follow_startup.c'], late_confirm_config),
                ])
            if args.blue_red_blend_grace:
                blend_grace_config = [*late_confirm_config, '-DFOLLOW_BLUE_RED_BLEND_GRACE=1']
                cases.extend([
                    ('blue-red-blend-grace-owner', owner_files, blend_grace_config),
                    ('blue-red-blend-grace-startup',
                     ['testing/host/test_follow_startup.c'], blend_grace_config),
                ])
            if args.blue_red_blend_margin:
                blend_margin_config = [*blend_grace_config, '-DFOLLOW_BLUE_RED_BLEND_MARGIN=1']
                cases.extend([
                    ('blue-red-blend-margin-owner', owner_files, blend_margin_config),
                    ('blue-red-blend-margin-startup',
                     ['testing/host/test_follow_startup.c'], blend_margin_config),
                ])
            if args.blue_exit_left_boost:
                left_boost_config = [*blend_margin_config, '-DFOLLOW_BLUE_EXIT_LEFT_BOOST=1']
                cases.extend([
                    ('blue-exit-left-boost-owner', owner_files, left_boost_config),
                    ('blue-exit-left-boost-startup',
                     ['testing/host/test_follow_startup.c'], left_boost_config),
                ])
            if args.red_first_left_boost:
                red_first_left_config = [*left_boost_config, '-DFOLLOW_RED_FIRST_LEFT_BOOST=1']
                cases.extend([
                    ('red-first-left-boost-owner', owner_files, red_first_left_config),
                    ('red-first-left-boost-startup',
                     ['testing/host/test_follow_startup.c'], red_first_left_config),
                ])
            if args.red_second_arc_boost:
                red_second_arc_config = [*red_first_left_config, '-DFOLLOW_RED_SECOND_ARC_BOOST=1']
                cases.extend([
                    ('red-second-arc-boost-owner', owner_files, red_second_arc_config),
                    ('red-second-arc-boost-startup',
                     ['testing/host/test_follow_startup.c'], red_second_arc_config),
                ])
            if args.blue_second_left_boost:
                bend_power_config = [*red_second_arc_config, '-DFOLLOW_BLUE_SECOND_LEFT_BOOST=1']
                cases.extend([
                    ('bend-power-owner', owner_files, bend_power_config),
                    ('bend-power-startup', ['testing/host/test_follow_startup.c'], bend_power_config),
                ])
            if args.red_final_left_boost:
                red_final_left_config = [*bend_power_config, '-DFOLLOW_RED_FINAL_LEFT_BOOST=1']
                cases.extend([
                    ('red-final-left-boost-owner', owner_files, red_final_left_config),
                    ('red-final-left-boost-startup', ['testing/host/test_follow_startup.c'], red_final_left_config),
                ])
            if args.blue_exit_edge_confirm:
                edge_confirm_config = [*red_final_left_config, '-DFOLLOW_BLUE_EXIT_EDGE_CONFIRM=1']
                cases.extend([
                    ('blue-exit-edge-confirm-owner', owner_files, edge_confirm_config),
                    ('blue-exit-edge-confirm-startup', ['testing/host/test_follow_startup.c'], edge_confirm_config),
                ])
            if args.blue_red_late_approach:
                late_approach_config = [*edge_confirm_config, '-DFOLLOW_BLUE_RED_LATE_APPROACH=1']
                cases.extend([
                    ('blue-red-late-approach-owner', owner_files, late_approach_config),
                    ('blue-red-late-approach-startup', ['testing/host/test_follow_startup.c'], late_approach_config),
                ])
            if args.red_diagonal_reacquire:
                red_diagonal_config = [*late_approach_config, '-DFOLLOW_RED_DIAGONAL_REACQUIRE=1']
                cases.extend([
                    ('red-diagonal-reacquire-owner', owner_files, red_diagonal_config),
                    ('red-diagonal-reacquire-startup', ['testing/host/test_follow_startup.c'], red_diagonal_config),
                ])
            if args.red_final_arc_extend:
                red_final_arc_config = [*red_diagonal_config, '-DFOLLOW_RED_FINAL_ARC_EXTEND=1']
                cases.extend([
                    ('red-final-arc-extend-owner', owner_files, red_final_arc_config),
                    ('red-final-arc-extend-startup', ['testing/host/test_follow_startup.c'], red_final_arc_config),
                ])
            if args.red_final_arc_strong:
                red_arc_strong_config = [*red_final_arc_config, '-DFOLLOW_RED_FINAL_ARC_STRONG=1']
                cases.extend([
                    ('red-final-arc-strong-owner', owner_files, red_arc_strong_config),
                    ('red-final-arc-strong-startup', ['testing/host/test_follow_startup.c'], red_arc_strong_config),
                ])
            if args.blue_red_join_extend:
                join_extend_config = [*red_arc_strong_config, '-DFOLLOW_BLUE_RED_JOIN_EXTEND=1']
                cases.extend([
                    ('blue-red-join-extend-owner', owner_files, join_extend_config),
                    ('blue-red-join-extend-startup', ['testing/host/test_follow_startup.c'], join_extend_config),
                ])
            if args.red_pivot_inside_release:
                inside_release_config = [*join_extend_config, '-DFOLLOW_RED_PIVOT_INSIDE_RELEASE=1']
                cases.extend([
                    ('red-pivot-inside-release-owner', owner_files, inside_release_config),
                    ('red-pivot-inside-release-startup', ['testing/host/test_follow_startup.c'], inside_release_config),
                ])
            if args.red_outside_recovery:
                outside_config = [*inside_release_config, '-DFOLLOW_RED_OUTSIDE_RECOVERY=1']
                cases.extend([
                    ('red-outside-recovery-owner', owner_files, outside_config),
                    ('red-outside-recovery-startup', ['testing/host/test_follow_startup.c'], outside_config),
                ])
            if args.red_right_edge_outside:
                right_edge_config = [*outside_config, '-DFOLLOW_RED_RIGHT_EDGE_OUTSIDE=1']
                cases.extend([
                    ('red-right-edge-outside-owner', owner_files, right_edge_config),
                    ('red-right-edge-outside-startup', ['testing/host/test_follow_startup.c'], right_edge_config),
                ])
            if args.red_corner_refine:
                corner_refine_config = [*right_edge_config, '-DFOLLOW_RED_CORNER_REFINE=1']
                cases.extend([
                    ('red-corner-refine-owner', owner_files, corner_refine_config),
                    ('red-corner-refine-startup', ['testing/host/test_follow_startup.c'], corner_refine_config),
                ])
            if args.red_spin_hold:
                spin_hold_config = [*corner_refine_config, '-DFOLLOW_RED_SPIN_HOLD=1']
                cases.extend([
                    ('red-spin-hold-owner', owner_files, spin_hold_config),
                    ('red-spin-hold-startup', ['testing/host/test_follow_startup.c'], spin_hold_config),
                    # The MQTT follow image reuses this v0.87 steering with a remote
                    # start; its STRAIGHT mode must bypass every bend manoeuvre.
                    ('straight-follow', ['testing/host/test_straight_follow.c', *core],
                     [*spin_hold_config, '-DAAI05_FOLLOW_MQTT=1']),
                ])
            core_files = next(files for name,files,extra in cases if name=='core')
            cases.append(('core-turn20',core_files,['-DFOLLOW_TURN_LIMIT_PCT=20']))
            cases.append(('owner-turn20',owner_files,['-DFOLLOW_TURN_LIMIT_PCT=20']))
            cases.append(('owner-original50-turn20',owner_files,
                          ['-DFOLLOW_DUTY_LIMIT_PCT=50','-DFOLLOW_TURN_LIMIT_PCT=20']))
            for name, files, extra in cases:
                exe = str(Path(scratch)/name)
                run(['gcc', *strict, '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                     '-Iapp_program/subsystem2','-Iapp_program/subsystem1','-Iinclude',
                     '-I'+scratch,*extra,*files,'-o',exe], name+'-compile')
                run([exe], name)
            if regression_duty == 27.5:
                for name, extra in [('motor-pulse', []), ('motor-probe', ['-DMOTOR_TEST_PROBE=1'])]:
                    exe = str(Path(scratch)/name)
                    run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                         '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                         '-Iapp_program/subsystem2', *extra, 'testing/host/test_motor_pulse.c',
                         '-o', exe], name+'-compile')
                    run([exe], name)
            run(['arm-none-eabi-gcc', *strict, '-mcpu=cortex-m0plus','-mthumb',
                 '-ffreestanding','-c',core[0],'-o',str(Path(scratch)/'follow.o')],
                'core-target')
        if args.firmware:
            if not args.sdk:
                raise RuntimeError('--firmware requires --sdk')
            sdk = Path(args.sdk).resolve()
            alias = Path('/tmp/aai05-follow-sdk')
            if alias.is_symlink():
                if alias.resolve() != sdk:
                    raise RuntimeError('SDK alias mismatch')
            elif alias.exists():
                raise RuntimeError('SDK alias occupied')
            else:
                alias.symlink_to(sdk, target_is_directory=True)
            env['PICO_SDK_PATH'] = str(alias)
            def track_flags(track):
                return [f'FOLLOW_{colour.upper()}={int(track == colour)}'
                        for colour in ('yellow', 'red', 'blue', 'multi')]

            flags = ['SMP=1','CONSOLE=usb_cdc','WIFI=cyw43','NET=none','MQTT=0',
                     'FOLLOW_APP=1','MOTION_APP=0','REMOTE_APP=0','COMM_APP=0',
                     'RGB_DIAG=0','ENCODER_PIO=1','CRASH_REPORT=0',
                     'FOLLOW_DUTY='+str(25 if args.duty == 27.5 else args.duty),'FOLLOW_TURN='+str(args.turn),
                     *track_flags('black'),'FOLLOW_CORNER=0','FOLLOW_CLIPPED=0','FOLLOW_TWO_TURN=0',
                     f'RGB_HOME={int(args.home_rgb)}']
            for name, bad in [('network',['NET=lwip','MQTT=1']),
                              ('motion',['MOTION_APP=1']),('rgbdiag',['RGB_DIAG=1']),
                              ('pio',['ENCODER_PIO=0']),('single',['SMP=0']),
                              ('duty',['FOLLOW_DUTY=60']),('turn',['FOLLOW_TURN=50']),
                              ('geometry',['FOLLOW_YELLOW=2']),
                              ('orphan-yellow',['FOLLOW_YELLOW=1','FOLLOW_APP=0']),
                              ('red-geometry',['FOLLOW_RED=2']),
                              ('orphan-red',['FOLLOW_RED=1','FOLLOW_APP=0']),
                              ('mixed-colour',['FOLLOW_RED=1','FOLLOW_YELLOW=1']),
                              ('blue-geometry',['FOLLOW_BLUE=2']),
                              ('orphan-blue',['FOLLOW_BLUE=1','FOLLOW_APP=0']),
                              ('mix-blue-red',['FOLLOW_BLUE=1','FOLLOW_RED=1']),
                              ('mix-blue-yellow',['FOLLOW_BLUE=1','FOLLOW_YELLOW=1']),
                              ('multi-geometry',['FOLLOW_MULTI=2']),
                              ('orphan-multi',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_APP=0']),
                              ('multi-duty',['FOLLOW_MULTI=1','FOLLOW_DUTY=35']),
                              ('corner-direction',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CORNER=2']),
                              ('orphan-corner',['FOLLOW_CORNER=1']),
                              ('clipped-value',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CLIPPED=2']),
                              ('orphan-clipped',['FOLLOW_CLIPPED=1']),
                              ('clipped-pivot',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CLIPPED=1',
                                                'FOLLOW_CORNER=1']),
                              ('clipped-turn',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CLIPPED=1',
                                               'FOLLOW_TURN=20']),
                              ('orphan-blue-bend',['FOLLOW_BLUE_BEND_RIGHT=1']),
                              ('blue-bend-value',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CLIPPED=1',
                                                  'FOLLOW_BLUE_BEND_RIGHT=2']),
                              ('orphan-loop-recovery',['FOLLOW_LOOP_RECOVERY_DIRECTION=1']),
                              ('loop-recovery-value',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CLIPPED=1',
                                                      'FOLLOW_LOOP_RECOVERY_DIRECTION=2']),
                              ('loop-recovery-blue-bend',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CLIPPED=1',
                                                         'FOLLOW_BLUE_BEND_RIGHT=1',
                                                         'FOLLOW_LOOP_RECOVERY_DIRECTION=1']),
                              ('orphan-early-bend',['FOLLOW_EARLY_BEND=1']),
                              ('early-bend-value',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CLIPPED=1',
                                                   'FOLLOW_LOOP_RECOVERY_DIRECTION=1','FOLLOW_EARLY_BEND=2']),
                              ('early-bend-reverse',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CLIPPED=1',
                                                     'FOLLOW_LOOP_RECOVERY_DIRECTION=-1','FOLLOW_EARLY_BEND=1']),
                              ('orphan-home-track',['FOLLOW_HOME_TRACK=1']),
                              ('home-track-no-rgb-home',['FOLLOW_MULTI=1','FOLLOW_DUTY=25','FOLLOW_CLIPPED=1',
                                                         'FOLLOW_LOOP_RECOVERY_DIRECTION=1','FOLLOW_EARLY_BEND=1',
                                                         'FOLLOW_HOME_TRACK=1','RGB_HOME=0']),
                              ('orphan-blue-steer-ramp',['FOLLOW_BLUE_STEER_RAMP=1']),
                              ('orphan-blue-ramp-start',['FOLLOW_BLUE_RAMP_START=5']),
                              ('blue-ramp-start-value',['FOLLOW_BLUE_RAMP_START=14']),
                              ('orphan-two-turn',['FOLLOW_TWO_TURN=1']),
                              ('two-turn-value',['FOLLOW_TWO_TURN=2']),
                              ('multi-30-without-supported-profile',['FOLLOW_MULTI=1','FOLLOW_DUTY=30']),
                              ('orphan-half-duty',['FOLLOW_DUTY=27.5']),
                              ('orphan-yellow-exit-cue',['FOLLOW_YELLOW_EXIT_CUE=1']),
                              ('orphan-yellow-lead-cue',['FOLLOW_YELLOW_LEAD_CUE=1']),
                              ('orphan-blue-reversal-damping',['FOLLOW_BLUE_REVERSAL_DAMPING=1']),
                              ('orphan-blue-steer-wide-range',['FOLLOW_BLUE_STEER_WIDE_RANGE=1']),
                              ('orphan-stop-trace',['FOLLOW_STOP_TRACE=1']),
                              ('orphan-blue-red-handoff',['FOLLOW_BLUE_RED_HANDOFF=1']),
                              ('orphan-blue-second-boost',['FOLLOW_BLUE_SECOND_BOOST=1']),
                              ('orphan-blue-outer-edge',['FOLLOW_BLUE_OUTER_EDGE=1']),
                              ('orphan-blue-outer-recovery',['FOLLOW_BLUE_OUTER_RECOVERY=1']),
                              ('orphan-blue-bend-slow',['FOLLOW_BLUE_BEND_SLOW=1']),
                              ('orphan-blue-confirm-grace',['FOLLOW_BLUE_CONFIRM_GRACE=1']),
                              ('orphan-bend-trace',['FOLLOW_BEND_TRACE=1']),
                              ('orphan-blue-exit-lead',['FOLLOW_BLUE_EXIT_LEAD=1']),
                              ('orphan-blue-exit-tight',['FOLLOW_BLUE_EXIT_TURN=18']),
                              ('orphan-blue-exit-early',['FOLLOW_BLUE_EXIT_TURN=22']),
                              ('orphan-blue-red-confirm-grace',['FOLLOW_BLUE_RED_CONFIRM_GRACE=1']),
                              ('orphan-blue-exit-after-recovery',['FOLLOW_BLUE_EXIT_AFTER_RECOVERY=1']),
                              ('orphan-red-exit-lead',['FOLLOW_RED_EXIT_LEAD=1']),
                              ('orphan-red-exit-early',['FOLLOW_RED_EXIT_EARLY=1']),
                              ('orphan-blue-red-bend-trial',['FOLLOW_BLUE_RED_BEND_TRIAL=1']),
                              ('orphan-blue-red-bend-cues',['FOLLOW_BLUE_RED_BEND_CUES=1']),
                              ('orphan-red-second-pivot',['FOLLOW_RED_SECOND_PIVOT=1']),
                              ('orphan-red-second-slow',['FOLLOW_RED_SECOND_SLOW=1']),
                              ('orphan-red-bend-after-handoff',['FOLLOW_RED_BEND_AFTER_HANDOFF=1']),
                              ('orphan-red-first-bend-boost',['FOLLOW_RED_FIRST_BEND_BOOST=1']),
                              ('orphan-red-first-bend-tighter',['FOLLOW_RED_FIRST_BEND_TIGHTER=1']),
                              ('orphan-blue-red-join-refinement',['FOLLOW_BLUE_RED_JOIN_REFINEMENT=1']),
                              ('orphan-bend-cue-refinement',['FOLLOW_BEND_CUE_REFINEMENT=1']),
                              ('orphan-red-second-exit-refinement',['FOLLOW_RED_SECOND_EXIT_REFINEMENT=1']),
                              ('orphan-blue-diagonal-exit',['FOLLOW_BLUE_DIAGONAL_EXIT=1']),
                              ('orphan-red-handoff-align',['FOLLOW_RED_HANDOFF_ALIGN=1']),
                              ('orphan-red-exit-align',['FOLLOW_RED_EXIT_ALIGN=1']),
                              ('orphan-red-between-bends',['FOLLOW_RED_BETWEEN_BENDS=1']),
                              ('orphan-red-inside-release',['FOLLOW_RED_INSIDE_RELEASE=1']),
                              ('orphan-red-exit-turn',['FOLLOW_RED_EXIT_TURN=1']),
                              ('orphan-red-exit-turn-early',['FOLLOW_RED_EXIT_TURN_EARLY=1']),
                              ('orphan-red-exit-turn-tighter',['FOLLOW_RED_EXIT_TURN_TIGHTER=1']),
                              ('orphan-red-return-grace',['FOLLOW_RED_RETURN_GRACE=1']),
                              ('orphan-red-exit-turn-strong',['FOLLOW_RED_EXIT_TURN_STRONG=1']),
                              ('orphan-red-exit-turn-hold',['FOLLOW_RED_EXIT_TURN_HOLD=1']),
                              ('orphan-blue-exit-pivot',['FOLLOW_BLUE_EXIT_PIVOT=1']),
                              ('orphan-blue-red-late-confirm',['FOLLOW_BLUE_RED_LATE_CONFIRM=1']),
                              ('orphan-blue-red-blend-grace',['FOLLOW_BLUE_RED_BLEND_GRACE=1']),
                              ('orphan-blue-red-blend-margin',['FOLLOW_BLUE_RED_BLEND_MARGIN=1']),
                              ('orphan-blue-exit-left-boost',['FOLLOW_BLUE_EXIT_LEFT_BOOST=1']),
                              ('orphan-red-first-left-boost',['FOLLOW_RED_FIRST_LEFT_BOOST=1']),
                              ('orphan-red-second-arc-boost',['FOLLOW_RED_SECOND_ARC_BOOST=1']),
                              ('orphan-blue-second-left-boost',['FOLLOW_BLUE_SECOND_LEFT_BOOST=1']),
                              ('orphan-red-final-left-boost',['FOLLOW_RED_FINAL_LEFT_BOOST=1']),
                              ('orphan-blue-exit-edge-confirm',['FOLLOW_BLUE_EXIT_EDGE_CONFIRM=1']),
                              ('orphan-blue-red-late-approach',['FOLLOW_BLUE_RED_LATE_APPROACH=1']),
                              ('orphan-red-diagonal-reacquire',['FOLLOW_RED_DIAGONAL_REACQUIRE=1']),
                              ('orphan-red-final-arc-extend',['FOLLOW_RED_FINAL_ARC_EXTEND=1']),
                              ('orphan-red-final-arc-strong',['FOLLOW_RED_FINAL_ARC_STRONG=1']),
                              ('orphan-blue-red-join-extend',['FOLLOW_BLUE_RED_JOIN_EXTEND=1']),
                              ('orphan-red-pivot-inside-release',['FOLLOW_RED_PIVOT_INSIDE_RELEASE=1']),
                              ('orphan-red-outside-recovery',['FOLLOW_RED_OUTSIDE_RECOVERY=1']),
                              ('orphan-red-right-edge-outside',['FOLLOW_RED_RIGHT_EDGE_OUTSIDE=1']),
                              ('orphan-red-corner-refine',['FOLLOW_RED_CORNER_REFINE=1']),
                              ('orphan-red-spin-hold',['FOLLOW_RED_SPIN_HOLD=1']),
                              ('rgb-profile',['RGB_HOME=2']),
                              *[(f'mix-multi-{track}',
                                 ['FOLLOW_MULTI=1','FOLLOW_DUTY=25',f'FOLLOW_{track.upper()}=1'])
                                for track in ('red', 'yellow', 'blue')]]:
                run(['make','-n',*flags,*bad], 'reject-'+name, repo/'build_make', True)
            candidate = artifacts/(args.track+'_follow_'+args.label)
            candidate.mkdir(exist_ok=False)
            evidence['artifacts'] = []
            suffix = f'D{args.duty}_T{args.turn}'
            versions = {'black': 'Black_Follow_v0_13_3_', 'yellow': 'Yellow_Follow_v0_14_',
                        'red': 'Red_Follow_v0_15_', 'blue': 'Blue_Follow_v0_16_',
                        'multi': 'Multicolour_Follow_v0_18_'}
            version = versions[args.track] + suffix
            builds = [(version+'_NO_DRIVE',1,0,args.track,[]), (version,0,0,args.track,[])]
            if args.early_bend:
                profile = f'Early_Bend_Clockwise_v0_24_D{args.duty}_HOME'
                extra = ['FOLLOW_CLIPPED=1', 'FOLLOW_LOOP_RECOVERY_DIRECTION=1', 'FOLLOW_EARLY_BEND=1']
                if args.home_track:
                    profile = f'Home_Track_Clockwise_v0_25_D{args.duty}'
                    extra.append('FOLLOW_HOME_TRACK=1')
                if args.blue_steer_ramp:
                    profile = f'Home_Track_Blue_Ramp_v0_26_D{args.duty}'
                    extra.append('FOLLOW_BLUE_STEER_RAMP=1')
                    if args.blue_ramp_start == 5:
                        duty_suffix = str(args.duty).replace('.', 'p')
                        profile = f'Home_Track_Blue_Gentle_v0_27_D{duty_suffix}'
                        extra.append('FOLLOW_BLUE_RAMP_START=5')
                        if args.duty == 27.5:
                            extra.append('FOLLOW_DUTY=27.5')
                if args.two_turn:
                    profile = f'Home_Track_Two_Turn_v0_28_D{args.duty}'
                    extra.append('FOLLOW_TWO_TURN=1')
                if args.yellow_exit_cue:
                    profile = 'Home_Track_Yellow_Approach_v0_29_D27p5'
                    extra.append('FOLLOW_YELLOW_EXIT_CUE=1')
                if args.yellow_lead_cue:
                    profile = 'Home_Track_Yellow_Lead_v0_30_D27p5'
                    extra.append('FOLLOW_YELLOW_LEAD_CUE=1')
                if args.blue_reversal_damping:
                    profile = 'Home_Track_Blue_Damped_v0_31_D27p5'
                    extra.append('FOLLOW_BLUE_REVERSAL_DAMPING=1')
                if args.blue_steer_wide_range:
                    profile = 'Home_Track_Blue_Wide_Range_v0_32_D27p5'
                    extra.append('FOLLOW_BLUE_STEER_WIDE_RANGE=1')
                if args.stop_trace:
                    profile = 'Home_Track_Stop_Trace_v0_33_D27p5'
                    extra.append('FOLLOW_STOP_TRACE=1')
                if args.blue_red_handoff:
                    profile = 'Home_Track_Blue_Red_Handoff_v0_34_D27p5'
                    extra.append('FOLLOW_BLUE_RED_HANDOFF=1')
                if args.blue_second_boost:
                    profile = 'Home_Track_Blue_Second_Boost_v0_35_D27p5'
                    extra.append('FOLLOW_BLUE_SECOND_BOOST=1')
                if args.blue_outer_edge:
                    profile = 'Home_Track_Blue_Outer_Edge_v0_36_D27p5'
                    extra.append('FOLLOW_BLUE_OUTER_EDGE=1')
                if args.blue_outer_recovery:
                    profile = 'Home_Track_Blue_Outer_Recovery_v0_37_D27p5'
                    extra.append('FOLLOW_BLUE_OUTER_RECOVERY=1')
                if args.blue_bend_slow:
                    profile = 'Home_Track_Blue_Bend_Slow_v0_38_D27p5'
                    extra.append('FOLLOW_BLUE_BEND_SLOW=1')
                if args.blue_confirm_grace:
                    profile = 'Home_Track_Blue_Confirm_v0_39_D27p5'
                    extra.append('FOLLOW_BLUE_CONFIRM_GRACE=1')
                if args.bend_trace:
                    profile = 'Home_Track_Bend_Trace_v0_40_D27p5'
                    extra.append('FOLLOW_BEND_TRACE=1')
                if args.blue_exit_lead:
                    profile = 'Home_Track_Blue_Exit_Lead_v0_41_D27p5'
                    extra.append('FOLLOW_BLUE_EXIT_LEAD=1')
                if args.blue_exit_early:
                    profile = 'Home_Track_Blue_Exit_Early_v0_43_D27p5'
                    extra.append('FOLLOW_BLUE_EXIT_TURN=22')
                elif args.blue_exit_tight:
                    profile = 'Home_Track_Blue_Exit_Tight_v0_42_D27p5'
                    extra.append('FOLLOW_BLUE_EXIT_TURN=18')
                if args.blue_red_confirm_grace:
                    profile = 'Home_Track_Red_Confirm_v0_44_D27p5'
                    extra.append('FOLLOW_BLUE_RED_CONFIRM_GRACE=1')
                if args.blue_exit_after_recovery:
                    profile = 'Home_Track_Blue_Exit_After_Recovery_v0_45_D27p5'
                    extra.append('FOLLOW_BLUE_EXIT_AFTER_RECOVERY=1')
                if args.red_exit_lead:
                    profile = 'Home_Track_Red_Exit_Lead_v0_46_D27p5'
                    extra.append('FOLLOW_RED_EXIT_LEAD=1')
                if args.red_exit_early:
                    profile = 'Home_Track_Red_Exit_Early_v0_47_D27p5'
                    extra.append('FOLLOW_RED_EXIT_EARLY=1')
                if args.blue_red_bend_trial:
                    profile = 'Home_Track_Blue_Red_Bends_v0_48_D27p5'
                    extra.append('FOLLOW_BLUE_RED_BEND_TRIAL=1')
                if args.blue_red_bend_cues:
                    profile = 'Home_Track_Blue_Red_Bend_Cues_v0_49_D27p5'
                    extra.append('FOLLOW_BLUE_RED_BEND_CUES=1')
                if args.red_second_pivot:
                    profile = 'Home_Track_Red_Second_Pivot_v0_50_D27p5'
                    extra.append('FOLLOW_RED_SECOND_PIVOT=1')
                if args.red_second_slow:
                    profile = 'Home_Track_Red_Second_Slow_v0_51_D25'
                    extra.append('FOLLOW_RED_SECOND_SLOW=1')
                if args.red_bend_after_handoff:
                    profile = 'Home_Track_Red_Bend_After_Handoff_v0_52_D25'
                    extra.append('FOLLOW_RED_BEND_AFTER_HANDOFF=1')
                if args.red_first_bend_boost:
                    profile = 'Home_Track_Red_First_Bend_Boost_v0_53_D25'
                    extra.append('FOLLOW_RED_FIRST_BEND_BOOST=1')
                if args.red_first_bend_tighter:
                    profile = 'Home_Track_Red_First_Bend_Tighter_v0_54_D25'
                    extra.append('FOLLOW_RED_FIRST_BEND_TIGHTER=1')
                if args.blue_red_join_refinement:
                    profile = 'Home_Track_Blue_Red_Join_Refinement_v0_55_D25'
                    extra.append('FOLLOW_BLUE_RED_JOIN_REFINEMENT=1')
                if args.bend_cue_refinement:
                    profile = 'Home_Track_Bend_Cue_Refinement_v0_56_D25'
                    extra.append('FOLLOW_BEND_CUE_REFINEMENT=1')
                if args.red_second_exit_refinement:
                    profile = 'Home_Track_Red_Second_Exit_Refinement_v0_57_D25'
                    extra.append('FOLLOW_RED_SECOND_EXIT_REFINEMENT=1')
                if args.blue_diagonal_exit:
                    profile = 'Home_Track_Blue_Diagonal_Exit_v0_58_D25'
                    extra.append('FOLLOW_BLUE_DIAGONAL_EXIT=1')
                if args.red_handoff_align:
                    profile = 'Home_Track_Red_Handoff_Align_v0_59_D25'
                    extra.append('FOLLOW_RED_HANDOFF_ALIGN=1')
                if args.red_exit_align:
                    profile = 'Home_Track_Red_Exit_Align_v0_60_D25'
                    extra.append('FOLLOW_RED_EXIT_ALIGN=1')
                if args.red_between_bends:
                    profile = 'Home_Track_Red_Between_Bends_v0_61_D25'
                    extra.append('FOLLOW_RED_BETWEEN_BENDS=1')
                if args.red_inside_release:
                    profile = 'Home_Track_Red_Inside_Release_v0_62_D25'
                    extra.append('FOLLOW_RED_INSIDE_RELEASE=1')
                if args.red_exit_turn:
                    profile = 'Home_Track_Red_Exit_Turn_v0_63_D25'
                    extra.append('FOLLOW_RED_EXIT_TURN=1')
                if args.red_exit_turn_early:
                    profile = 'Home_Track_Red_Exit_Turn_Early_v0_64_D25'
                    extra.append('FOLLOW_RED_EXIT_TURN_EARLY=1')
                if args.red_exit_turn_tighter:
                    profile = 'Home_Track_Red_Exit_Turn_Tighter_v0_65_D25'
                    extra.append('FOLLOW_RED_EXIT_TURN_TIGHTER=1')
                if args.red_return_grace:
                    profile = 'Home_Track_Red_Return_Grace_v0_66_D25'
                    extra.append('FOLLOW_RED_RETURN_GRACE=1')
                if args.red_exit_turn_strong:
                    profile = 'Home_Track_Red_Exit_Turn_Strong_v0_67_D25'
                    extra.append('FOLLOW_RED_EXIT_TURN_STRONG=1')
                if args.red_exit_turn_hold:
                    profile = 'Home_Track_Red_Exit_Turn_Hold_v0_68_D25'
                    extra.append('FOLLOW_RED_EXIT_TURN_HOLD=1')
                if args.blue_exit_pivot:
                    profile = 'Home_Track_Blue_Exit_Pivot_v0_69_D25'
                    extra.append('FOLLOW_BLUE_EXIT_PIVOT=1')
                if args.blue_red_late_confirm:
                    profile = 'Home_Track_Blue_Red_Late_Confirm_v0_70_D25'
                    extra.append('FOLLOW_BLUE_RED_LATE_CONFIRM=1')
                if args.blue_red_blend_grace:
                    profile = 'Home_Track_Blue_Red_Blend_Grace_v0_71_D25'
                    extra.append('FOLLOW_BLUE_RED_BLEND_GRACE=1')
                if args.blue_red_blend_margin:
                    profile = 'Home_Track_Blue_Red_Blend_Margin_v0_72_D25'
                    extra.append('FOLLOW_BLUE_RED_BLEND_MARGIN=1')
                if args.blue_exit_left_boost:
                    profile = 'Home_Track_Blue_Exit_Left_Boost_v0_73_D25'
                    extra.append('FOLLOW_BLUE_EXIT_LEFT_BOOST=1')
                if args.red_first_left_boost:
                    profile = 'Home_Track_Red_First_Left_Boost_v0_74_D25'
                    extra.append('FOLLOW_RED_FIRST_LEFT_BOOST=1')
                if args.red_second_arc_boost:
                    profile = 'Home_Track_Red_Second_Arc_Boost_v0_75_D25'
                    extra.append('FOLLOW_RED_SECOND_ARC_BOOST=1')
                if args.blue_second_left_boost:
                    profile = 'Home_Track_Bend_Power_v0_75_D25'
                    extra.append('FOLLOW_BLUE_SECOND_LEFT_BOOST=1')
                if args.red_final_left_boost:
                    profile = 'Home_Track_Red_Final_Left_Boost_v0_76_D25'
                    extra.append('FOLLOW_RED_FINAL_LEFT_BOOST=1')
                if args.blue_exit_edge_confirm:
                    profile = 'Home_Track_Blue_Exit_Edge_Confirm_v0_77_D25'
                    extra.append('FOLLOW_BLUE_EXIT_EDGE_CONFIRM=1')
                if args.blue_red_late_approach:
                    profile = 'Home_Track_Blue_Red_Late_Approach_v0_78_D25'
                    extra.append('FOLLOW_BLUE_RED_LATE_APPROACH=1')
                if args.red_diagonal_reacquire:
                    profile = 'Home_Track_Red_Diagonal_Reacquire_v0_79_D25'
                    extra.append('FOLLOW_RED_DIAGONAL_REACQUIRE=1')
                if args.red_final_arc_extend:
                    profile = 'Home_Track_Red_Final_Arc_Extend_v0_80_D25' + ('_ROOM2' if args.room2_rgb else '')
                    extra.append('FOLLOW_RED_FINAL_ARC_EXTEND=1')
                if args.red_final_arc_strong:
                    profile = 'Home_Track_Red_Final_Arc_Strong_v0_81_D25' + ('_ROOM2' if args.room2_rgb else '')
                    extra.append('FOLLOW_RED_FINAL_ARC_STRONG=1')
                if args.blue_red_join_extend:
                    profile = 'Home_Track_Blue_Red_Join_Extend_v0_82_D25' + ('_ROOM2' if args.room2_rgb else '')
                    extra.append('FOLLOW_BLUE_RED_JOIN_EXTEND=1')
                if args.red_pivot_inside_release:
                    profile = 'Home_Track_Red_Pivot_Inside_Release_v0_83_D25' + ('_ROOM2' if args.room2_rgb else '')
                    extra.append('FOLLOW_RED_PIVOT_INSIDE_RELEASE=1')
                if args.red_outside_recovery:
                    profile = 'Home_Track_Red_Outside_Recovery_v0_84_D25' + ('_ROOM2' if args.room2_rgb else '')
                    extra.append('FOLLOW_RED_OUTSIDE_RECOVERY=1')
                if args.red_right_edge_outside:
                    profile = 'Home_Track_Red_Right_Edge_Outside_v0_85_D25' + ('_ROOM2' if args.room2_rgb else '')
                    extra.append('FOLLOW_RED_RIGHT_EDGE_OUTSIDE=1')
                if args.red_corner_refine:
                    profile = 'Home_Track_Red_Corner_Refine_v0_86_D25' + ('_ROOM2' if args.room2_rgb else '')
                    extra.append('FOLLOW_RED_CORNER_REFINE=1')
                if args.red_spin_hold:
                    profile = 'Home_Track_Red_Spin_Hold_v0_87_D25' + ('_ROOM2' if args.room2_rgb else '')
                    extra.append('FOLLOW_RED_SPIN_HOLD=1')
                builds = [(profile,0,0,'multi',extra)]
            elif args.loop_recovery:
                builds = []
                for direction_name, direction in LOOP_DIRECTIONS:
                    profile = f'Loop_{direction_name}_v0_23_D{args.duty}_HOME'
                    extra = ['FOLLOW_CLIPPED=1', f'FOLLOW_LOOP_RECOVERY_DIRECTION={direction}']
                    for probe, variant in [(1, '_NO_DRIVE'), (0, '')]:
                        builds.append((profile+variant,probe,0,'multi',extra))
            elif args.clipped_loop:
                home = '_HOME' if args.home_rgb else ''
                profile = f'Clipped_Loop_v0_21_D{args.duty}_T{args.turn}to14{home}'
                extra = ['FOLLOW_CLIPPED=1']
                if args.blue_bend_right:
                    profile = f'Blue_Bend_Right_v0_22_D{args.duty}{home}'
                    extra.append('FOLLOW_BLUE_BEND_RIGHT=1')
                builds = [(profile+variant,probe,0,'multi',extra)
                          for probe, variant in [(1, '_NO_DRIVE'), (0, '')]]
            elif args.corners:
                builds = []
                for side, direction in [('Left', -1), ('Right', 1)]:
                    corner = f'Corner_{side}_v0_19_{suffix}'
                    if args.home_rgb:
                        corner += '_HOME'
                    for probe, variant in [(1, '_NO_DRIVE'), (0, '')]:
                        builds.append((corner+variant,probe,0,'multi',[f'FOLLOW_CORNER={direction}']))
            elif args.track != 'multi':
                builds.extend([(version+'_TEST',0,1,args.track,[]),
                               ('Black_Follow_Compatibility',0,0,'black',[])])
                if args.track in ('red', 'blue'):
                    builds.append((versions['yellow']+suffix,0,0,'yellow',[]))
                if args.track == 'blue':
                    builds.append((versions['red']+suffix,0,0,'red',[]))
                builds.extend([
                    ('RGB_Compatibility',1,0,'black',['FOLLOW_APP=0','RGB_DIAG=1','ENCODER_PIO=0']),
                    ('Remote_PIO_Compatibility',0,0,'black',['FOLLOW_APP=0','NET=lwip',
                        'MQTT=1','MOTION_APP=1','REMOTE_APP=1']),
                ])
            for name, probe, test, track, extra in builds:
                stem = 'aai05_'+args.label+'_'+name.lower()
                room2 = ['RGB_ROOM2=1'] if (args.room2_rgb and track == 'multi') else []
                run(['make','-j8',*flags,*track_flags(track),*extra,*room2,'MOTOR_PROBE='+str(probe),
                     'TEST='+str(test),'EXE_FILE='+stem], 'build-'+name, repo/'build_make')
                if args.track == 'blue' and name not in (version, version+'_NO_DRIVE'):
                    continue  # Compile compatibility/TEST variants without publishing them.
                for extension in ('uf2','elf','map'):
                    target=candidate/('AAI05_'+name+'.'+extension)
                    shutil.copyfile(repo/'build_make'/(stem+'.'+extension),target)
                    evidence['artifacts'].append(dict(path=str(target.relative_to(repo)),
                        sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
        evidence['status'] = 'PASS'
    except Exception as error:
        evidence.update(status='FAILED', error=str(error))
        raise
    finally:
        evidence['old_uf2s'] = len(before)
        evidence['old_uf2s_unchanged'] = all(p.exists() and hashlib.sha256(p.read_bytes()).hexdigest()==h for p,h in before.items())
        (logs/(args.label+'-result.json')).write_text(json.dumps(evidence,indent=2)+'\n')
        if not evidence['old_uf2s_unchanged']:
            raise RuntimeError('Prior firmware changed')
    print('PASS: bounded follow software; hardware NOT RUN; '+str(len(before))+' old UF2s preserved')


if __name__ == '__main__':
    main()
