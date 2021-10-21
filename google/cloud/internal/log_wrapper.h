// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
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
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <google/protobuf/message.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::string DebugString(google::protobuf::Message const& m,
                        TracingOptions const& options);

char const* DebugFutureStatus(std::future_status s);

// Create a unique ID that can be used to match asynchronous requests/response
// pairs.
std::string RequestIdForLogging();

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

template <
    typename Functor, typename Request,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, grpc::ClientContext&, Request const&>,
    typename std::enable_if<std::is_same<Result, google::cloud::Status>::value,
                            int>::type = 0>
Result LogWrapper(Functor&& functor, grpc::ClientContext& context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto response = functor(context, request);
  GCP_LOG(DEBUG) << where << "() >> status=" << response;
  return response;
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, grpc::ClientContext&, Request const&>,
          typename std::enable_if<IsStatusOr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, grpc::ClientContext& context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto response = functor(context, request);
  if (!response) {
    GCP_LOG(DEBUG) << where << "() >> status=" << response.status();
  } else {
    GCP_LOG(DEBUG) << where
                   << "() >> response=" << DebugString(*response, options);
  }
  return response;
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, grpc::ClientContext&, Request const&>,
          typename std::enable_if<IsUniquePtr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, grpc::ClientContext& context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto response = functor(context, request);
  GCP_LOG(DEBUG) << where << "() >> " << (response ? "not null" : "null")
                 << " stream";
  return response;
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, std::unique_ptr<grpc::ClientContext>, Request const&>,
          typename std::enable_if<IsUniquePtr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor,
                  std::unique_ptr<grpc::ClientContext> context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto response = functor(std::move(context), request);
  GCP_LOG(DEBUG) << where << "() >> " << (response ? "not null" : "null")
                 << " stream";
  return response;
}

template <
    typename Functor, typename Request,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, grpc::ClientContext&, Request const&, grpc::CompletionQueue*>>
Result LogWrapper(Functor&& functor, grpc::ClientContext& context,
                  Request const& request, grpc::CompletionQueue* cq,
                  char const* where, TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto response = functor(context, request, cq);
  GCP_LOG(DEBUG) << where << "() >> " << (response ? "not null" : "null")
                 << " async response reader";
  return response;
}

template <
    typename Functor, typename Request,
    typename Result =
        google::cloud::internal::invoke_result_t<Functor, Request>,
    typename std::enable_if<IsFutureStatusOr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, Request request, char const* where,
                  TracingOptions const& options) {
  // Because this is an asynchronous request we need a unique identifier so
  // applications can match the request and response in the log.
  auto prefix = std::string(where) + "(" + RequestIdForLogging() + ")";
  GCP_LOG(DEBUG) << prefix << " << " << DebugString(request, options);
  auto response = functor(std::move(request));
  // Ideally we would have an ID to match the request with the asynchronous
  // response, but for functions with this signature there is nothing that comes
  // to mind.
  GCP_LOG(DEBUG) << prefix << " >> future_status="
                 << DebugFutureStatus(
                        response.wait_for(std::chrono::microseconds(0)));
  return response.then([prefix, options](decltype(response) f) {
    auto response = f.get();
    if (!response) {
      GCP_LOG(DEBUG) << prefix << " >> status=" << response.status();
    } else {
      GCP_LOG(DEBUG) << prefix
                     << " >> response=" << DebugString(*response, options);
    }
    return response;
  });
}

template <
    typename Functor, typename Request,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, google::cloud::CompletionQueue&,
        std::unique_ptr<grpc::ClientContext>, Request const&>,
    typename std::enable_if<IsFutureStatusOr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, google::cloud::CompletionQueue& cq,
                  std::unique_ptr<grpc::ClientContext> context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  // Because this is an asynchronous request we need a unique identifier so
  // applications can match the request and response in the log.
  auto prefix = std::string(where) + "(" + RequestIdForLogging() + ")";
  GCP_LOG(DEBUG) << prefix << " << " << DebugString(request, options);
  auto response = functor(cq, std::move(context), request);
  // Ideally we would have an ID to match the request with the asynchronous
  // response, but for functions with this signature there is nothing that comes
  // to mind.
  GCP_LOG(DEBUG) << prefix << " >> future_status="
                 << DebugFutureStatus(
                        response.wait_for(std::chrono::microseconds(0)));
  return response.then([prefix, options](decltype(response) f) {
    auto response = f.get();
    if (!response) {
      GCP_LOG(DEBUG) << prefix << " >> status=" << response.status();
    } else {
      GCP_LOG(DEBUG) << prefix
                     << " >> response=" << DebugString(*response, options);
    }
    return response;
  });
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>, Request const&>,
          typename std::enable_if<IsFutureStatus<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, google::cloud::CompletionQueue& cq,
                  std::unique_ptr<grpc::ClientContext> context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  // Because this is an asynchronous request we need a unique identifier so
  // applications can match the request and response in the log.
  auto prefix = std::string(where) + "(" + RequestIdForLogging() + ")";
  GCP_LOG(DEBUG) << prefix << " << " << DebugString(request, options);
  auto response = functor(cq, std::move(context), request);
  // Ideally we would have an ID to match the request with the asynchronous
  // response, but for functions with this signature there is nothing that comes
  // to mind.
  GCP_LOG(DEBUG) << prefix << " >> future_status="
                 << DebugFutureStatus(
                        response.wait_for(std::chrono::microseconds(0)));
  return response.then([prefix](future<Status> f) {
    auto response = f.get();
    GCP_LOG(DEBUG) << prefix << " >> response=" << response;
    return response;
  });
}

template <typename Functor, typename Request, typename Response,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, grpc::ClientContext*, Request const&, Response*>,
          typename std::enable_if<std::is_same<Result, grpc::Status>::value,
                                  int>::type = 0>
Result LogWrapper(Functor&& functor, grpc::ClientContext* context,
                  Request const& request, Response* response, char const* where,
                  TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto status = functor(context, request, response);
  if (!status.ok()) {
    GCP_LOG(DEBUG) << where << "() >> status=" << status.error_message();
  } else {
    GCP_LOG(DEBUG) << where << "() << " << DebugString(*response, options);
  }
  return status;
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, grpc::ClientContext*, Request const&>,
          typename std::enable_if<IsUniquePtr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, grpc::ClientContext* context,
                  Request const& request, char const* where,
                  TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto response = functor(context, request);
  GCP_LOG(DEBUG) << where << "() >> " << (response ? "not null" : "null")
                 << " stream";
  return response;
}

template <
    typename Functor, typename Request,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, grpc::ClientContext*, Request const&, grpc::CompletionQueue*>,
    typename std::enable_if<IsUniquePtr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, grpc::ClientContext* context,
                  Request const& request, grpc::CompletionQueue* cq,
                  char const* where, TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto response = functor(context, request, cq);
  GCP_LOG(DEBUG) << where << "() >> " << (response ? "not null" : "null")
                 << " async response reader";
  return response;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_LOG_WRAPPER_H
