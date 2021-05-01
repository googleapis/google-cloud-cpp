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

std::string GenerateSvgBadge(std::string const& build_name,
                             std::string const& status) {
  // The badge is basically two rectangles with text inside:
  //   [gcb][build-name]
  // We need to:
  // - size these rectangles to fit the text,
  // - place the text more or less in the center, and
  // - put a clip-path around the rectangles to "round" the edges
  //
  // We will name the first box "system" because it represents what was used for
  // the build, the second box is the "name" box.
  //
  // <------------------------ width ------------------------>
  // <---- system_width -----><--------- name_width --------->
  // <---- na -->
  // <---------- name_anchor ---------------->
  // <---- system_width -----><--------- name_width --------->
  // <------------------------ width ------------------------>
  //
  // The height of the boxes is fixed to 20px.
  auto constexpr kBadgeImageFormat =
      R"""(<svg xmlns='http://www.w3.org/2000/svg'
       width='{width}' height='20'
       role="img"
       aria-label="gcb: {build_name}"
    >
    <linearGradient id='a' x2='0' y2='100%'>
      <stop offset='0' stop-color='#bbb' stop-opacity='.1'/>
      <stop offset='1' stop-opacity='.1'/>
    </linearGradient>
    <clipPath id="r">
      <rect width="{width}" height="20" rx="3" fill="#fff"></rect>
    </clipPath>
    <g clip-path="url(#r)">
      <rect width="{system_width}" height="20" fill="#555"></rect>
      <rect x="{system_width}" width="{name_width}" height="20" fill="{color}"></rect>
      <rect width="{width}" height="20" fill="url(#a)"></rect>
    </g>
    <g fill='#fff' text-anchor='middle' font-family='DejaVu Sans,Verdana,Geneva,sans-serif' font-size='11'>
      <text x='{system_anchor}' y='15' fill='#010101' fill-opacity='.3'>
        gcb
      </text>
      <text x='{system_anchor}' y='14'>
        gcb
      </text>
      <text x='{name_anchor}' y='15' fill='#010101' fill-opacity='.3'>
        {build_name}
      </text>
      <text x='{name_anchor}' y='14'>
        {build_name}
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

  auto textbox_width = [](std::string const& text) {
    auto constexpr kPadding = 10;
    // We estimate the font size at 7px per character, this is probably
    // wrong for a variable width font, but seems to work in practice.
    auto constexpr kAvgCharSize = 7;
    return kPadding + kAvgCharSize * text.size();
  };
  auto const system_width = textbox_width("gcb");
  auto const system_anchor = system_width / 2;
  auto const name_width = textbox_width(build_name);
  auto const name_anchor = system_width + name_width / 2;
  auto const width = system_width + name_width;
  return fmt::format(
      kBadgeImageFormat, fmt::arg("system_width", system_width),
      fmt::arg("system_anchor", system_anchor),
      fmt::arg("name_width", name_width), fmt::arg("name_anchor", name_anchor),
      fmt::arg("width", width), fmt::arg("build_name", build_name),
      fmt::arg("status", status), fmt::arg("color", color));
}
