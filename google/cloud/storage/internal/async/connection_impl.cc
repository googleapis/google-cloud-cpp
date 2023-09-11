// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/internal/async/accumulate_read_object.h"
#include "google/cloud/storage/internal/async/insert_object.h"
#include "google/cloud/storage/internal/async/write_payload_impl.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/grpc/channel_refresh.h"
#include "google/cloud/storage/internal/grpc/configure_client_context.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"
#include "google/cloud/storage/internal/grpc/object_request_parser.h"
#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/internal/async_retry_loop.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

inline std::unique_ptr<storage::RetryPolicy> retry_policy(
    Options const& options) {
  return options.get<storage::RetryPolicyOption>()->clone();
}

inline std::unique_ptr<BackoffPolicy> backoff_policy(Options const& options) {
  return options.get<storage::BackoffPolicyOption>()->clone();
}

inline std::unique_ptr<storage::IdempotencyPolicy> idempotency_policy(
    Options const& options) {
  return options.get<storage::IdempotencyPolicyOption>()->clone();
}

}  // namespace

AsyncConnectionImpl::AsyncConnectionImpl(
    CompletionQueue cq, std::shared_ptr<GrpcChannelRefresh> refresh,
    std::shared_ptr<StorageStub> stub, Options options)
    : cq_(std::move(cq)),
      refresh_(std::move(refresh)),
      stub_(std::move(stub)),
      options_(std::move(options)) {}

future<StatusOr<storage::ObjectMetadata>>
AsyncConnectionImpl::AsyncInsertObject(InsertObjectParams p) {
  auto proto = ToProto(p.request);
  if (!proto) {
    return make_ready_future(
        StatusOr<storage::ObjectMetadata>(std::move(proto).status()));
  }
  // We are using request ids, so the request is always idempotent.
  auto const idempotency = Idempotency::kIdempotent;
  auto const current = internal::MakeImmutableOptions(std::move(p.options));
  auto call = [stub = stub_, params = std::move(p), current,
               id = invocation_id_generator_.MakeInvocationId()](
                  CompletionQueue& cq,
                  std::shared_ptr<grpc::ClientContext> context,
                  Options const& options,
                  google::storage::v2::WriteObjectRequest const& proto) {
    auto hash_function =
        [](storage_experimental::InsertObjectRequest const& r) {
          return storage::internal::CreateHashFunction(
              r.GetOption<storage::Crc32cChecksumValue>(),
              r.GetOption<storage::DisableCrc32cChecksum>(),
              r.GetOption<storage::MD5HashValue>(),
              r.GetOption<storage::DisableMD5Hash>());
        };

    ApplyQueryParameters(*context, options, params.request);
    ApplyRoutingHeaders(*context, params.request);
    context->AddMetadata("x-goog-gcs-idempotency-token", id);
    // TODO(#12359) - pass the `options` parameter
    google::cloud::internal::OptionsSpan span(current);
    auto rpc = stub->AsyncWriteObject(cq, std::move(context));
    auto running =
        InsertObject::Call(std::move(rpc), hash_function(params.request), proto,
                           WritePayloadImpl::GetImpl(params.payload), current);
    return running->Start().then([running](auto f) mutable {
      running.reset();  // extend the life of the co-routine until it co-returns
      return f.get();
    });
  };
  return google::cloud::internal::AsyncRetryLoop(
      retry_policy(*current), backoff_policy(*current), idempotency, cq_,
      std::move(call), current, *std::move(proto), __func__);
}

future<storage_experimental::AsyncReadObjectRangeResponse>
AsyncConnectionImpl::AsyncReadObjectRange(ReadObjectParams p) {
  auto proto = ToProto(p.request.impl_);
  if (!proto) {
    auto response = storage_experimental::AsyncReadObjectRangeResponse{};
    response.status = std::move(proto).status();
    return make_ready_future(std::move(response));
  }

  auto const current = internal::MakeImmutableOptions(std::move(p.options));
  auto context_factory = [options = internal::CurrentOptions(),
                          request = std::move(p.request)]() {
    auto context = std::make_shared<grpc::ClientContext>();
    ApplyQueryParameters(*context, options, request);
    return context;
  };
  return storage_internal::AsyncAccumulateReadObjectFull(
             cq_, stub_, std::move(context_factory), *std::move(proto),
             *current)
      .then([current](
                future<storage_internal::AsyncAccumulateReadObjectResult> f) {
        return ToResponse(f.get(), *current);
      });
}

future<StatusOr<storage::ObjectMetadata>>
AsyncConnectionImpl::AsyncComposeObject(ComposeObjectParams p) {
  auto current = internal::MakeImmutableOptions(std::move(p.options));
  auto proto = ToProto(p.request.impl_);
  if (!proto) {
    return make_ready_future(
        StatusOr<storage::ObjectMetadata>(std::move(proto).status()));
  }
  auto const idempotency =
      idempotency_policy(*current)->IsIdempotent(p.request.impl_)
          ? Idempotency::kIdempotent
          : Idempotency::kNonIdempotent;
  auto call = [stub = stub_, current, request = std::move(p.request)](
                  CompletionQueue& cq,
                  std::shared_ptr<grpc::ClientContext> context,
                  google::storage::v2::ComposeObjectRequest const& proto) {
    ApplyQueryParameters(*context, *current, request);
    return stub->AsyncComposeObject(cq, std::move(context), proto);
  };
  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(*current), backoff_policy(*current), idempotency, cq_,
             std::move(call), *std::move(proto), __func__)
      .then([current](auto f) -> StatusOr<storage::ObjectMetadata> {
        auto response = f.get();
        if (!response) return std::move(response).status();
        return FromProto(*response, *current);
      });
}

future<Status> AsyncConnectionImpl::AsyncDeleteObject(DeleteObjectParams p) {
  auto current = internal::MakeImmutableOptions(std::move(p.options));
  auto proto = ToProto(p.request.impl_);
  auto const idempotency =
      idempotency_policy(*current)->IsIdempotent(p.request.impl_)
          ? Idempotency::kIdempotent
          : Idempotency::kNonIdempotent;
  return google::cloud::internal::AsyncRetryLoop(
      retry_policy(*current), backoff_policy(*current), idempotency, cq_,
      [stub = stub_, current, request = std::move(p.request)](
          CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
          google::storage::v2::DeleteObjectRequest const& proto) {
        ApplyQueryParameters(*context, *current, request);
        // TODO(#12359) - pass the `options` parameter
        google::cloud::internal::OptionsSpan span(current);
        return stub->AsyncDeleteObject(cq, std::move(context), proto);
      },
      proto, __func__);
}

std::shared_ptr<storage_experimental::AsyncConnection> MakeAsyncConnection(
    CompletionQueue cq, Options options) {
  options = storage_internal::DefaultOptionsGrpc(std::move(options));
  auto p = CreateStorageStub(cq, options);
  return std::make_shared<AsyncConnectionImpl>(
      std::move(cq), std::move(p.first), std::move(p.second),
      std::move(options));
}

std::shared_ptr<storage_experimental::AsyncConnection> MakeAsyncConnection(
    CompletionQueue cq, std::shared_ptr<StorageStub> stub, Options options) {
  return std::make_shared<AsyncConnectionImpl>(
      std::move(cq), std::shared_ptr<GrpcChannelRefresh>{}, std::move(stub),
      std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
