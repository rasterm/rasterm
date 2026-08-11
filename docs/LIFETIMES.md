# Thread Safety and Buffer Lifetimes

`Engine` owns terminal state and is single threaded. Initialize, render, clear, reset,
and shut it down from one thread. Frame, palette, metadata, damage, and shared views are
immutable non owning descriptions.

`Presenter` is thread safe for concurrent submission and statistics queries. Its
capacity one mailbox replaces stale frames rather than blocking producers.

`waitUntilIdle` is a bounded synchronization point for sink acceptance. It can block on
an in flight `OutputSink` call and therefore must not run from a Presenter worker
callback. `invalidate` is ordered on the worker and forces a full next render. Neither
operation adds another frame queue.

- `submit(FrameView)` and its indexed overload copy all referenced data.
- `submit(OwnedFrame&&)` and its indexed overload move ownership into the mailbox.
- `submitShared(...)` performs no copy. Its `shared_ptr<const void>` token must own every
  referenced pixel, palette, and damage allocation.

Output sink methods run on the Engine caller thread or Presenter worker. Diagnostic and
event callbacks run synchronously on the detecting thread. Read only Presenter queries
are reentrant. A Presenter worker callback must not initialize, move, shut down, destroy,
or otherwise mutate its Presenter; a reentrant shutdown request is rejected and external
shutdown must subsequently join the worker. Destroying the object from its own callback
is forbidden because the callback is executing through that object.

`OutputSink::write` is all or nothing: returning `false` means none of the supplied bytes
were accepted. rasterm may retry cursor, alternate screen, and synchronized output
restoration once. Sinks that can make partial physical writes must finish the remaining
bytes internally before returning `true`.

Timestamps are signed nanoseconds in a caller defined clock domain. rasterm preserves
them for events and telemetry but does not interpret them. Frame ID zero and
`unknownTimestamp` mean unspecified.
