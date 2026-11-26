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
#include <ostream>
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

struct StringCase {
  std::string_view input;
  std::string_view expected;
};

std::ostream& operator<<(std::ostream& os, StringCase const& c) {
  return os << "{input=\"" << c.input << "\", expected=\"" << c.expected
            << "\"}";
}

class NormalizeServerAddressTest : public ::testing::TestWithParam<StringCase> {
};

TEST_P(NormalizeServerAddressTest, NormalizesEndpoint) {
  StringCase const& c = GetParam();
  EXPECT_EQ(NormalizeServerAddress(c.input), c.expected);
}

INSTANTIATE_TEST_SUITE_P(
    MetricsAttributes, NormalizeServerAddressTest,
    ::testing::Values(
        StringCase{"", ""},
        StringCase{"storage.googleapis.com", "storage.googleapis.com"},
        StringCase{"storage.googleapis.com:443", "storage.googleapis.com"},
        StringCase{"https://storage.googleapis.com:443/v1",
                   "storage.googleapis.com"},
        StringCase{"https://user:pass@storage.googleapis.com:443/v1",
                   "storage.googleapis.com"},
        StringCase{"http://localhost:8080/storage/v1", "localhost"},
        StringCase{"dns:///storage.googleapis.com:443",
                   "storage.googleapis.com"},
        StringCase{"DNS:///storage.googleapis.com:443",
                   "storage.googleapis.com"},
        StringCase{"dns://8.8.8.8/storage.googleapis.com:443",
                   "storage.googleapis.com"},
        StringCase{"dns:storage.googleapis.com", "storage.googleapis.com"},
        StringCase{"dns:storage.googleapis.com:443", "storage.googleapis.com"},
        StringCase{"xds:///storage.googleapis.com", "storage.googleapis.com"},
        StringCase{"xds://authority/storage.googleapis.com:443",
                   "storage.googleapis.com"},
        StringCase{"xds:storage.googleapis.com", "storage.googleapis.com"},
        StringCase{"google-c2p:///storage.googleapis.com",
                   "storage.googleapis.com"},
        StringCase{"google-c2p:storage.googleapis.com",
                   "storage.googleapis.com"},
        StringCase{
            "google-c2p-experimental:///storage.googleapis.com?force-xds",
            "storage.googleapis.com"},
        StringCase{"google-c2p-experimental:storage.googleapis.com:443",
                   "storage.googleapis.com"},
        StringCase{"https://storage.us-central1.rep.googleapis.com",
                   "storage.us-central1.rep.googleapis.com"},
        StringCase{"https://storage.googleapis.com#frag",
                   "storage.googleapis.com"},
        StringCase{"https://[::1]:8080/path", "::1"},
        StringCase{"[::1]:8080", "::1"},
        StringCase{"2001:4860:8040::1", "2001:4860:8040::1"}));

class NetworkPathOfPeerTest : public ::testing::TestWithParam<StringCase> {};

TEST_P(NetworkPathOfPeerTest, ClassifiesPeer) {
  StringCase const& c = GetParam();
  EXPECT_EQ(NetworkPathOfPeer(c.input), c.expected);
}

INSTANTIATE_TEST_SUITE_P(
    MetricsAttributes, NetworkPathOfPeerTest,
    ::testing::Values(
        StringCase{"ipv4:34.126.0.1:443", "directpath"},
        StringCase{"ipv4:34.126.0.1", "directpath"},
        StringCase{"34.126.0.1", "directpath"},
        StringCase{"ipv4:34.126.63.255:443", "directpath"},
        StringCase{"34.126.10.20:443", "directpath"},
        StringCase{"ipv6:[2001:4860:8040::1]:443", "directpath"},
        StringCase{"ipv6:2001:4860:8040::1", "directpath"},
        StringCase{"[2001:4860:8040::1]:443", "directpath"},
        StringCase{"2001:4860:8040:0:0:0:0:1", "directpath"},
        StringCase{"2001:4860:8040::1%eth0", "directpath"},
        StringCase{"ipv6:%5B2001:4860:8040::1%5D:443", "directpath"},
        StringCase{"ipv6:%5b2001:4860:8040::1%5d:443", "directpath"},
        StringCase{"ipv6:%5B2001:4860:807f::1%25eth0%5D:443", "directpath"},
        StringCase{"ipv6:[::ffff:34.126.0.1]:443", "directpath"},
        StringCase{"ipv6:[::FfFf:34.126.0.1]:443", "directpath"},
        StringCase{"0:0:0:0:0:ffff:34.126.0.1", "directpath"},
        StringCase{"::0:ffff:34.126.0.1", "directpath"},
        StringCase{"ipv6:[::ffff:227e:1]:443", "directpath"},
        StringCase{"ipv6:[::ffff:227e:3fff]:443", "directpath"},
        StringCase{"0:0:0:0:0:ffff:227e:1", "directpath"},
        StringCase{"ipv4:35.126.0.1:443", "cloudpath"},
        StringCase{"ipv4:34.125.255.255:443", "cloudpath"},
        StringCase{"ipv4:34.126.64.0:443", "cloudpath"},
        StringCase{"ipv4:142.250.190.78:443", "cloudpath"},
        StringCase{"ipv6:[2002:4860:8040::1]:443", "cloudpath"},
        StringCase{"ipv6:[2001:4861:8040::1]:443", "cloudpath"},
        StringCase{"ipv6:%5B2001:4860:803f::1%5D:443", "cloudpath"},
        StringCase{"ipv6:%5B2001:4860:8080::1%5D:443", "cloudpath"},
        StringCase{"ipv6:[::ffff:142.250.190.78]:443", "cloudpath"},
        StringCase{"::34.126.0.1", "cloudpath"},
        StringCase{"ipv6:[::ffff:227e:4000]:443", "cloudpath"},
        StringCase{"ipv6:[::ffff:237e:1]:443", "cloudpath"},
        StringCase{"ipv6:[::ffff:227d:1]:443", "cloudpath"},
        StringCase{"1:0:0:0:0:ffff:227e:1", "cloudpath"},
        StringCase{"0:1:0:0:0:ffff:227e:1", "cloudpath"},
        StringCase{"0:0:1:0:0:ffff:227e:1", "cloudpath"},
        StringCase{"0:0:0:1:0:ffff:227e:1", "cloudpath"},
        StringCase{"0:0:0:0:1:ffff:227e:1", "cloudpath"},
        StringCase{"0:0:0:0:0:fffe:227e:1", "cloudpath"},
        StringCase{"::ffff:1", "cloudpath"}, StringCase{"::1", "cloudpath"},
        StringCase{"", "unknown"}, StringCase{"ipv4:", "unknown"},
        StringCase{"unix:/tmp/sock", "unknown"},
        StringCase{"not-an-ip", "unknown"},
        StringCase{"ipv4:999.0.0.1:443", "unknown"},
        StringCase{"::ffff:999.0.0.1", "unknown"},
        StringCase{"1:2:34.126.0.1", "unknown"},
        StringCase{"34..0.1", "unknown"},
        StringCase{"4294967296.0.0.1", "unknown"},
        StringCase{"34.126.0.1a", "unknown"},
        StringCase{"ipv6:[2001:4860:80400::1]:443", "unknown"},
        StringCase{"ipv6:[2001:4860:8040:]:443", "unknown"},
        StringCase{"ipv6:[2001:4860:8040::1", "unknown"},
        StringCase{"[2001:4860:8040::1", "unknown"},
        StringCase{"ipv6:%5B2001:4860:8040::1", "unknown"},
        StringCase{"1:2:3:4:5:6:7", "unknown"},
        StringCase{"1:2:3:4:5:6:7:8:9", "unknown"},
        StringCase{"1:2:3:4::5:6:7:8", "unknown"},
        StringCase{"2001::4860::1", "unknown"}, StringCase{":::", "unknown"}));

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
