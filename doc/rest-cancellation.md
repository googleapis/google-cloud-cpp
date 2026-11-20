# Cancelling synchronous REST requests

`google::cloud::experimental::RestCancellationOption` associates a copyable
`RestCancellation` handle with a synchronous REST operation. Pass it in the
per-call `Options` accepted by `storage::Client`:

```cpp
#include "google/cloud/storage/client.h"
#include "google/cloud/rest_options.h"

namespace gc = google::cloud;
namespace gcs = google::cloud::storage;
namespace gce = google::cloud::experimental;

// In the calling thread:
gce::RestCancellation cancellation;
auto options = gc::Options{}.set<gce::RestCancellationOption>(cancellation);
// Give another ordinary thread a copy of `cancellation` before starting.
gc::StatusOr<gcs::ObjectMetadata> result =
    client.GetObjectMetadata("bucket", "object", options);

// In the interrupt callback on the other thread:
// cancellation.Cancel();
```

The same option works with `ReadObject()` and `WriteObject()`. Streams retain
it for their entire lifetime, including reads, writes, retries, and `Close()`.
Use `ObjectReadStream::status()` or `ObjectWriteStream::last_status()` /
`metadata()` to inspect failures; cancelling an active request produces
`StatusCode::kCancelled`. Read/write I/O retains its existing exception behavior.
As with other read-source close errors, explicit `ObjectReadStream::Close()`
records the cancellation status and throws `RuntimeStatusError` when exceptions
are enabled. With exceptions disabled it returns with `badbit` set. Destruction
still suppresses close exceptions.
Buffered I/O may succeed until the next transport operation observes the state.
An idle stream observes cancellation when it is next used or destroyed;
`Cancel()` itself does not close its socket.

Copies share state; cancellation is permanent and idempotent. Create a fresh
handle for each independently cancellable operation or stream. Reusing a handle
explicitly groups those operations. Setting the option on a client groups all
calls that inherit it. Calls without that handle are unaffected, even when they
share a client, credentials, or pooled connections. Destroying an application
handle does not cancel anything or invalidate copies retained by operations.

Only the handle is safe for concurrent cancellation. Do not concurrently close,
destroy, or otherwise operate on a stream. `Cancel()` is intended for ordinary
threads, not an operating-system signal handler.

## Connecting an application cancellation callback

Give the application's cancellation callback a copy of the handle and call
`Cancel()` from an ordinary thread. Register the callback before starting the
operation and keep its registration alive until the operation and stream cleanup
finish. If cancellation may already have been requested, check the application's
state after registration and cancel the handle before making the SDK call.

For a streaming transfer, pass the same `options` to `ReadObject()` or
`WriteObject()`. If a local source fails during an upload, cancel before unwinding
or explicitly suspend the upload with `std::move(stream).Suspend()`; arbitrary
application exceptions do not request cancellation automatically.

## Cleanup and completion races

When cancellation is observed, the request thread removes the transfer from
libcurl and discards its easy handle. Cleanup does not finish a cancelled
transfer. A cancelled upload's explicit `Close()` or automatic destructor
finalization does not send a final chunk. An unfinished resumable session may
remain on the service until it expires; cancellation does not synchronously
issue a session-deletion request.

Cancellation is not rollback. If a final chunk or other mutating request has
already reached the server, the service may commit it even when the client
returns `kCancelled`. A race with completion can return success or cancellation.
Applications that need to determine the outcome must query the service using a
new cancellation handle, and use generation preconditions where appropriate.

## Implementation and limits

Per-call options flow through `RestContext` into a fixed cancellation handle in
`CurlImpl`. The interrupting thread only updates an atomic flag. Cancellable
requests poll at most every 50ms while waiting for socket activity, and a
libcurl progress callback also checks cancellation during transfers. Requests
without this option retain their existing poll interval. Retry backoffs use the
same bounded checks, and retry loops check cancellation independently of the
application's retry policy.

Storage reads with `RestCancellationOption` set bypass read hedging for the
entire stream, even before cancellation is requested. They run on the calling
thread so unrelated requests occupying the hedging worker pools cannot delay
cancellation. Reads without this option retain the configured hedging behavior.

This avoids shared access to libcurl handles, callback registration races, and
synchronization with pooled-handle destruction. There are no extra request
threads, detached workers, signal handlers, or gRPC/protobuf dependencies.
The cost is up to 20 poll wakeups per second per idle cancellable request.
Scheduling and blocking libcurl/application callbacks can delay observation;
50ms is a poll interval, not a hard end-to-end deadline. A libcurl build using
a synchronous DNS resolver can also block beyond that interval.

Credential acquisition already in progress is **not cancellable**. In the
current credential API, `AuthenticationHeaders()` and `GetToken()` receive no
operation context, credential HTTP calls create separate contexts, and
`CachedCredentials::GetToken()` holds a shared mutex during refresh. Cancelling
one waiter must not cancel refresh needed by unrelated calls. Supporting this
requires a separate design for cancellable credential waiters and refresh
ownership. This implementation checks before acquisition and afterward; after
acquisition returns, a cancelled operation does not send its resource request.
Credential discovery during client construction is also outside this scope.
The option does not provide cancellation of gRPC operations or application
source/sink work.
