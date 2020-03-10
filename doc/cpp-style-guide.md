# C++ Style Guide for google-cloud-cpp

This project follows the [Google Style Guide][google-style-guide-link]
(hereafter GSG). In a few cases where the GSG gives latitude, we have made a
choice for consistency within this project. The following describes where we
differ from the GSG.

[google-style-guide-link]: https://google.github.io/styleguide/cppguide.html

## Include Guards

All header files should have include guards formatted like
`PROJECT_DIR_FILE_H`. This differs from the advice in the GSG in that ours do
not end with a trailing underscore.

[link to GSG's section on include guards](https://google.github.io/styleguide/cppguide.html#The__define_Guard)

## Reference Arguments

Function return values are preferred over argument out-params. However, when an
out-param is appropriate, prefer a non-const reference.

As of this writing, this rule violates the GSG.

[link to GSG's section on reference arguments](https://google.github.io/styleguide/cppguide.html#Reference_Arguments)

## Preincrement and Predecrement

Always use the prefix forms (`++i`) of increment and decrement unless the
post-increment *semantics* are needed.

[link to GSG's section on prefix increment/decrement](https://google.github.io/styleguide/cppguide.html#Preincrement_and_Predecrement)

## Where to put `const`

Put the const on the right of what it modifies, as in `std::string const&`.
This simplifies the rules for const making it *always* modify what is on its
left. This is sometimes referred to as ["east const"][east-const-link].

[link to GSG's section on using const](https://google.github.io/styleguide/cppguide.html#Use_of_const)

[east-const-link]: https://google.com/search?q=c%2B%2B+"east+const"

## Pointer and Reference Expressions

Place the asterisk or ampersand adjacent to the type when declaring pointers or
references.

```C++
char* c;                 // GOOD
std::string const& str;  // GOOD

char *c;             // BAD
string const &str;   // BAD
string const & str;  // BAD
```

[link to GSG's section on pointer and reference expressions](https://google.github.io/styleguide/cppguide.html#Pointer_and_Reference_Expressions)

## Enumerator Names

Enumerators (for both scoped and unscoped enums) should be named like: `ENUM_NAME`.

[link to GSG's section on enumerator names](https://google.github.io/styleguide/cppguide.html#Enumerator_Names)

## Order of Includes

Order includes from local to global to minimize implicit dependencies between
headers. That is, start with the `.h` file that corresponds to the current
`.cc` file (also do this for the corresonding unit test file), followed by
other `.h` files from the same project, followed by includes from external
projects, followed by C++ standard library headers, followed by C system
headers. For example:

```C++
// Within the file google/cloud/x/foo.cc
#include "google/cloud/x/foo.h"
#include "google/cloud/x/bar.h"
#include "google/cloud/y/baz.h"
#include <grpcpp/blorg.h>
#include <google/bigtable/blah.h>
#include <map>
#include <vector>
#include <unistd.h>
```

This differs substantially from the corresponding section in the GSG, but we
feel the rule presented here is both simpler and better minimizes the implicit
dependencies exposed to each header.

[link to GSG's section on include order](https://google.github.io/styleguide/cppguide.html#Names_and_Order_of_Includes)

## Legal Notice and Author Line

Every file should contain license boilerplate, for new files use:

```C++
// Copyright YYYY Google LLC
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
```

where `YYYY` is the year when the file is being introduced. Do not change existing files boilerplate.

## Doxygen Comments

Use Doxygen-style comments to document classes, functions, etc. Prefer
`@directives` over `\directives`. Do document the template parameters for
template classes. Use `///` for one-line Doxygen comments, use `/** */`
otherwise. Document all classes, functions, and symbols exposed as part of the
API of the library, even obvious ones.

## TODO Comments

Use `TODO` comments for code that is temporary, a short-term solution, or
good-enough but not perfect. All TODO comments should reference a github issue
and a brief description:

```
// TODO(#123) - here we need to randomize the sleep delay….
```
