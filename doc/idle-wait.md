# Idle-Wait Mode for WAI

## Purpose

`rk65c02` defaults to max-throughput execution. This is ideal for benchmarks and
batch workloads, but can waste host CPU when guest software repeatedly executes
`WAI` in idle loops.

Idle-wait mode is an optional host-assisted mechanism that reduces host load
during those `WAI` periods.

## API

Use the public API:

- `rk65c02_idle_wait_set(e, cb, ctx)`
- `rk65c02_idle_wait_clear(e)`

Callback type:

- `rk65c02_wait_cb_t(rk65c02emu_t *e, void *ctx)`

The callback is invoked only when the CPU is in `STOPPED` state with
`stopreason == WAI`.

## Recommended Host Strategy

Treat the callback as "wait until next wake source":

1. Compute next timer/device deadline.
2. Block on a condition variable, eventfd, poll timeout, or equivalent.
3. On timer expiry or external event, assert IRQ with `rk65c02_assert_irq(e)`.
4. Return from callback.

This mirrors common emulator design: halt the CPU in idle and resume on
interrupt source readiness.

## Callback Contract

- The callback may block, but should eventually return on:
  - interrupt/event delivery,
  - host stop request, or
  - shutdown path.
- It should not assume real-time pacing for non-idle execution.
- It is called at a safe boundary where CPU execution is already stopped in
  `WAI`.

## Notes

- Default behavior is unchanged if idle-wait is not enabled.
- `STP`, breakpoints, stepping, and host-stop paths do not use idle-wait.
