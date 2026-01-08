# Google Cloud Platform C++ Client Libraries: Troubleshooting

## How can I trace gRPC issues?

When working with Google Cloud C++ libraries (which heavily use gRPC), you can use the underlying gRPC C-core environment variables to enable logging.

### Prerequisites

Ensure you have the `grpc` libraries linked correctly.

### Transport logging with gRPC

The primary method for debugging gRPC calls is setting environment variables. These affect the underlying C core.

* `GRPC_TRACE`: A comma-separated list of tracers.  
* `GRPC_VERBOSITY`: `DEBUG`, `INFO`, or `ERROR`.

```
GRPC_VERBOSITY=debug GRPC_TRACE=all ./my_application
```

Common useful traces:

* `api`: Traces API calls.  
* `http`: Traces HTTP/2 frame transport.  
* `tcp`: Traces raw TCP bytes.

## Debug Logging (Library Level)

The Google Cloud C++ libraries have their own tracing mechanism controlled via `google::cloud::Options`.

### Enabling Tracing via Environment Variable

You can enable basic RPC tracing by setting:

```
export GOOGLE_CLOUD_CPP_ENABLE_TRACING="rpc"
./my_application
```

This will log request payloads and response statuses to `clog` (standard error).

### Enabling Tracing via Code

You can programmatically enable tracing using `TracingComponentsOption`. This is useful if you want to target specific subsystems or log streams.

```c
#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"

void EnableTracing() {
  namespace gc = ::google::cloud;
  
  // Enable tracing for RPCs and low-level connection logic
  auto options = gc::Options{}
      .set<gc::TracingComponentsOption>({"rpc", "rpc-streams"});

  // You can also redirect the log output by replacing the backend
  // (Advanced: requires implementing a LogSink)
  
  auto connection = google::cloud::pubsub::MakePublisherConnection(options);
  // ... usage ...
}
```

### Log Output Format

When `rpc` tracing is enabled, you will see output similar to this:

```
[2024-12-11T19:40:00.000Z] [DEBUG] rpc: Pub/Sub: Publish request={...protobuf dump...}
[2024-12-11T19:40:00.100Z] [DEBUG] rpc: Pub/Sub: Publish response={message_ids: "12345"} OK
```

## Reporting a problem

If you encounter issues, the Google Cloud C++ team maintains a repository on GitHub.

* **GitHub Repository:** [googleapis/google-cloud-cpp](https://github.com/googleapis/google-cloud-cpp)  
* **Stack Overflow:** Use tags `google-cloud-platform` and `c++`.

When filing an issue, please include:

1. The library version (or commit hash).  
2. The OS and Compiler version (e.g., GCC 11 on Ubuntu 20.04).  
3. Debug logs (sanitized) from `GOOGLE_CLOUD_CPP_ENABLE_TRACING=rpc`.


