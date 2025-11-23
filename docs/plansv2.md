# Revised Kernal Loop

## Tick loop

 1. Advance Time
 2. Process Hardware/HR timers (timer wheel expiries)
 3. Run software interrupts
 4. Call scheduler tick
 5. Preempt Then Schedule
