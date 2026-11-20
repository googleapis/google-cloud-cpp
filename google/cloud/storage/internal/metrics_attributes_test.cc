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

#include "google/cloud/storage/internal/metrics_attributes.h"
#include <gtest/gtest.h>
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(MetricsAttributes, NormalizeServerAddress) {
  struct Case {
    std::string_view in;
    std::string_view out;
  } const cases[] = {
      {"", ""},
      {"storage.googleapis.com", "storage.googleapis.com"},
      {"storage.googleapis.com:443", "storage.googleapis.com"},
      {"https://storage.googleapis.com:443/v1", "storage.googleapis.com"},
      {"https://user:pass@storage.googleapis.com:443/v1",
       "storage.googleapis.com"},
      {"http://localhost:8080/storage/v1", "localhost"},
      {"dns:///storage.googleapis.com:443", "storage.googleapis.com"},
      {"dns://8.8.8.8/storage.googleapis.com:443", "storage.googleapis.com"},
      {"google-c2p:///storage.googleapis.com", "storage.googleapis.com"},
      {"google-c2p-experimental:///storage.googleapis.com?force-xds",
       "storage.googleapis.com"},
      {"https://storage.us-central1.rep.googleapis.com",
       "storage.us-central1.rep.googleapis.com"},
      {"https://[::1]:8080/path", "::1"},
      {"2001:4860:8040::1", "2001:4860:8040::1"},
  };
  for (auto const& c : cases) {
    EXPECT_EQ(NormalizeServerAddress(c.in), c.out) << "in=" << c.in;
  }
}

TEST(MetricsAttributes, NetworkPathOfPeer) {
  struct Case {
    std::string_view peer;
    std::string_view expected;
  } const cases[] = {
      {"ipv4:34.126.0.1:443", "directpath"},
      {"ipv4:34.126.63.255:443", "directpath"},
      {"34.126.10.20:443", "directpath"},
      {"ipv6:[2001:4860:8040::1]:443", "directpath"},
      {"[2001:4860:8040::1]:443", "directpath"},
      {"2001:4860:8040:0:0:0:0:1", "directpath"},
      {"ipv6:%5B2001:4860:8040::1%5D:443", "directpath"},
      {"ipv6:%5b2001:4860:8040::1%5d:443", "directpath"},
      {"ipv6:%5B2001:4860:807f::1%25eth0%5D:443", "directpath"},
      {"ipv6:[::ffff:34.126.0.1]:443", "directpath"},
      {"ipv4:34.125.255.255:443", "cloudpath"},
      {"ipv4:34.126.64.0:443", "cloudpath"},
      {"ipv4:142.250.190.78:443", "cloudpath"},
      {"ipv6:%5B2001:4860:803f::1%5D:443", "cloudpath"},
      {"ipv6:%5B2001:4860:8080::1%5D:443", "cloudpath"},
      {"ipv6:[::ffff:142.250.190.78]:443", "cloudpath"},
      {"::1", "cloudpath"},
      {"", "unknown"},
      {"unix:/tmp/sock", "unknown"},
      {"not-an-ip", "unknown"},
      {"ipv4:999.0.0.1:443", "unknown"},
      {"ipv6:[2001:4860:80400::1]:443", "unknown"},
      {"ipv6:[2001:4860:8040:]:443", "unknown"},
      {"ipv6:[2001:4860:8040::1", "unknown"},
      {"2001::4860::1", "unknown"},
      {":::", "unknown"},
  };
  for (auto const& c : cases) {
    EXPECT_EQ(NetworkPathOfPeer(c.peer), c.expected) << "peer=" << c.peer;
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
