// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_DOCFX_FUNCTION_CLASSIFIERS_H
#define GOOGLE_CLOUD_CPP_DOCFX_FUNCTION_CLASSIFIERS_H

#include <pugixml.hpp>

namespace docfx {

// Determine if a function is a constructor.
bool IsConstructor(pugi::xml_node node);

// Determine if a function is an operator.
bool IsOperator(pugi::xml_node node);

// Determine if a doxygen element describes a function.
bool IsFunction(pugi::xml_node node);

// Determine if a doxygen element is a function, but not a constructor or
// operator.
bool IsPlainFunction(pugi::xml_node node);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_FUNCTION_CLASSIFIERS_H
