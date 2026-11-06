# Debug-only output removal. This is not a firmware fix or a hardware failsafe.
proc aai05_force_coast {} {
    set failed 0
    foreach {address value} {
        0xd0000014 0x00000f00
        0xd0000024 0x00000f00
        0x40014044 0x00000305
        0x4001404c 0x00000305
        0x40014054 0x00000305
        0x4001405c 0x00000305
    } {
        if {[catch {mww $address $value} reason]} {
            echo "COAST WRITE FAILED at $address: $reason"
            set failed 1
        }
    }
    if {$failed} {return 0}
    if {[catch {
        foreach address {0x40014044 0x4001404c 0x40014054 0x4001405c} {
            if {[lindex [read_memory $address 32 1] 0] != 0x305} {error {GPIO override readback differs}}
        }
        foreach address {0xd0000010 0xd0000020} {
            if {([lindex [read_memory $address 32 1] 0] & 0xf00) != 0xf00} {error {SIO coast readback differs}}
        }
    } reason]} {
        echo "COAST READBACK FAILED: $reason"
        return 0
    }
    return 1
}

proc aai05_available_coast {} {
    foreach core {rp2040.core0 rp2040.core1} {
        if {[catch {targets $core} reason]} {
            echo "Cannot select $core for output removal: $reason"
            continue
        }
        if {[aai05_force_coast]} {return 1}
    }
    return 0
}

proc aai05_core_state {core} {
    if {[catch {$core curstate} state]} {
        echo "Cannot read $core state: $state"
        return unknown
    }
    return $state
}

proc aai05_stop_for_capture {} {
    # Request halt without waiting, to preserve the failing PC if already halted.
    # Attempt output removal before any longer halt wait; verify again afterward.
    foreach core {rp2040.core0 rp2040.core1} {
        if {[catch {targets $core; halt 0} reason]} {echo "Initial halt not yet verified for $core: $reason"}
        echo "State before output removal for $core: [aai05_core_state $core]"
    }
    set first_coast [aai05_available_coast]
    set halt_failed 0
    foreach core {rp2040.core0 rp2040.core1} {
        if {[catch {targets $core; halt 250} reason]} {
            echo "HALT FAILED for $core: $reason"
            set halt_failed 1
        }
    }
    set final_coast [aai05_available_coast]
    if {!$first_coast} {echo {First output-removal attempt was not verified}}
    if {$halt_failed || !$final_coast ||
        [aai05_core_state rp2040.core0] ne "halted" || [aai05_core_state rp2040.core1] ne "halted"} {
        echo {STOP NOT VERIFIED: SWITCH THE CAR OFF NOW}
        return 0
    }
    echo {Both cores halted and motor GPIO8-11 read back high/coast; no resume}
    return 1
}
