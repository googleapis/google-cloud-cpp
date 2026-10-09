# Google Cloud Platform C++ Client Libraries: Post-Quantum Cryptography (PQC)

The Google Cloud C++ Client Libraries support Post-Quantum Cryptography (PQC)
for TLS 1.3 connections to protect network traffic against "harvest now, decrypt
later" attacks.

By default, both gRPC and REST transports negotiate the hybrid post-quantum key
exchange mechanism **`X25519MLKEM768`** (combining classical `X25519` ECDH with
NIST-standardized `ML-KEM-768`) whenever supported by the underlying TLS and
cryptographic libraries.

## 1. Default Behavior by Transport

No code changes are required to use PQC when building with supported dependency
versions:

| Transport | Default Behavior                                            | Minimum Dependency Requirements                        |
| --------- | ----------------------------------------------------------- | ------------------------------------------------------ |
| **gRPC**  | Enabled automatically by gRPC and its TLS stack.            | gRPC built with BoringSSL or OpenSSL >= 3.5.0          |
| **REST**  | `X25519MLKEM768` is prioritized in `CURLOPT_SSL_EC_CURVES`. | `libcurl >= 7.73.0` with BoringSSL or OpenSSL >= 3.5.0 |

### REST Transport Details

When creating a REST connection, the library inspects the linked `libcurl` and
SSL/TLS libraries at runtime. If `X25519MLKEM768` is available, it is prepended
to the list of supported TLS groups passed to `libcurl`.

If the runtime environment does not support `X25519MLKEM768` (for example, when
linked against OpenSSL < 3.5.0 or `libcurl < 7.73.0`), the client logs an
informational (`INFO`) message and falls back to the default classical curves.

## 2. Disabling Post-Quantum Cryptography

In certain network environments, legacy firewalls, proxies, or middleboxes may
drop or time out TLS handshakes containing larger hybrid post-quantum
`ClientHello` key shares. If you encounter TLS handshake failures, you can opt
out of PQC.

### Disabling PQC with REST

Set the `GOOGLE_CLOUD_CPP_DISABLE_PQC` environment variable to any value before
starting your application:

```
export GOOGLE_CLOUD_CPP_DISABLE_PQC="true"
./my_application
```

When `GOOGLE_CLOUD_CPP_DISABLE_PQC` is set, the REST client does not prioritize
`X25519MLKEM768` and leaves the underlying SSL library's default curve order
unchanged.

### Disabling PQC with gRPC

The `GOOGLE_CLOUD_CPP_DISABLE_PQC` environment variable only applies to REST
clients. Neither `google-cloud-cpp` nor upstream C++ gRPC provides an
environment variable to disable `X25519MLKEM768` on default credentials.

To restrict a gRPC client to classical TLS key exchange groups, configure custom
TLS channel credentials using `grpc::experimental::TlsChannelCredentialsOptions`
and pass them via `google::cloud::GrpcCredentialOption`:

```cpp
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/tls_credentials_options.h>

google::cloud::Options ConfigureClassicalTlsForGrpc(
    std::shared_ptr<grpc::CallCredentials> call_credentials) {
  grpc::experimental::TlsChannelCredentialsOptions tls_options;
  tls_options.set_key_exchange_groups({
      GRPC_TLS_KEY_EXCHANGE_GROUP_X25519,
      GRPC_TLS_KEY_EXCHANGE_GROUP_SECP256R1,
  });

  auto channel_credentials = grpc::CompositeChannelCredentials(
      grpc::experimental::TlsCredentials(tls_options),
      std::move(call_credentials));

  return google::cloud::Options{}
      .set<google::cloud::GrpcCredentialOption>(std::move(channel_credentials));
}
```
