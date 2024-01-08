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
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/internal/async/accumulate_read_object.h"
#include "google/cloud/storage/internal/async/insert_object.h"
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/storage/internal/async/reader_connection_impl.h"
#include "google/cloud/storage/internal/async/write_payload_impl.h"
#include "google/cloud/storage/internal/async/writer_connection_buffered.h"
#include "google/cloud/storage/internal/async/writer_connection_finalized.h"
#include "google/cloud/storage/internal/async/writer_connection_impl.h"
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
#include "google/cloud/internal/make_status.h"
#include <memory>

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

future<StatusOr<storage::ObjectMetadata>> AsyncConnectionImpl::InsertObject(
    InsertObjectParams p) {
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

future<StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>>
AsyncConnectionImpl::ReadObject(ReadObjectParams p) {
  using StreamingRpc = google::cloud::internal::AsyncStreamingReadRpc<
      google::storage::v2::ReadObjectResponse>;

  auto current = internal::MakeImmutableOptions(std::move(p.options));
  auto proto = ToProto(p.request.impl_);
  if (!proto) {
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>(
            std::move(proto).status()));
  }

  auto call = [stub = stub_, current, request = std::move(p.request)](
                  CompletionQueue& cq,
                  std::shared_ptr<grpc::ClientContext> context,
                  google::storage::v2::ReadObjectRequest const& proto)
      -> future<StatusOr<std::unique_ptr<StreamingRpc>>> {
    ApplyQueryParameters(*context, *current, request);
    // TODO(#12359) - pass the `options` parameter
    google::cloud::internal::OptionsSpan span(current);

    auto rpc = stub->AsyncReadObject(cq, std::move(context), proto);
    auto start = rpc->Start();
    return start.then([rpc = std::move(rpc)](auto f) mutable {
      if (f.get()) return make_ready_future(make_status_or(std::move(rpc)));
      auto r = std::move(rpc);
      return r->Finish().then([](auto f) {
        auto status = f.get();
        return StatusOr<std::unique_ptr<StreamingRpc>>(std::move(status));
      });
    });
  };

  auto transform = [current](auto f) mutable
      -> StatusOr<
          std::unique_ptr<storage_experimental::AsyncReaderConnection>> {
    auto rpc = f.get();
    if (!rpc) return std::move(rpc).status();
    return std::unique_ptr<storage_experimental::AsyncReaderConnection>(
        std::make_unique<AsyncReaderConnectionImpl>(current, *std::move(rpc)));
  };

  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(*current), backoff_policy(*current),
             Idempotency::kIdempotent, cq_, std::move(call), *std::move(proto),
             __func__)
      .then(std::move(transform));
}

future<StatusOr<storage_experimental::ReadPayload>>
AsyncConnectionImpl::ReadObjectRange(ReadObjectParams p) {
  auto proto = ToProto(p.request.impl_);
  if (!proto) {
    return make_ready_future(
        StatusOr<storage_experimental::ReadPayload>(std::move(proto).status()));
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

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::StartUnbufferedUpload(UploadParams p) {
  auto current = internal::MakeImmutableOptions(std::move(p.options));

  if (p.request.HasOption<storage::UseResumableUploadSession>()) {
    auto query = storage::internal::QueryResumableUploadRequest(
        p.request.GetOption<storage::UseResumableUploadSession>().value());
    p.request.impl_.ForEachOption(storage::internal::CopyCommonOptions(query));
    return ResumeUpload(current, std::move(p.request), query);
  }
  auto response = StartResumableWrite(current, p.request.impl_);
  return response.then([w = WeakFromThis(), current = std::move(current),
                        request = std::move(p.request)](auto f) mutable {
    auto self = w.lock();
    if (auto self = w.lock()) {
      return self->UnbufferedUploadImpl(std::move(current), std::move(request),
                                        f.get());
    }
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            internal::CancelledError("Cannot lock self", GCP_ERROR_INFO())));
  });
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::StartBufferedUpload(UploadParams p) {
  auto current = internal::MakeImmutableOptions(p.options);
  auto async_write_object = [c = current, r = p.request, w = WeakFromThis()](
                                std::string const& upload_id) mutable {
    auto query = storage::internal::QueryResumableUploadRequest(upload_id);
    r.impl_.ForEachOption(storage::internal::CopyCommonOptions(query));
    r.set_multiple_options(storage::UseResumableUploadSession(upload_id));
    if (auto self = w.lock()) return self->ResumeUpload(c, r, query);
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>{
            internal::CancelledError("Cannot lock self", GCP_ERROR_INFO())});
  };
  return StartUnbufferedUpload(std::move(p))
      .then([current = std::move(current),
             async_write_object = std::move(async_write_object)](auto f) mutable
            -> StatusOr<
                std::unique_ptr<storage_experimental::AsyncWriterConnection>> {
        auto w = f.get();
        if (!w) return std::move(w).status();
        auto factory = [upload_id = (*w)->UploadId(),
                        f = std::move(async_write_object)]() mutable {
          return f(upload_id);
        };
        return MakeWriterConnectionBuffered(std::move(factory), *std::move(w),
                                            *current);
      });
}

future<StatusOr<storage::ObjectMetadata>> AsyncConnectionImpl::ComposeObject(
    ComposeObjectParams p) {
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

future<Status> AsyncConnectionImpl::DeleteObject(DeleteObjectParams p) {
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

future<StatusOr<google::storage::v2::StartResumableWriteResponse>>
AsyncConnectionImpl::StartResumableWrite(
    internal::ImmutableOptions current,
    storage::internal::ResumableUploadRequest request) {
  auto proto = ToProto(request);
  if (!proto) {
    return make_ready_future(
        StatusOr<google::storage::v2::StartResumableWriteResponse>(
            std::move(proto).status()));
  }
  // Querying the status of an upload is always idempotent. It is a read-only
  // operation.
  auto const idempotency = Idempotency::kIdempotent;
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
      std::move(retry), std::move(backoff), idempotency, cq_,
      [stub = stub_, current = std::move(current),
       request = std::move(request)](
          CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
          google::storage::v2::StartResumableWriteRequest const& proto) {
        ApplyQueryParameters(*context, *current, request);
        // TODO(#12359) - pass the `options` parameter
        google::cloud::internal::OptionsSpan span(current);
        return stub->AsyncStartResumableWrite(cq, std::move(context), proto);
      },
      *proto, __func__);
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::ResumeUpload(
    internal::ImmutableOptions current,
    storage_experimental::ResumableUploadRequest request,
    storage::internal::QueryResumableUploadRequest query) {
  auto response = QueryWriteStatus(current, std::move(query));
  return response.then([w = WeakFromThis(), c = std::move(current),
                        r = std::move(request)](auto f) mutable {
    auto self = w.lock();
    if (auto self = w.lock()) {
      return self->UnbufferedUploadImpl(std::move(c), std::move(r), f.get());
    }
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            internal::CancelledError("Cannot lock self", GCP_ERROR_INFO())));
  });
}

future<StatusOr<google::storage::v2::QueryWriteStatusResponse>>
AsyncConnectionImpl::QueryWriteStatus(
    internal::ImmutableOptions current,
    storage::internal::QueryResumableUploadRequest request) {
  auto proto = ToProto(request);
  // Starting a new upload is always idempotent. Any side-effects of early
  // attempts are not observable.
  auto const idempotency = Idempotency::kIdempotent;
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
      std::move(retry), std::move(backoff), idempotency, cq_,
      [stub = stub_, current = std::move(current),
       request = std::move(request)](
          CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
          google::storage::v2::QueryWriteStatusRequest const& proto) {
        ApplyQueryParameters(*context, *current, request);
        // TODO(#12359) - pass the `options` parameter
        google::cloud::internal::OptionsSpan span(current);
        return stub->AsyncQueryWriteStatus(cq, std::move(context), proto);
      },
      proto, __func__);
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::UnbufferedUploadImpl(
    internal::ImmutableOptions current,
    storage_experimental::ResumableUploadRequest request,
    StatusOr<google::storage::v2::StartResumableWriteResponse> response) {
  if (!response) {
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            std::move(response).status()));
  }
  auto hash_function = storage::internal::CreateHashFunction(
      request.GetOption<storage::Crc32cChecksumValue>(),
      request.GetOption<storage::DisableCrc32cChecksum>(),
      request.GetOption<storage::MD5HashValue>(),
      request.GetOption<storage::DisableMD5Hash>());
  auto configure = [current, request = std::move(request),
                    upload =
                        response->upload_id()](grpc::ClientContext& context) {
    ApplyQueryParameters(context, *current, request);
    ApplyResumableUploadRoutingHeader(context, upload);
  };

  return UnbufferedUploadImpl(std::move(current), std::move(configure),
                              std::move(*response->mutable_upload_id()),
                              std::move(hash_function), 0);
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::UnbufferedUploadImpl(
    internal::ImmutableOptions current,
    storage_experimental::ResumableUploadRequest request,
    StatusOr<google::storage::v2::QueryWriteStatusResponse> response) {
  if (!response) {
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            std::move(response).status()));
  }
  auto id =
      request.GetOption<storage::UseResumableUploadSession>().value_or("");
  if (response->has_resource()) {
    auto metadata = FromProto(response->resource(), *current);
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            std::make_unique<AsyncWriterConnectionFinalized>(
                std::move(id), std::move(metadata))));
  }

  auto hash_function =
      response->persisted_size() == 0
          ? storage::internal::CreateHashFunction(
                request.GetOption<storage::Crc32cChecksumValue>(),
                request.GetOption<storage::DisableCrc32cChecksum>(),
                request.GetOption<storage::MD5HashValue>(),
                request.GetOption<storage::DisableMD5Hash>())
          : storage::internal::CreateNullHashFunction();
  auto configure = [current, request = std::move(request),
                    upload = id](grpc::ClientContext& context) {
    ApplyQueryParameters(context, *current, request);
    ApplyResumableUploadRoutingHeader(context, upload);
  };
  return UnbufferedUploadImpl(std::move(current), std::move(configure),
                              std::move(id), std::move(hash_function),
                              response->persisted_size());
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::UnbufferedUploadImpl(
    internal::ImmutableOptions current,
    std::function<void(grpc::ClientContext&)> configure_context,
    std::string upload_id,
    std::shared_ptr<storage::internal::HashFunction> hash_function,
    std::int64_t persisted_size) {
  using StreamingRpc = AsyncWriterConnectionImpl::StreamingRpc;

  struct RequestPlaceholder {};
  auto call =
      [stub = stub_, current, configure_context = std::move(configure_context)](
          CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
          RequestPlaceholder const&)
      -> future<StatusOr<std::unique_ptr<StreamingRpc>>> {
    configure_context(*context);
    // TODO(#12359) - pass the `options` parameter
    google::cloud::internal::OptionsSpan span(current);

    auto rpc = stub->AsyncBidiWriteObject(cq, std::move(context));
    auto start = rpc->Start();
    // TODO(coryan): I think we just call `Start()` and then send the data and
    // the metadata (if needed) on the first Write() call.
    return start.then([rpc = std::move(rpc)](auto f) mutable {
      if (f.get()) return make_ready_future(make_status_or(std::move(rpc)));
      auto r = std::move(rpc);
      return r->Finish().then([](auto f) {
        auto status = f.get();
        return StatusOr<std::unique_ptr<StreamingRpc>>(std::move(status));
      });
    });
  };

  auto transform = [current, id = std::move(upload_id), persisted_size,
                    hash = std::move(hash_function)](auto f) mutable
      -> StatusOr<
          std::unique_ptr<storage_experimental::AsyncWriterConnection>> {
    auto rpc = f.get();
    if (!rpc) return std::move(rpc).status();
    return std::unique_ptr<storage_experimental::AsyncWriterConnection>(
        std::make_unique<AsyncWriterConnectionImpl>(
            current, *std::move(rpc), std::move(id), std::move(hash),
            persisted_size));
  };

  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(*current), backoff_policy(*current),
             Idempotency::kIdempotent, cq_, std::move(call),
             RequestPlaceholder{}, __func__)
      .then(std::move(transform));
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
