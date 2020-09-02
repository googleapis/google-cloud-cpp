#include "google/cloud/storage/client.h"
#include <gmock/gmock.h>
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {

int main() {
  const auto& client_options =
      ClientOptions(oauth2::CreateAnonymousCredentials());

  Client client(client_options);

  auto stream = client.WriteObject("mock-bucket-name", "mock-object-name");
  stream << "Bring me to work!";

  const auto& session_url = stream.resumable_session_id();
  const auto& client_status =
      client.DeleteResumableUpload(session_url, client_options);

  stream.Close();

  return 0;
}
}  // namespace STORAGE_CLIENT_NS

}  // namespace storage
}  // namespace cloud
}  // namespace google
