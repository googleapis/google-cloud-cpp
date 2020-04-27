//
// Created by coryan on 2/6/20.
//

#include "google/cloud/pubsub/internal/user_agent_prefix.h"
#include "google/cloud/internal/compiler_info.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::string UserAgentPrefix() {
  return "gcloud-cpp/" + google::cloud::pubsub::VersionString() + " (" +
         ::google::cloud::internal::CompilerId() + "-" +
         ::google::cloud::internal::CompilerVersion() + "; " +
         ::google::cloud::internal::CompilerFeatures() + ")";
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
