// Copyright 2021 Google LLC
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

#include "generate_svg_badge.h"
#include <fmt/core.h>
#include <algorithm>
#include <cctype>

std::string GenerateSvgBadge(std::string const& build_name,
                             std::string const& status) {
  // The badge is basically two rectangles with text inside:
  //   [gcb:build-name][message]
  // We need to:
  // - size these rectangles to fit the text,
  // - place the text more or less in the center, and
  // - put a clip-path around the rectangles to "round" the edges
  //
  // The arguments to form the svg badge become:
  //
  // <------------------------ width ------------------------------->
  // <---------- label_width -----><-------- message_width --------->
  // <- label_anchor ->
  // <---------- label_width -----><-------- message_width --------->
  // <------------- message_anchor ----------------->
  // <------------------------ width ------------------------------->
  //
  // Where "label" is the name for the first box, "message" for the second box.
  // The "*_anchor" represent where the text is anchored, approximately at the
  // center of each box.
  // The text is repeated below, with slight offsets to create a shadow
  // effect.
  // The height of the boxes is fixed to 20px.
  auto constexpr kBadgeImageFormat =
      R"""(<svg xmlns='http://www.w3.org/2000/svg' width='{width}' height='20' role="img">
    <linearGradient id='a' x2='0' y2='100%'>
      <stop offset='0' stop-color='#bbb' stop-opacity='.1'/>
      <stop offset='1' stop-opacity='.1'/>
    </linearGradient>
    <clipPath id="r">
      <rect width="{width}" height="20" rx="3" fill="#fff"></rect>
    </clipPath>
    <g clip-path="url(#r)">
      <rect width="{label_width}" height="20" fill="#555"></rect>
      <rect x="{label_width}" width="{message_width}" height="20" fill="{color}"></rect>
      <rect width="{width}" height="20" fill="url(#a)"></rect>
    </g>
    <g fill='#fff' text-anchor='middle' font-family='DejaVu Sans,Verdana,Geneva,sans-serif' font-size='11'>
      <text x='{label_anchor}' y='15' fill='#010101' fill-opacity='.3'>
        {label}
      </text>
      <text x='{label_anchor}' y='14'>
        {label}
      </text>
      <text x='{message_anchor}' y='15' fill='#010101' fill-opacity='.3'>
        {message}
      </text>
      <text x='{message_anchor}' y='14'>
        {message}
      </text>
    </g>
  </svg>)""";
  auto color = [&status]() {
    auto constexpr kSuccessColor = "#4C1";
    auto constexpr kFailureColor = "#E05D44";
    auto constexpr kWorkingColor = "#9F9F9F";
    if (status == "SUCCESS") return kSuccessColor;
    if (status == "FAILURE") return kFailureColor;
    return kWorkingColor;
  }();

  auto label = "gcb:" + build_name;
  auto message = status;
  std::transform(message.begin(), message.end(), message.begin(),
                 [](auto c) { return static_cast<char>(std::tolower(c)); });

  auto textbox_width = [](std::string const& text) {
    auto constexpr kPadding = 10;
    // We estimate the font size at 7px per character, this is probably
    // wrong for a variable width font, but seems to work in practice.
    auto constexpr kAvgCharSize = 7;
    return kPadding + kAvgCharSize * text.size();
  };
  auto const label_width = textbox_width(label);
  auto const label_anchor = label_width / 2;
  auto const message_width = textbox_width(message);
  auto const message_anchor = label_width + message_width / 2;
  auto const width = label_width + message_width;
  return fmt::format(
      kBadgeImageFormat, fmt::arg("label", label),
      fmt::arg("label_width", label_width),
      fmt::arg("label_anchor", label_anchor), fmt::arg("message", message),
      fmt::arg("message_width", message_width),
      fmt::arg("message_anchor", message_anchor), fmt::arg("width", width),
      fmt::arg("status", status), fmt::arg("color", color));
}
