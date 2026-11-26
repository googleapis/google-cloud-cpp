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
#include "absl/strings/match.h"
#include "absl/strings/strip.h"
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::optional<std::uint32_t> ParseUint(std::string_view s, int base) {
  std::uint32_t out = 0;
  char const* const end = s.data() + s.size();
  std::from_chars_result const res = std::from_chars(s.data(), end, out, base);
  if (res.ec != std::errc{} || res.ptr != end) return std::nullopt;
  return out;
}

std::optional<bool> ParseIpv4(std::string_view ip) {
  std::array<std::uint32_t, 4> octets{};
  for (std::size_t i = 0; i != octets.size(); ++i) {
    std::size_t const dot = ip.find('.');
    bool const expect_dot = i + 1 != octets.size();
    if (expect_dot != (dot != std::string_view::npos)) return std::nullopt;
    std::string_view const part = expect_dot ? ip.substr(0, dot) : ip;
    std::optional<std::uint32_t> const octet = ParseUint(part, 10);
    if (!octet.has_value() || *octet > 255) return std::nullopt;
    octets[i] = *octet;
    if (expect_dot) ip.remove_prefix(dot + 1);
  }
  // DirectPath IPv4 VIP range: 34.126.0.0/18 (34.126.0.0 - 34.126.63.255).
  return octets[0] == 34 && octets[1] == 126 && octets[2] <= 63;
}

std::optional<std::uint32_t> ParseHexGroup(std::string_view part) {
  if (part.empty() || part.size() > 4) return std::nullopt;
  return ParseUint(part, 16);
}

std::optional<std::size_t> ParseIpv6Half(std::string_view part,
                                         std::array<std::uint32_t, 8>& groups) {
  if (part.empty()) return std::size_t{0};
  std::size_t count = 0;
  while (true) {
    if (count == groups.size()) return std::nullopt;
    std::size_t const colon = part.find(':');
    std::string_view const seg =
        colon == std::string_view::npos ? part : part.substr(0, colon);
    std::optional<std::uint32_t> const val = ParseHexGroup(seg);
    if (!val.has_value()) return std::nullopt;
    groups[count++] = *val;
    if (colon == std::string_view::npos) return count;
    part.remove_prefix(colon + 1);
  }
}

std::optional<std::array<std::uint32_t, 8>> ParseIpv6Groups(
    std::string_view ip, std::size_t expected_groups) {
  std::array<std::uint32_t, 8> groups{};
  std::size_t const dcolon = ip.find("::");
  if (dcolon == std::string_view::npos) {
    std::optional<std::size_t> const head_count = ParseIpv6Half(ip, groups);
    if (!head_count.has_value() || *head_count != expected_groups) {
      return std::nullopt;
    }
    return groups;
  }
  if (ip.find("::", dcolon + 2) != std::string_view::npos) {
    return std::nullopt;
  }
  std::array<std::uint32_t, 8> tail{};
  std::optional<std::size_t> const head_count =
      ParseIpv6Half(ip.substr(0, dcolon), groups);
  std::optional<std::size_t> const tail_count =
      ParseIpv6Half(ip.substr(dcolon + 2), tail);
  if (!head_count.has_value() || !tail_count.has_value() ||
      *head_count + *tail_count >= expected_groups) {
    return std::nullopt;
  }
  for (std::size_t i = 0; i != *tail_count; ++i) {
    groups[expected_groups - *tail_count + i] = tail[i];
  }
  return groups;
}

bool IsIpv4MappedPrefix(std::array<std::uint32_t, 8> const& groups) {
  return groups[0] == 0 && groups[1] == 0 && groups[2] == 0 && groups[3] == 0 &&
         groups[4] == 0 && groups[5] == 0xffff;
}

std::optional<bool> ParseIpv6(std::string_view ip) {
  if (!absl::StrContains(ip, ':')) return std::nullopt;
  if (absl::StrContains(ip, '.')) {
    std::size_t const last_colon = ip.rfind(':');
    std::optional<bool> const ipv4_directpath =
        ParseIpv4(ip.substr(last_colon + 1));
    if (!ipv4_directpath.has_value()) return std::nullopt;
    std::string_view prefix = ip.substr(0, last_colon);
    if (absl::EndsWith(prefix, ":")) {
      // Preserve `::` when the dotted-quad immediately follows `::` (e.g.
      // `::1.2.3.4`).
      prefix = ip.substr(0, last_colon + 1);
    }
    std::optional<std::array<std::uint32_t, 8>> const groups =
        ParseIpv6Groups(prefix, 6);
    if (!groups.has_value()) return std::nullopt;
    return IsIpv4MappedPrefix(*groups) && *ipv4_directpath;
  }
  std::optional<std::array<std::uint32_t, 8>> const groups =
      ParseIpv6Groups(ip, 8);
  if (!groups.has_value()) return std::nullopt;
  // Hex IPv4-mapped IPv6 (::ffff:aabb:ccdd) against 34.126.0.0/18.
  if (IsIpv4MappedPrefix(*groups)) {
    return ((*groups)[6] >> 8) == 34 && ((*groups)[6] & 0xff) == 126 &&
           ((*groups)[7] >> 8) <= 63;
  }
  // DirectPath IPv6 VIP range: 2001:4860:8040::/42.
  return (*groups)[0] == 0x2001 && (*groups)[1] == 0x4860 &&
         ((*groups)[2] & 0xFFC0) == 0x8040;
}

std::optional<std::string_view> UnwrapBrackets(std::string_view s) {
  std::string_view body = s;
  if (absl::ConsumePrefix(&body, "[")) {
    std::size_t const close = body.find(']');
    if (close != std::string_view::npos) return body.substr(0, close);
    return std::nullopt;
  }
  if (absl::ConsumePrefix(&body, "%5B") || absl::ConsumePrefix(&body, "%5b")) {
    std::size_t close = body.find("%5D");
    if (close == std::string_view::npos) close = body.find("%5d");
    if (close != std::string_view::npos) return body.substr(0, close);
    return std::nullopt;
  }
  return std::nullopt;
}

std::string_view StripZone(std::string_view ip) {
  std::size_t const zone = ip.find('%');
  return zone != std::string_view::npos ? ip.substr(0, zone) : ip;
}

std::string_view StripPeerWrapper(std::string_view peer) {
  if (absl::ConsumePrefix(&peer, "ipv4:")) {
    std::size_t const colon = peer.rfind(':');
    return colon != std::string_view::npos ? peer.substr(0, colon) : peer;
  }
  if (absl::ConsumePrefix(&peer, "ipv6:")) {
    std::optional<std::string_view> const unwrapped = UnwrapBrackets(peer);
    return StripZone(unwrapped.value_or(peer));
  }
  if (std::optional<std::string_view> const unwrapped = UnwrapBrackets(peer);
      unwrapped.has_value()) {
    return StripZone(*unwrapped);
  }
  std::size_t const colon = peer.find(':');
  if (colon != std::string_view::npos && peer.rfind(':') == colon) {
    return peer.substr(0, colon);
  }
  return StripZone(peer);
}

}  // namespace

std::string NormalizeServerAddress(std::string_view endpoint) {
  std::size_t const scheme_pos = endpoint.find("://");
  if (scheme_pos != std::string_view::npos) {
    std::string_view const scheme = endpoint.substr(0, scheme_pos);
    endpoint.remove_prefix(scheme_pos + 3);
    if (absl::EqualsIgnoreCase(scheme, "dns") ||
        absl::EqualsIgnoreCase(scheme, "xds") ||
        absl::EqualsIgnoreCase(scheme, "google-c2p") ||
        absl::EqualsIgnoreCase(scheme, "google-c2p-experimental")) {
      std::size_t const slash = endpoint.find('/');
      if (slash != std::string_view::npos) endpoint.remove_prefix(slash + 1);
    }
  } else {
    for (std::string_view const prefix :
         {"dns:", "xds:", "google-c2p:", "google-c2p-experimental:"}) {
      if (absl::StartsWithIgnoreCase(endpoint, prefix)) {
        endpoint.remove_prefix(prefix.size());
        break;
      }
    }
  }
  std::size_t const cut = endpoint.find_first_of("/?#");
  if (cut != std::string_view::npos) endpoint = endpoint.substr(0, cut);
  std::size_t const at = endpoint.rfind('@');
  if (at != std::string_view::npos) endpoint.remove_prefix(at + 1);
  if (absl::ConsumePrefix(&endpoint, "[")) {
    std::size_t const close = endpoint.find(']');
    if (close != std::string_view::npos) endpoint = endpoint.substr(0, close);
    return std::string(endpoint);
  }
  std::size_t const colon = endpoint.find(':');
  if (colon != std::string_view::npos && endpoint.rfind(':') == colon) {
    endpoint = endpoint.substr(0, colon);
  }
  return std::string(endpoint);
}

std::string_view NetworkPathOfPeer(std::string_view peer) {
  if (peer.empty()) return "unknown";
  std::string_view const ip = StripPeerWrapper(peer);
  if (ip.empty()) return "unknown";
  if (std::optional<bool> const is_directpath = ParseIpv4(ip);
      is_directpath.has_value()) {
    return *is_directpath ? "directpath" : "cloudpath";
  }
  if (std::optional<bool> const is_directpath = ParseIpv6(ip);
      is_directpath.has_value()) {
    return *is_directpath ? "directpath" : "cloudpath";
  }
  return "unknown";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
