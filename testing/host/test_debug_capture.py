"""Check debug-stop ordering/failure handling with a fake target, never hardware."""
from pathlib import Path
import tkinter
import unittest

CORE = Path(__file__).resolve().parents[2] / 'tools/debug/capture_core.tcl'
DRIVER = CORE.with_name('capture_request.cfg')


class CaptureStopTests(unittest.TestCase):
    def target(self):
        interp = tkinter.Tcl()
        interp.eval('''
set writes {}
set events {}
set current rp2040.core0
set halt_fail 0
set write_fail 0
set write_fail_target {}
set memory(0xd0000010) 0x02800000
set memory(0xd0000020) 0x02800000
set states(rp2040.core0) running
set states(rp2040.core1) running
proc echo {args} {}
proc mww {address value} {
    global writes events memory write_fail write_fail_target current
    if {$write_fail || $write_fail_target eq $current} {error {synthetic SWD write failure}}
    lappend writes [list $address $value]
    lappend events write
    if {$address == 0xd0000014} {set memory(0xd0000010) [expr {$memory(0xd0000010) | $value}]} elseif {$address == 0xd0000024} {set memory(0xd0000020) [expr {$memory(0xd0000020) | $value}]} else {set memory($address) $value}
}
proc read_memory {address width count} {
    global memory
    if {$width != 32 || $count != 1} {error {unexpected test read}}
    return [list $memory($address)]
}
proc targets {name} {set ::current $name}
proc halt {timeout} {
    global events current states halt_fail
    lappend events [list halt $timeout]
    if {$halt_fail} {error {synthetic halt timeout}}
    if {$timeout != 0} {set states($current) halted}
}
proc rp2040.core0 {action} {return $::states(rp2040.core0)}
proc rp2040.core1 {action} {return $::states(rp2040.core1)}
proc resume {args} {error {capture must never resume the target}}
proc reset {args} {error {capture must never reset the target}}
''')
        interp.call('source', str(CORE))
        return interp

    def test_stop_attempt_precedes_halt_and_both_cores_stay_halted(self):
        interp = self.target()
        self.assertEqual(int(interp.call('aai05_stop_for_capture')), 1)
        self.assertEqual(interp.eval('lindex $events 0'), 'halt 0')
        self.assertEqual(interp.eval('lindex $events 2'), 'write')
        self.assertEqual(interp.eval('lindex $events 8'), 'halt 250')
        self.assertEqual(interp.eval('set states(rp2040.core0)'), 'halted')
        self.assertEqual(interp.eval('set states(rp2040.core1)'), 'halted')
        self.assertEqual(int(interp.eval('llength $writes')), 12)
        self.assertEqual(int(interp.eval('set memory(0x40014044)'), 0), 0x305)
        self.assertEqual(int(interp.eval('set memory(0x4001405c)'), 0), 0x305)
        self.assertEqual(int(interp.eval('expr {$memory(0xd0000010) & 0x02800000}')), 0x02800000)

    def test_failed_halt_still_attempts_coast_but_does_not_claim_capture_ready(self):
        interp = self.target()
        interp.eval('set halt_fail 1')
        self.assertEqual(int(interp.call('aai05_stop_for_capture')), 0)
        self.assertEqual(int(interp.eval('llength $writes')), 12)

    def test_failed_output_writes_do_not_claim_stop_success(self):
        interp = self.target()
        interp.eval('set write_fail 1')
        self.assertEqual(int(interp.call('aai05_stop_for_capture')), 0)

    def test_selection_and_state_errors_still_attempt_output_removal(self):
        for command in ['proc targets {name} {if {$name eq "rp2040.core1"} {error selection}; set ::current $name}',
                        'proc rp2040.core1 {action} {error state}']:
            interp = self.target()
            interp.eval(command)
            self.assertEqual(int(interp.call('aai05_stop_for_capture')), 0)
            self.assertEqual(int(interp.eval('llength $writes')), 12)

    def test_shared_output_removal_uses_an_available_debug_path(self):
        for core in ['rp2040.core0', 'rp2040.core1']:
            interp = self.target()
            interp.call('set', 'write_fail_target', core)
            self.assertEqual(int(interp.call('aai05_stop_for_capture')), 1)

    def test_armed_transport_error_attempts_stop_and_reports_power_off(self):
        interp = self.target()
        interp.eval('''
set messages {}
set gpio_reads 0
set fake_ms 0
foreach address {0x40014044 0x4001404c 0x40014054 0x4001405c} {set memory($address) 5}
set memory(0xd0000010) 0x02800f00
set memory(0x40050050) 0
set memory(0x40050064) 0
proc echo {args} {lappend ::messages $args}
rename source real_source
proc find {path} {return MOCK_CONFIG}
proc source {path} {}
proc adapter {args} {}
proc gdb {args} {}
proc tcl {args} {}
proc telnet {args} {}
proc init {args} {}
proc poll {args} {}
proc ms {} {return $::fake_ms}
proc sleep {amount} {incr ::fake_ms $amount}
rename read_memory old_read_memory
proc read_memory {address width count} {
    if {$address == 0xd0000004} {
        incr ::gpio_reads
        if {$::gpio_reads >= 2} {error {synthetic armed read error}}
        return {0x100000}
    }
    return [old_read_memory $address $width $count]
}
''')
        source = DRIVER.read_text(encoding='utf-8')
        self.assertEqual(source.count('while {1} {sleep 100}'), 1)
        source = source.replace('while {1} {sleep 100}', 'set ::driver_done 1')
        interp.eval(source)
        self.assertEqual(int(interp.eval('set driver_done')), 1)
        self.assertGreaterEqual(int(interp.eval('llength $writes')), 12)
        self.assertIn('SWITCH', interp.eval('set messages'))


if __name__ == '__main__':
    unittest.main()
