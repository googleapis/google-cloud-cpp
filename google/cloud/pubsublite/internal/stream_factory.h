#include <google/cloud/pubsublite/v1/publisher.grpc.pb.h>

namespace google {
namespace cloud {
namespace pubsublite_internal {

template <class Request, class Response>
using BidiStream = google::cloud::internal::AsyncStreamingReadWriteRpc<Request, Response>;

template <class Request, class Response>
using StreamFactory = std::function<std::unique_ptr<BidiStream<Request, Response>>()>;

using ClientMetadata = absl::flat_hash_map<std::string, std::string>;

namespace stream_factory_internal {

inline std::unique_ptr<grpc::ClientContext> MakeGrpcClientContext(const ClientMetadata& metadata) {
    auto context = std::make_unique<grpc::ClientContext>();
    for (const auto& [k, v] : metadata) {
        context->AddMetadata(k, v);
    }
    return context;
}

}  // namespace stream_factory_internal

inline StreamFactory<pubsublite::v1::PublishRequest, pubsublite::v1::PublishResponse> MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::grpc::PublisherService::StubInterface> stub,
    google::cloud::CompletionQueue& cq, ClientMetadata metadata = {}) {
        return [stub, &cq, metadata=std::move(metadata)]{
            return internal::MakeStreamingReadWriteRpc<
                pubsublite::v1::PublishRequest, pubsublite::v1::PublishResponse>(
                    cq, stream_factory_internal::MakeGrpcClientContext(metadata),
                    [this, stub](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
                        return stub->PrepareAsyncPublish(context, cq);
                    });
        };
    }

inline StreamFactory<pubsublite::v1::SubscribeRequest, pubsublite::v1::SubscribeResponse> MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::grpc::SubscriberService::StubInterface> stub,
    google::cloud::CompletionQueue& cq, ClientMetadata metadata = {}) {
        return [stub, &cq, metadata=std::move(metadata)]{
            return google::cloud::internal::MakeStreamingReadWriteRpc<
                pubsublite::v1::SubscribeRequest, pubsublite::v1::SubscribeResponse>(
                    cq, stream_factory_internal::MakeGrpcClientContext(metadata),
                    [this, stub](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
                        return stub->PrepareAsyncSubscribe(context, cq);
                    });
        };
    }



inline StreamFactory<pubsublite::v1::StreamingCommitCursorRequest, pubsublite::v1::StreamingCommitCursorResponse> MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::grpc::CursorService::StubInterface> stub,
    google::cloud::CompletionQueue& cq, ClientMetadata metadata = {}) {
        return [stub, &cq, metadata=std::move(metadata)]{
            return google::cloud::internal::MakeStreamingReadWriteRpc<
                pubsublite::v1::StreamingCommitCursorRequest, pubsublite::v1::StreamingCommitCursorResponse>(
                    cq, stream_factory_internal::MakeGrpcClientContext(metadata),
                    [this, stub](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
                        return stub->PrepareAsyncStreamingCommitCursor(context, cq);
                    });
        };
    }


inline StreamFactory<pubsublite::v1::PartitionAssignmentRequest, pubsublite::v1::PartitionAssignment> MakeStreamFactory(
    std::shared_ptr<pubsublite::v1::grpc::PartitionAssignmentService::StubInterface> stub,
    google::cloud::CompletionQueue& cq, ClientMetadata metadata = {}) {
        return [stub, &cq, metadata=std::move(metadata)]{
            return google::cloud::internal::MakeStreamingReadWriteRpc<
                pubsublite::v1::PartitionAssignmentRequest, pubsublite::v1::PartitionAssignment>(
                    cq, stream_factory_internal::MakeGrpcClientContext(metadata),
                    [this, stub](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
                        return stub->PrepareAsyncAssignPartitions(context, cq);
                    });
        };
    }

}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
