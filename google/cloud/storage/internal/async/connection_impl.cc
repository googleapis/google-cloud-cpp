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
#include "google/cloud/storage/async/idempotency_policy.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/async/resume_policy.h"
#include "google/cloud/storage/internal/async/accumulate_read_object.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/internal/async/insert_object.h"
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/storage/internal/async/reader_connection_impl.h"
#include "google/cloud/storage/internal/async/reader_connection_resume.h"
#include "google/cloud/storage/internal/async/rewriter_connection_impl.h"
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
#include "google/cloud/storage/internal/grpc/scale_stall_timeout.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/internal/async_read_write_stream_timeout.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/async_streaming_read_rpc_timeout.h"
#include "google/cloud/internal/async_streaming_write_rpc_timeout.h"
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

inline std::unique_ptr<storage_experimental::IdempotencyPolicy>
idempotency_policy(Options const& options) {
  return options.get<storage_experimental::IdempotencyPolicyOption>()();
}

std::unique_ptr<storage::internal::HashFunction> CreateHashFunction(
    Options const& options) {
  auto const has_precomputed_crc32c =
      options.has<storage_experimental::UseCrc32cValueOption>();
  auto const enable_crc32c =
      options.get<storage_experimental::EnableCrc32cValidationOption>();
  auto const has_precomputed_md5 =
      options.has<storage_experimental::UseMD5ValueOption>();
  auto const enable_md5 =
      options.get<storage_experimental::EnableMD5ValidationOption>();

  auto const crc32c = !has_precomputed_crc32c && enable_crc32c;
  auto const md5 = !has_precomputed_md5 && enable_md5;

  if (crc32c && md5) {
    return std::make_unique<storage::internal::CompositeFunction>(
        std::make_unique<storage::internal::Crc32cHashFunction>(),
        storage::internal::MD5HashFunction::Create());
  }
  if (crc32c) return std::make_unique<storage::internal::Crc32cHashFunction>();
  if (md5) return storage::internal::MD5HashFunction::Create();
  return storage::internal::CreateNullHashFunction();
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
  auto current = internal::MakeImmutableOptions(std::move(p.options));
  auto call = [stub = stub_, params = std::move(p),
               id = invocation_id_generator_.MakeInvocationId()](
                  CompletionQueue& cq,
                  std::shared_ptr<grpc::ClientContext> context,
                  google::cloud::internal::ImmutableOptions options,
                  google::storage::v2::WriteObjectRequest const& proto) {
    using StreamingRpcTimeout =
        google::cloud::internal::AsyncStreamingWriteRpcTimeout<
            google::storage::v2::WriteObjectRequest,
            google::storage::v2::WriteObjectResponse>;
    auto timeout = ScaleStallTimeout(
        options->get<storage::TransferStallTimeoutOption>(),
        options->get<storage::TransferStallMinimumRateOption>(),
        google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);

    auto hash_function =
        [](storage_experimental::InsertObjectRequest const& r) {
          return storage::internal::CreateHashFunction(
              r.GetOption<storage::Crc32cChecksumValue>(),
              r.GetOption<storage::DisableCrc32cChecksum>(),
              r.GetOption<storage::MD5HashValue>(),
              r.GetOption<storage::DisableMD5Hash>());
        };

    ApplyQueryParameters(*context, *options, params.request);
    ApplyRoutingHeaders(*context, params.request);
    context->AddMetadata("x-goog-gcs-idempotency-token", id);
    auto rpc = stub->AsyncWriteObject(cq, std::move(context), options);
    rpc = std::make_unique<StreamingRpcTimeout>(cq, timeout, timeout,
                                                std::move(rpc));
    auto running = InsertObject::Call(
        std::move(rpc), hash_function(params.request), proto,
        WritePayloadImpl::GetImpl(params.payload), std::move(options));
    return running->Start().then([running](auto f) mutable {
      running.reset();  // extend the life of the co-routine until it co-returns
      return f.get();
    });
  };
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
      std::move(retry), std::move(backoff), idempotency, cq_, std::move(call),
      std::move(current), *std::move(proto), __func__);
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>>
AsyncConnectionImpl::ReadObject(ReadObjectParams p) {
  using ReturnType =
      StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>;
  auto current = internal::MakeImmutableOptions(std::move(p.options));
  auto proto = ToProto(p.request.impl_);
  if (!proto) return make_ready_future(ReturnType(std::move(proto).status()));

  // Get the policy factory and immediately create a policy.
  auto resume_policy =
      current->get<storage_experimental::ResumePolicyOption>()();
  // Create the hash function and validator based on the original request. Note
  // that p.request will be moved-from, so we have to do it relatively early in
  // this function.
  auto hash_function =
      std::make_shared<storage::internal::Crc32cMessageHashFunction>(
          storage::internal::CreateHashFunction(p.request.impl_));
  auto hash_validator = storage::internal::CreateHashValidator(p.request.impl_);

  auto connection_factory =
      MakeReaderConnectionFactory(std::move(current), std::move(p.request),
                                  *std::move(proto), hash_function);
  auto connection = std::make_unique<AsyncReaderConnectionResume>(
      std::move(resume_policy), std::move(hash_function),
      std::move(hash_validator), std::move(connection_factory));

  return make_ready_future(ReturnType(std::move(connection)));
}

future<StatusOr<storage_experimental::ReadPayload>>
AsyncConnectionImpl::ReadObjectRange(ReadObjectParams p) {
  auto proto = ToProto(p.request.impl_);
  if (!proto) {
    return make_ready_future(
        StatusOr<storage_experimental::ReadPayload>(std::move(proto).status()));
  }

  auto const current = internal::MakeImmutableOptions(std::move(p.options));
  auto context_factory = [current, request = std::move(p.request)]() {
    auto context = std::make_shared<grpc::ClientContext>();
    ApplyQueryParameters(*context, *current, request);
    return context;
  };
  return storage_internal::AsyncAccumulateReadObjectFull(
             cq_, stub_, std::move(context_factory), *std::move(proto), current)
      .then([current](
                future<storage_internal::AsyncAccumulateReadObjectResult> f) {
        return ToResponse(f.get(), *current);
      });
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::StartUnbufferedUpload(UploadParams p) {
  auto current = internal::MakeImmutableOptions(std::move(p.options));

  auto response = StartResumableWrite(current, p.request);
  return response.then([w = WeakFromThis(), current = std::move(current),
                        request = std::move(p.request)](auto f) mutable {
    auto self = w.lock();
    if (auto self = w.lock()) {
      return self->StartUnbufferedUploadImpl(std::move(current),
                                             std::move(request), f.get());
    }
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            internal::CancelledError("Cannot lock self", GCP_ERROR_INFO())));
  });
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::StartBufferedUpload(UploadParams p) {
  auto current = internal::MakeImmutableOptions(p.options);
  auto async_write_object = [c = current, start = p.request,
                             w = WeakFromThis()](
                                std::string const& upload_id) mutable {
    auto query = google::storage::v2::QueryWriteStatusRequest{};
    query.set_upload_id(upload_id);
    *query.mutable_common_object_request_params() =
        start.common_object_request_params();
    if (auto self = w.lock()) return self->ResumeUpload(c, std::move(query));
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

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::ResumeUnbufferedUpload(ResumeUploadParams p) {
  auto current = internal::MakeImmutableOptions(std::move(p.options));
  return ResumeUpload(current, std::move(p.request));
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::ResumeBufferedUpload(ResumeUploadParams p) {
  auto current = internal::MakeImmutableOptions(std::move(p.options));
  // This is a factory to create unbuffered writer connections. We will call
  // it and wrap its result with a BufferedWriterConnection. Cleverly (too
  // cleverly?) this wrapper calls the factory again if it needs to resume an
  // upload.
  auto make_unbuffered = [c = current, query = p.request,
                          w = WeakFromThis()]() mutable {
    if (auto self = w.lock()) return self->ResumeUpload(c, std::move(query));
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>{
            internal::CancelledError("Cannot lock self", GCP_ERROR_INFO())});
  };

  auto f = make_unbuffered();
  return f.then(
      [current = std::move(current),
       make_unbuffered = std::move(make_unbuffered)](auto f) mutable
      -> StatusOr<
          std::unique_ptr<storage_experimental::AsyncWriterConnection>> {
        auto w = f.get();
        if (!w) return std::move(w).status();
        return MakeWriterConnectionBuffered(std::move(make_unbuffered),
                                            *std::move(w), *current);
      });
}

future<StatusOr<google::storage::v2::Object>>
AsyncConnectionImpl::ComposeObject(ComposeObjectParams p) {
  auto current = internal::MakeImmutableOptions(std::move(p.options));
  auto const idempotency =
      idempotency_policy(*current)->ComposeObject(p.request);
  auto call = [stub = stub_](
                  CompletionQueue& cq,
                  std::shared_ptr<grpc::ClientContext> context,
                  google::cloud::internal::ImmutableOptions options,
                  google::storage::v2::ComposeObjectRequest const& request) {
    return stub->AsyncComposeObject(cq, std::move(context), std::move(options),
                                    request);
  };
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
      std::move(retry), std::move(backoff), idempotency, cq_, std::move(call),
      current, std::move(p.request), __func__);
}

future<Status> AsyncConnectionImpl::DeleteObject(DeleteObjectParams p) {
  auto current = internal::MakeImmutableOptions(std::move(p.options));
  auto const idempotency =
      idempotency_policy(*current)->DeleteObject(p.request);
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
      std::move(retry), std::move(backoff), idempotency, cq_,
      [stub = stub_](CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::storage::v2::DeleteObjectRequest const& proto) {
        return stub->AsyncDeleteObject(cq, std::move(context),
                                       std::move(options), proto);
      },
      std::move(current), std::move(p.request), __func__);
}

std::shared_ptr<storage_experimental::AsyncRewriterConnection>
AsyncConnectionImpl::RewriteObject(RewriteObjectParams p) {
  auto current = internal::MakeImmutableOptions(std::move(p.options));

  return std::make_shared<RewriterConnectionImpl>(
      cq_, stub_, std::move(current), std::move(p.request));
}

AsyncReaderConnectionFactory AsyncConnectionImpl::MakeReaderConnectionFactory(
    google::cloud::internal::ImmutableOptions current,
    google::cloud::storage_experimental::ReadObjectRequest request,
    google::storage::v2::ReadObjectRequest proto_request,
    std::shared_ptr<storage::internal::HashFunction> hash_function) {
  using StreamingRpc = google::cloud::internal::AsyncStreamingReadRpc<
      google::storage::v2::ReadObjectResponse>;
  using StreamingRpcTimeout =
      google::cloud::internal::AsyncStreamingReadRpcTimeout<
          google::storage::v2::ReadObjectResponse>;

  auto make_rpc = [stub = stub_, request = std::move(request)](
                      CompletionQueue& cq,
                      std::shared_ptr<grpc::ClientContext> context,
                      google::cloud::internal::ImmutableOptions options,
                      google::storage::v2::ReadObjectRequest const& proto)
      -> future<StatusOr<std::unique_ptr<StreamingRpc>>> {
    ApplyQueryParameters(*context, *options, request);
    auto timeout = ScaleStallTimeout(
        options->get<storage::DownloadStallTimeoutOption>(),
        options->get<storage::DownloadStallMinimumRateOption>(),
        google::storage::v2::ServiceConstants::MAX_READ_CHUNK_BYTES);
    auto rpc = stub->AsyncReadObject(cq, std::move(context), std::move(options),
                                     proto);
    rpc = std::make_unique<StreamingRpcTimeout>(cq, timeout, timeout,
                                                std::move(rpc));
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

  auto const* caller = __func__;
  return [caller, cq = cq_, current = std::move(current),
          make_rpc = std::move(make_rpc),
          hash_function = std::move(hash_function),
          proto_request = std::move(proto_request)](
             storage::Generation generation,
             std::int64_t received_bytes) mutable {
    UpdateGeneration(proto_request, std::move(generation));
    UpdateReadRange(proto_request, received_bytes);

    // Make this mutable, because it is called only once and we can
    // `std::move()` the captured values.
    auto transform = [current, hash_function](auto f) mutable
        -> StatusOr<
            std::unique_ptr<storage_experimental::AsyncReaderConnection>> {
      auto rpc = f.get();
      if (!rpc) return std::move(rpc).status();
      return std::unique_ptr<storage_experimental::AsyncReaderConnection>(
          std::make_unique<AsyncReaderConnectionImpl>(
              std::move(current), *std::move(rpc), std::move(hash_function)));
    };

    auto retry = retry_policy(*current);
    auto backoff = backoff_policy(*current);
    // Do not use `std::move()` for the captured variables, as we need to keep
    // such variables valid for all factory invocations.
    return google::cloud::internal::AsyncRetryLoop(
               std::move(retry), std::move(backoff), Idempotency::kIdempotent,
               cq, make_rpc, current, proto_request, caller)
        .then(std::move(transform));
  };
}

future<StatusOr<google::storage::v2::StartResumableWriteResponse>>
AsyncConnectionImpl::StartResumableWrite(
    internal::ImmutableOptions current,
    google::storage::v2::StartResumableWriteRequest request) {
  // Starting an upload is always idempotent. The side-effects, if any, are not
  // observable. That is, calling the RPC twice creates two upload sessions, but
  // there is no query, list, or any other RPC that can help you detect if the
  // first call ever happened. Not even billing, the upload id is free and
  // automatically garbage collected.
  auto const idempotency = Idempotency::kIdempotent;
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
      std::move(retry), std::move(backoff), idempotency, cq_,
      [stub = stub_](
          CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
          google::cloud::internal::ImmutableOptions options,
          google::storage::v2::StartResumableWriteRequest const& proto) {
        internal::ConfigureContext(*context, *options);
        return stub->AsyncStartResumableWrite(cq, std::move(context),
                                              std::move(options), proto);
      },
      std::move(current), std::move(request), __func__);
}

future<StatusOr<google::storage::v2::QueryWriteStatusResponse>>
AsyncConnectionImpl::QueryWriteStatus(
    internal::ImmutableOptions current,
    google::storage::v2::QueryWriteStatusRequest request) {
  // Starting a new upload is always idempotent. Any side-effects of early
  // attempts are not observable.
  auto const idempotency = Idempotency::kIdempotent;
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
      std::move(retry), std::move(backoff), idempotency, cq_,
      [stub = stub_](
          CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
          google::cloud::internal::ImmutableOptions options,
          google::storage::v2::QueryWriteStatusRequest const& proto) {
        google::cloud::internal::ConfigurePollContext(*context, *options);
        return stub->AsyncQueryWriteStatus(cq, std::move(context),
                                           std::move(options), proto);
      },
      std::move(current), std::move(request), __func__);
}

// Combines a call to QueryWriteStatus and creates an unbuffered upload.
future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::ResumeUpload(
    internal::ImmutableOptions current,
    google::storage::v2::QueryWriteStatusRequest query) {
  auto response = QueryWriteStatus(current, query);
  return response.then([w = WeakFromThis(), c = std::move(current),
                        q = std::move(query)](auto f) mutable {
    auto self = w.lock();
    if (auto self = w.lock()) {
      return self->ResumeUnbufferedUploadImpl(std::move(c), std::move(q),
                                              f.get());
    }
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            internal::CancelledError("Cannot lock self", GCP_ERROR_INFO())));
  });
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::StartUnbufferedUploadImpl(
    internal::ImmutableOptions current,
    google::storage::v2::StartResumableWriteRequest request,
    StatusOr<google::storage::v2::StartResumableWriteResponse> response) {
  if (!response) {
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            std::move(response).status()));
  }
  auto hash_function = CreateHashFunction(*current);
  auto configure =
      [current, upload = response->upload_id()](grpc::ClientContext& context) {
        ApplyResumableUploadRoutingHeader(context, upload);
      };

  auto proto = google::storage::v2::BidiWriteObjectRequest{};
  proto.set_upload_id(*response->mutable_upload_id());
  *proto.mutable_common_object_request_params() =
      std::move(*request.mutable_common_object_request_params());

  return UnbufferedUploadImpl(std::move(current), std::move(configure),
                              std::move(proto), std::move(hash_function), 0);
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::ResumeUnbufferedUploadImpl(
    internal::ImmutableOptions current,
    google::storage::v2::QueryWriteStatusRequest query,
    StatusOr<google::storage::v2::QueryWriteStatusResponse> response) {
  if (!response) {
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            std::move(response).status()));
  }
  if (response->has_resource()) {
    auto metadata = FromProto(response->resource(), *current);
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
            std::make_unique<AsyncWriterConnectionFinalized>(
                std::move(*query.mutable_upload_id()), std::move(metadata))));
  }

  // In most cases computing a hash for a resumed upload is not feasible. We
  // lack the data to initialize the hash functions. The one exception is when
  // the upload resumes from the beginning of the file.
  auto hash_function = storage::internal::CreateNullHashFunction();
  if (response->persisted_size() == 0) {
    hash_function = CreateHashFunction(*current);
  }
  auto configure =
      [current, upload_id = query.upload_id()](grpc::ClientContext& context) {
        internal::ConfigureContext(context, *current);
        ApplyResumableUploadRoutingHeader(context, upload_id);
      };
  auto proto = google::storage::v2::BidiWriteObjectRequest{};
  proto.set_upload_id(std::move(*query.mutable_upload_id()));
  *proto.mutable_common_object_request_params() =
      std::move(*query.mutable_common_object_request_params());
  return UnbufferedUploadImpl(std::move(current), std::move(configure),
                              std::move(proto), std::move(hash_function),
                              response->persisted_size());
}

future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
AsyncConnectionImpl::UnbufferedUploadImpl(
    internal::ImmutableOptions current,
    std::function<void(grpc::ClientContext&)> configure_context,
    google::storage::v2::BidiWriteObjectRequest request,
    std::shared_ptr<storage::internal::HashFunction> hash_function,
    std::int64_t persisted_size) {
  using StreamingRpc = AsyncWriterConnectionImpl::StreamingRpc;
  using StreamingRpcTimeout =
      google::cloud::internal::AsyncStreamingReadWriteRpcTimeout<
          google::storage::v2::BidiWriteObjectRequest,
          google::storage::v2::BidiWriteObjectResponse>;
  struct RequestPlaceholder {};

  auto call = [stub = stub_, configure_context = std::move(configure_context)](
                  CompletionQueue& cq,
                  std::shared_ptr<grpc::ClientContext> context,
                  google::cloud::internal::ImmutableOptions options,
                  RequestPlaceholder const&)
      -> future<StatusOr<std::unique_ptr<StreamingRpc>>> {
    configure_context(*context);
    auto timeout = ScaleStallTimeout(
        options->get<storage::TransferStallTimeoutOption>(),
        options->get<storage::TransferStallMinimumRateOption>(),
        google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);
    auto rpc =
        stub->AsyncBidiWriteObject(cq, std::move(context), std::move(options));
    rpc = std::make_unique<StreamingRpcTimeout>(cq, timeout, timeout, timeout,
                                                std::move(rpc));
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

  auto transform = [current, request = std::move(request), persisted_size,
                    hash = std::move(hash_function)](auto f) mutable
      -> StatusOr<
          std::unique_ptr<storage_experimental::AsyncWriterConnection>> {
    auto rpc = f.get();
    if (!rpc) return std::move(rpc).status();
    return std::unique_ptr<storage_experimental::AsyncWriterConnection>(
        std::make_unique<AsyncWriterConnectionImpl>(
            current, std::move(request), *std::move(rpc), std::move(hash),
            persisted_size));
  };

  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
             std::move(retry), std::move(backoff), Idempotency::kIdempotent,
             cq_, std::move(call), std::move(current), RequestPlaceholder{},
             __func__)
      .then(std::move(transform));
}

std::shared_ptr<storage_experimental::AsyncConnection> MakeAsyncConnection(
    CompletionQueue cq, Options options) {
  options = DefaultOptionsAsync(std::move(options));
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
