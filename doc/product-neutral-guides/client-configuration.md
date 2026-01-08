# Google Cloud Platform C++ Client Libraries: Client Configuration

The Google Cloud C++ Client Libraries allow you to configure client behavior via the `google::cloud::Options` class passed to the client constructor or the connection factory functions.

## 1. Common Configuration Options

The `google::cloud::Options` class is a type-safe map where you set specific option structs.

| Option Struct | Description |
| ----- | ----- |
| `google::cloud::EndpointOption` | The address of the API remote host. Used for Regional Endpoints. |
| `google::cloud::UserProjectOption` | Quota project to use for the request. |
| `google::cloud::AuthorityOption` | Sets the `:authority` pseudo-header (useful for testing/emulators). |
| `google::cloud::UnifiedCredentialsOption` | Explicit credentials object (overrides default discovery). |
| `google::cloud::TracingComponentsOption` | Controls client-side logging/tracing. |

## 2. Customizing the API Endpoint

You can modify the API endpoint to connect to a specific Google Cloud region or to a private endpoint.

### Connecting to a Regional Endpoint

```c
#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/common_options.h"

void CreateRegionalClient() {
  namespace pubsub = ::google::cloud::pubsub;
  namespace gc = ::google::cloud;

  // Configure options
  auto options = gc::Options{}
      .set<gc::EndpointOption>("us-east1-pubsub.googleapis.com");

  // Create the connection with options
  auto connection = pubsub::MakePublisherConnection(options);
  
  // Create the client
  auto client = pubsub::Publisher(connection);
}
```

## 3. Configuring a Proxy

### Proxy with gRPC

The C++ gRPC layer respects standard environment variables. You generally do not configure this in C++ code.

Set the following environment variables in your shell or Docker container:

```
export http_proxy="http://proxy.example.com:3128"
export https_proxy="http://proxy.example.com:3128"
```

**Handling Self-Signed Certificates:** If your proxy uses a self-signed certificate, use the standard gRPC environment variable:

```
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="/path/to/roots.pem"
```

### Proxy with REST

If using a library that supports REST (like `google-cloud-storage`), it primarily relies on `libcurl`, which also respects the standard `http_proxy` and `https_proxy` environment variables.

## 4. Configuring Retries and Timeouts

In C++, retry policies are configured via `Options` or passed specifically to the connection factory.

### Configuring Retry Policies

You can set the `RetryPolicyOption` and `BackoffPolicyOption`.

```c
#include "google/cloud/secretmanager/secret_manager_client.h"
#include "google/cloud/options.h"
#include <chrono>

void ConfigureRetries() {
  namespace secretmanager = ::google::cloud::secretmanager;
  using namespace std::chrono_literals;

  // Define a limited retry policy (e.g., max 3 retries or 10 minutes)
  auto retry_policy = secretmanager::SecretManagerServiceLimitedTimeRetryPolicy(10min);

  // Define backoff (exponential: starts at 1s, max 30s, scaling 2.0)
  auto backoff_policy = google::cloud::ExponentialBackoffPolicy(1s, 30s, 2.0);

  auto options = google::cloud::Options{}
      .set<secretmanager::SecretManagerServiceRetryPolicyOption>(
          retry_policy.clone())
      .set<secretmanager::SecretManagerServiceBackoffPolicyOption>(
          backoff_policy.clone());

  auto connection = secretmanager::MakeSecretManagerServiceConnection(options);
  auto client = secretmanager::SecretManagerServiceClient(connection);
}
```

### Configuring Timeouts

There isn't a single "timeout" integer. Instead, you can configure the **Idempotency Policy** (to determine which RPCs are safe to retry) or use `google::cloud::Options` to set specific RPC timeouts if the library exposes a specific option, though usually, the `RetryPolicy` (Total Timeout) governs the duration of the call.

For per-call context (like deadlines), you can sometimes use `grpc::ClientContext` if dropping down to the raw stub level, but idiomatic Google Cloud C++ usage prefers the Policy approach.


