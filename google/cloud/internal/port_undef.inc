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

// This file manages preprocessor definitions that obstruct portability.
//
// To hide these definitions from the outside world you must follow this pattern
// when #including port_def.inc in a header file:
//
// #include "other_header.h"
// #include "message.h"
// // etc.
//
// #include "port_def.inc"  // MUST be last header included
//
// // Definitions for this header.
//
// #include "port_undef.inc"
//
// This is a textual header with no include guard, because we want to
// detect/prohibit anytime it is #included twice without a corresponding
// #undef.

#ifdef GOOGLE_CLOUD_CPP_PORT_
#error "port_undef.inc must be included after port_def.inc"
#endif
#undef GOOGLE_CLOUD_CPP_PORT_

// Windows declares several inconvenient macro names.  We #undef them and then
// restore them in port_undef.inc.
#if defined(_WIN32) || defined(_MSC_VER)
#pragma pop_macro("CreateService")
#endif
