// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_WRAPPER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_WRAPPER_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/debug_future_status.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/debug_string_protobuf.h"
#include "google/cloud/internal/debug_string_status.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <google/protobuf/message.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <string>
#include <type_traits>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename T>
struct IsStatusOr : public std::false_type {};
template <typename T>
struct IsStatusOr<StatusOr<T>> : public std::true_type {};

template <typename T>
struct IsUniquePtr : public std::false_type {};
template <typename T>
struct IsUniquePtr<std::unique_ptr<T>> : public std::true_type {};

template <typename T>
struct IsFutureStatusOr : public std::false_type {};
template <typename T>
struct IsFutureStatusOr<future<StatusOr<T>>> : public std::true_type {};

template <typename T>
struct IsFutureStatus : public std::false_type {};
template <>
struct IsFutureStatus<future<Status>> : public std::true_type {};

Status LogResponse(Status response, absl::string_view where,
                   absl::string_view args, TracingOptions const& options);

template <typename T>
StatusOr<T> LogResponse(StatusOr<T> response, absl::string_view where,
                        absl::string_view args, TracingOptions const& options) {
  if (!response) {
    return LogResponse(std::move(response).status(), where, args, options);
  }
  GCP_LOG(DEBUG) << where << args
                 << " >> response=" << DebugString(*response, options);
  return response;
}

void LogResponseFuture(std::future_status status, absl::string_view where,
                       absl::string_view args, TracingOptions const& options);

future<Status> LogResponse(future<Status> response, std::string where,
                           std::string args, TracingOptions const& options);

template <typename T>
future<StatusOr<T>> LogResponse(future<StatusOr<T>> response, std::string where,
                                std::string args, TracingOptions options) {
  LogResponseFuture(response.wait_for(std::chrono::microseconds(0)), where,
                    args, options);
  return response.then([where = std::move(where), args = std::move(args),
                        options = std::move(options)](auto f) {
    return LogResponse(f.get(), where, args, options);
  });
}

void LogResponsePtr(bool not_null, absl::string_view where,
                    absl::string_view args, TracingOptions const& options);

template <typename T>
std::unique_ptr<T> LogResponse(std::unique_ptr<T> response,
                               absl::string_view where, absl::string_view args,
                               TracingOptions const& options) {
  LogResponsePtr(response, where, args, options);
  return response;
}

template <
    typename Functor, typename Request, typename Context,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, Context&, Request const&>,
    typename std::enable_if<std::is_same<Result, google::cloud::Status>::value,
                            int>::type = 0>
Result LogWrapper(Functor&& functor, Context& context, Request const& request,
                  char const* where, TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  return LogResponse(functor(context, request), where, "()", options);
}

template <typename Functor, typename Request, typename Context,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, Context&, Request const&>,
          typename std::enable_if<IsStatusOr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, Context& context, Request const& request,
                  char const* where, TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  return LogResponse(functor(context, request), where, "()", options);
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, std::shared_ptr<grpc::ClientContext>, Request const&>,
          typename std::enable_if<IsUniquePtr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor,
                  std::shared_ptr<grpc::ClientContext> context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  return LogResponse(functor(std::move(context), request), where, "()",
                     options);
}

template <
    typename Functor, typename Request,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, grpc::ClientContext&, Request const&, grpc::CompletionQueue*>>
Result LogWrapper(Functor&& functor, grpc::ClientContext& context,
                  Request const& request, grpc::CompletionQueue* cq,
                  char const* where, TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  return LogResponse(functor(context, request, cq), where, "()", options);
}

template <
    typename Functor, typename Request,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, google::cloud::CompletionQueue&,
        std::shared_ptr<grpc::ClientContext>, Request const&>,
    typename std::enable_if<IsFutureStatusOr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, google::cloud::CompletionQueue& cq,
                  std::shared_ptr<grpc::ClientContext> context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  // Because this is an asynchronous request we need a unique identifier so
  // applications can match the request and response in the log.
  auto args = "(" + RequestIdForLogging() + ")";
  GCP_LOG(DEBUG) << where << args << " << " << DebugString(request, options);
  return LogResponse(functor(cq, std::move(context), request), where,
                     std::move(args), options);
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, google::cloud::CompletionQueue&,
              std::shared_ptr<grpc::ClientContext>, Request const&>,
          typename std::enable_if<IsFutureStatus<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, google::cloud::CompletionQueue& cq,
                  std::shared_ptr<grpc::ClientContext> context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  // Because this is an asynchronous request we need a unique identifier so
  // applications can match the request and response in the log.
  auto args = "(" + RequestIdForLogging() + ")";
  GCP_LOG(DEBUG) << where << args << " << " << DebugString(request, options);
  return LogResponse(functor(cq, std::move(context), request), where,
                     std::move(args), options);
}

template <typename Functor, typename Request, typename Context,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, google::cloud::CompletionQueue&,
              std::unique_ptr<Context>, Request const&>,
          typename std::enable_if<IsFutureStatus<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, google::cloud::CompletionQueue& cq,
                  std::unique_ptr<Context> context, Request const& request,
                  char const* where, TracingOptions const& options) {
  // Because this is an asynchronous request we need a unique identifier so
  // applications can match the request and response in the log.
  auto args = "(" + RequestIdForLogging() + ")";
  GCP_LOG(DEBUG) << where << args << " << " << DebugString(request, options);
  return LogResponse(functor(cq, std::move(context), request), where,
                     std::move(args), options);
}

template <
    typename Functor, typename Request, typename Context,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, google::cloud::CompletionQueue&, std::unique_ptr<Context>,
        Request const&>,
    typename std::enable_if<IsFutureStatusOr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, google::cloud::CompletionQueue& cq,
                  std::unique_ptr<Context> context, Request const& request,
                  char const* where, TracingOptions const& options) {
  // Because this is an asynchronous request we need a unique identifier so
  // applications can match the request and response in the log.
  auto args = "(" + RequestIdForLogging() + ")";
  GCP_LOG(DEBUG) << where << args << " << " << DebugString(request, options);
  return LogResponse(functor(cq, std::move(context), request), where,
                     std::move(args), options);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_WRAPPER_H
