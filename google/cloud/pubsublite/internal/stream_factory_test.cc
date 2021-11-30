#include "google/cloud/pubsublite/internal/stream_factory.h"

#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite_internal {

TEST(StreamFactoryTest, CreateStreams) {
    CompletionQueue queue;
    ClientMetadata metadata{{"key1", "value1"}, {"key2", "value2"}};
    auto publish_factory = MakeStreamFactory(std::shared_ptr<pubsublite::v1::PublisherService::StubInterface>(nullptr), queue, metadata);
    auto subscribe_factory = MakeStreamFactory(std::shared_ptr<pubsublite::v1::SubscriberService::StubInterface>(nullptr), queue, metadata);
    auto cursor_factory = MakeStreamFactory(std::shared_ptr<pubsublite::v1::CursorService::StubInterface>(nullptr), queue, metadata);
    auto assignment_factory = MakeStreamFactory(std::shared_ptr<pubsublite::v1::PartitionAssignmentService::StubInterface>(nullptr), queue, metadata);
}

}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
