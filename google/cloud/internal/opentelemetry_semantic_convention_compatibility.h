// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_SEMANTIC_CONVENTION_COMPATIBILITY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_SEMANTIC_CONVENTION_COMPATIBILITY_H

#include <opentelemetry/version.h>

// OpenTelemetry 1.18.0 introduced dedicated semantic convention headers under
// `<opentelemetry/semconv/...>`. Define the required constants for
// OpenTelemetry < 1.18.0.
#if OPENTELEMETRY_VERSION_MAJOR > 1 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 18)
#include <opentelemetry/semconv/client_attributes.h>
#include <opentelemetry/semconv/http_attributes.h>
#include <opentelemetry/semconv/incubating/messaging_attributes.h>
#include <opentelemetry/semconv/incubating/rpc_attributes.h>
#include <opentelemetry/semconv/network_attributes.h>
#include <opentelemetry/semconv/server_attributes.h>
#include <opentelemetry/semconv/url_attributes.h>
#else
OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv {
namespace client {

static constexpr char const* kClientAddress = "client.address";
static constexpr char const* kClientPort = "client.port";

}  // namespace client
namespace http {

static constexpr char const* kHttpRequestMethod = "http.request.method";

}  // namespace http
namespace messaging {

static constexpr char const* kMessagingMessageEnvelopeSize =
    "messaging.message.envelope.size";
static constexpr char const* kMessagingOperationType =
    "messaging.operation.type";

}  // namespace messaging
namespace network {

static constexpr char const* kNetworkTransport = "network.transport";

namespace NetworkTransportValues {

static constexpr char const* kTcp = "tcp";

}  // namespace NetworkTransportValues

}  // namespace network
namespace rpc {

static constexpr char const* kRpcMethod = "rpc.method";

}  // namespace rpc
namespace server {

static constexpr char const* kServerAddress = "server.address";
static constexpr char const* kServerPort = "server.port";

}  // namespace server
namespace url {

static constexpr char const* kUrlFull = "url.full";

}  // namespace url
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
#endif  // OPENTELEMETRY_VERSION_MAJOR > 1 ||
        // (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >=
        // 18)

// OpenTelemetry 1.25.0 deprecated `rpc.system` (`kRpcSystem`) and
// `rpc.service` (`kRpcService`) in favor of `rpc.system.name`
// (`kRpcSystemName`) and a fully-qualified `rpc.method` (`kRpcMethod`).
// Define `kRpcSystemName` and `RpcSystemNameValues` for OpenTelemetry < 1.25.0.
#if OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR < 25

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv {
namespace rpc {

static constexpr char const* kRpcSystemName = "rpc.system.name";

namespace RpcSystemNameValues {

static constexpr char const* kGrpc = "grpc";
static constexpr char const* kDubbo = "dubbo";
static constexpr char const* kConnectrpc = "connectrpc";
static constexpr char const* kJsonrpc = "jsonrpc";

}  // namespace RpcSystemNameValues

}  // namespace rpc
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE

#endif  // OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR < 25

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_SEMANTIC_CONVENTION_COMPATIBILITY_H
