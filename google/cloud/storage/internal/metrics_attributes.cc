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
#include "absl/strings/strip.h"
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

bool ParseUint(std::string_view s, int base, std::uint32_t& out) {
  if (s.empty()) return false;
  auto const* end = s.data() + s.size();
  auto const res = std::from_chars(s.data(), end, out, base);
  return res.ec == std::errc{} && res.ptr == end;
}

// Returns true if `ip` is a valid IPv4 address; sets `is_directpath` when in
// 34.126.0.0/18 (34.126.0.0 - 34.126.63.255).
bool ParseIpv4(std::string_view ip, bool& is_directpath) {
  std::uint32_t octets[4] = {};
  for (int i = 0; i < 4; ++i) {
    auto const dot = ip.find('.');
    if ((i < 3) != (dot != std::string_view::npos)) return false;
    auto const part = (i < 3) ? ip.substr(0, dot) : ip;
    if (!ParseUint(part, 10, octets[i]) || octets[i] > 255) return false;
    if (i < 3) ip.remove_prefix(dot + 1);
  }
  is_directpath = octets[0] == 34 && octets[1] == 126 && octets[2] <= 63;
  return true;
}

bool IsValidIpv6Chars(std::string_view ip) {
  int colons = 0;
  for (char const c : ip) {
    if (c == ':') {
      ++colons;
    } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                 (c >= 'A' && c <= 'F'))) {
      return false;
    }
  }
  return colons >= 2;
}

bool ParseIpv6(std::string_view ip, bool& is_directpath) {
  if (!IsValidIpv6Chars(ip)) return false;
  is_directpath = false;
  std::uint32_t groups[3] = {};
  for (int i = 0; i < 3; ++i) {
    auto const colon = ip.find(':');
    if (colon == std::string_view::npos) return true;
    if (!ParseUint(ip.substr(0, colon), 16, groups[i]) || groups[i] > 0xFFFF) {
      return true;
    }
    ip.remove_prefix(colon + 1);
  }
  is_directpath = groups[0] == 0x2001 && groups[1] == 0x4860 &&
                  (groups[2] & 0xFFC0) == 0x8040;
  return true;
}

std::string_view StripPeerWrapper(std::string_view peer) {
  if (absl::ConsumePrefix(&peer, "ipv4:")) {
    auto const colon = peer.rfind(':');
    return colon != std::string_view::npos ? peer.substr(0, colon) : peer;
  }
  absl::ConsumePrefix(&peer, "ipv6:");
  if (absl::ConsumePrefix(&peer, "[")) {
    auto const close = peer.find(']');
    if (close != std::string_view::npos) return peer.substr(0, close);
  }
  return peer;
}

}  // namespace

std::string NormalizeServerAddress(std::string_view endpoint) {
  auto const scheme = endpoint.find("://");
  if (scheme != std::string_view::npos) {
    endpoint.remove_prefix(scheme + 3);
    absl::ConsumePrefix(&endpoint, "/");
  }
  auto const cut = endpoint.find_first_of("/?#");
  if (cut != std::string_view::npos) endpoint = endpoint.substr(0, cut);
  if (absl::ConsumePrefix(&endpoint, "[")) {
    auto const close = endpoint.find(']');
    if (close != std::string_view::npos) endpoint = endpoint.substr(0, close);
    return std::string(endpoint);
  }
  auto const colon = endpoint.find(':');
  if (colon != std::string_view::npos && endpoint.rfind(':') == colon) {
    endpoint = endpoint.substr(0, colon);
  }
  return std::string(endpoint);
}

std::string_view NetworkPathOfPeer(std::string_view peer) {
  if (peer.empty()) return "unknown";
  auto const ip = StripPeerWrapper(peer);
  if (ip.empty()) return "unknown";
  bool is_directpath = false;
  if (ParseIpv4(ip, is_directpath) || ParseIpv6(ip, is_directpath)) {
    return is_directpath ? "directpath" : "cloudpath";
  }
  return "unknown";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
