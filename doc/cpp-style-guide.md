# C++ Style Guide for google-cloud-cpp

This project largely follows (and extensively copies) the [Google Style Guide][google-style-guide-link] (hereafter GSG).
The exceptions are highlighted to help contributors familiar with the GSG start quickly.

[google-style-guide-link]: https://google.github.io/styleguide/cppguide.html

## Goals

##### Style rules should pull their weight

The benefit of a style rule must be large enough to justify asking all of our engineers to remember it. The benefit is
measured relative to the codebase we would get without the rule, so a rule against a very harmful practice may still
have a small benefit if people are unlikely to do it anyway. This principle mostly explains the rules we don’t have,
rather than the rules we do: for example, goto contravenes many of the following principles, but is already vanishingly
rare, so the Style Guide doesn’t discuss it.

##### Optimize for the user first, the reader second, and the writer last

The code in `google-cloud-cpp` will be used by far more developers than those contributing to the project.  They are the
main audience, make the libraries easy to use first.  Then make it easy to understand for a future developer, last make
it easy to write.

##### Be consistent with the broader C++ community as much as possible

Consistency with the way other organizations use C++ has value for the same reasons as consistency within our code base.
Most of the users of `google-cloud-cpp` will not be Googlers, they will be used to the standard library, and to other
popular libraries such as [Boost](https://www.boost.org).  The style guide should conform to their expectations and
reduce surprises for them.

If a feature in the C++ standard solves a problem, or if some idiom is widely known and accepted, that's an argument
for using it.  However, sometimes standard features and idioms are flawed, or were just designed without our codebase's
needs in mind.  In those cases (as described below) it's appropriate to constrain or ban standard features.

`google-cloud-cpp` tries to minimize the number of dependencies it imposes on its users.  When the C++ standard solves a
problem we prefer to adopt that solution first.

##### Avoid surprising or dangerous constructs

C++ has features that are more surprising or dangerous than one might think at a glance. Some style guide restrictions
are in place to prevent falling into these pitfalls. There is a high bar for style guide waivers on such restrictions,
because waiving such rules often directly risks compromising program correctness.

##### Concede to optimization when necessary

Performance optimizations can sometimes be necessary and appropriate, even when they conflict with the other principles
of this document.

##### Adopt rules that can be enforced by automatic tools

When designing this style guide we biased towards rules that can be automatically enforced by linters such as
`clang-tidy` or `clang-format`.

## Major Differences from the Google Style Guide

### Exceptions

This project uses C++ exceptions for error reporting and handling, but the project must compile without exception 
support, following [this approach](no-exception-support.md).

### External Libraries

Only use the following libraries:

* [gRPC](https://grpc.io): the Google Cloud APIs are exposed using this framework, we need to use it.
* [protobuf](https://github.com/google/protobuf): this is a hard dependencies for gRPC.
* [googletest](https://github.com/google/googletest): this is a requirement for gRPC and protobuf, and used only for
    testing.
* The C++11 standard library.

This is a fairly minimal set. Any proposal to add new libraries will require careful consideration. Additional libraries
add complexity to our users. Each additional library increases the chances that there will be an incompatible version
installed in their system or already part of their application.

## Guidelines

These guidelines are applicable to all the C++ code in `google-cloud-cpp`.  When this guideline matches the GSG, we
only describe the decisions in this document, and link to the GSG for rationale and examples.

### Header Files

In general, every `.cc` file should have an associated `.h` file. There are some common exceptions, such as unittests
and small `.cc` files containing just a `main()` function.

Correct use of header files can make a huge difference to the readability, size and performance of your code.

The following rules will guide you through the various pitfalls of using header files.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Self_contained_Headers)

#### Self-contained Headers

Header files should be self-contained (compile on their own) and end in `.h`. Non-header files that are meant for
inclusion should end in `.inc` and be used sparingly.

All header files should be self-contained. Users and refactoring tools should not have to adhere to special
conditions to include the header. Specifically, a header should have header guards and include all other headers it
needs.

Prefer placing the definitions for template and inline functions in the same file as their declarations. The
definitions of these constructs must be included into every `.cc` file that uses them, or the program may fail to link
in some build configurations. If declarations and definitions are in different files, including the former should
transitively include the latter. Do not move these definitions to separately included header files (`-inl.h`); this
practice was common in the past, but is no longer allowed.

As an exception, a template that is explicitly instantiated for all relevant sets of template arguments, or that is a
private implementation detail of a class, is allowed to be defined in the one and only `.cc` file that instantiates
the template.

There are rare cases where a file designed to be included is not self-contained. These are typically intended to be
included at unusual locations, such as the middle of another file. They might not use header guards, and might not
include their prerequisites. Name such files with the `.inc` extension. Use sparingly, and prefer self-contained
headers when possible.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Self_contained_Headers)

#### The `#define` Guard

All header files should have #define guards to prevent multiple inclusion. The format of the symbol name should be
`<PROJECT>_<PATH>_<FILE>_H_`.

To guarantee uniqueness, they should be based on the full path in a project's source tree. For example, the file
`foo/bar/baz.h` in project foo should have the following guard:

```C++
#ifndef GOOGLE_CLOUD_CPP_FOO_BAR_BAZ_H_
#define GOOGLE_CLOUD_CPP_FOO_BAR_BAZ_H_

#endif  // GOOGLE_CLOUD_CPP_FOO_BAR_BAZ_H_
```

[link to GSG](https://google.github.io/styleguide/cppguide.html#The__define_Guard)

#### Forward Declarations

Avoid using forward declarations where possible. Just `#include` the headers you need.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Forward_Declarations)

#### Inline Functions

Define functions inline only when they are small, say, 10 lines or fewer.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Inline_Functions)

#### Names and Order of Includes

Use the following order for readability and to avoid hidden dependencies: related header, the project's `.h` files,
grpc headers, protobuf headers, C++ library, C library.

The project headers should always be included using their full path from the project root.

```C++
#include "bigtable/client/foo.h"
```

Files included from other projects, including submodules are included using angle brackets. Files from within the
`google-cloud-cpp` project are included with quotes. Separate headers from different libraries using a blank line.  For
example, a `table.cc` file would have:

```C++
// Copyright notice elided ...
#include "bigtable/client/table.h"
#include "bigtable/client/foo.h"
#include <grpc/grpc/++.h>
#include <google/bigtable/v2/data.grpc.pb.h>
#include <map>
#include <vector>
#include <unitstd.h> // probably protected by #ifdef for Windows.
```

This is substantially different from the corresponding
[GSG section](https://google.github.io/styleguide/cppguide.html#Names_and_Order_of_Includes)

### Scoping

#### Namespaces

All the code should be in a namespace. Namespaces should have unique names based on the subproject name, based on its
path (e.g. `bigtable`, or `spanner`).  Do not use using-directives in headers (e.g. `using namespace foo`).
For unnamed namespaces, see [Unnamed Namespaces and Static Variables](#unnamed-namespaces-and-static-variables).

This project uses inline namespaces for versioning.  All types, functions and variables are in an inlined versioned
namespace,
such as `bigtable::v1`.  Project version numbers follow the semantic versioning guidelines, like all projects in
`google-cloud-*`.  The namespace is named after the major version.  Each subproject (bigtable, spanner, etc) will use
its own versions.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Namespaces)

#### Unnamed Namespaces and Static Variables

When definitions in a `.cc` file do not need to be referenced outside that file, place them in an unnamed namespace
or declare them `static`. Do not use either of these constructs in `.h` files.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Unnamed_Namespaces_and_Static_Variables)

#### Nonmember, Static Member, and Global Functions

Prefer placing nonmember functions in a namespace; use completely global functions rarely. Prefer grouping functions
with a namespace instead of using a class as if it were a namespace. Static methods of a class should generally be
closely related to instances of the class or the class's static data.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Nonmember,_Static_Member,_and_Global_Functions)

#### Local Variables

Place a function's variables in the narrowest scope possible, and initialize variables in the declaration.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Local_Variables)

#### Static and Global Variables

Variables of class type with static storage duration are forbidden: they cause hard-to-find bugs due to indeterminate
order of construction and destruction. However, such variables are allowed if they are `constexpr`: they have no
dynamic initialization or destruction.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Static_and_Global_Variables)

#### Classes

Classes are the fundamental unit of code in C++. Naturally, we use them extensively. This section lists the main dos
and don'ts you should follow when writing a class.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Classes)

#### Doing Work in Constructors

Avoid virtual method calls in constructors, and avoid initialization that can fail if you can't signal an error.

While we prefer [RAII](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) over `Init()`
member functions the guideline about avoiding initialization that can fail is particularly relevant if your class is
using RAII to acquire and release remote resources.  While signaling error to acquire the resources is easy with
exceptions, signaling an error to release the resources requires you to raise exceptions in the destructor, which is
not desirable.

#### Raising Exceptions in Destructors

Do not raise exceptions in destructors.  Declare your destructors as `noexcept`.

#### Implicit Conversions

Do not define implicit conversions. Use the `explicit` keyword for conversion operators and single-argument
constructors.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Implicit_Conversions)

#### Copyable and Movable Types

Support copying and/or moving if these operations are clear and meaningful for your type. Otherwise, disable the
implicitly generated special functions that perform copies and moves.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Copyable_Movable_Types)

#### Structs vs. Classes

Use a `struct` only for passive objects that carry data; everything else is a `class`.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Structs_vs._Classes)

#### Inheritance

Composition is often more appropriate than inheritance. When using inheritance, make it `public`.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Inheritance)

It is common practice in the C++ community to use `private` inheritance for composition, in this case, we prefer to
follow the GSG because it is fairly rare to use private inheritance in any case.

#### Multiple inheritance

Only very rarely is multiple implementation inheritance actually useful. We allow multiple inheritance when:

* at  most one of the base classes has an implementation; all other base classes must be pure interface classes, or
* in support of [Policy-based design](https://en.wikipedia.org/wiki/Policy-based_design).

[link to GSG](https://google.github.io/styleguide/cppguide.html#Multiple_Inheritance)

#### Interfaces

Pure interface classes may have the `Interface` suffix, but we recommend against it.  In general, pure interface
classes *should* be the main class exposed to users of the library.  It is better to make it easy for them to use the
class without having to say (and type) `Interface` each time.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Interfaces)

#### Operator Overloading

Overload operators judiciously. Avoid user-defined literals in the interface of the project; use them as needed in
the implementation and tests.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Operator_Overloading)

#### Access Control

Make data members `private`, unless they are `static const` (and follow the naming convention for constants). For
technical reasons, we allow data members of a test fixture class to be `protected` when using Google Test).

[link to GSG](https://google.github.io/styleguide/cppguide.html#Access_Control)

#### Declaration Order

Group similar declarations together, placing public parts earlier.

A class definition should usually start with a `public:` section, followed by `protected:`, then `private:`. Omit
sections that would be empty.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Declaration_Order)

#### Functions

#### Parameter Order

Prefer return values over output parameters.  Use `std::tuple` if many values are necessary.  If you must use an
output parameter, the parameter order is: inputs, then outputs.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Function_Parameter_Ordering)

#### Write Short Functions

Prefer small and focused functions.

We recognize that long functions are sometimes appropriate, so no hard limit is placed on functions length. If a
function exceeds about 40 lines, think about whether it can be broken up without harming the structure of the program.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Write_Short_Functions)

#### Reference Arguments

Pass parameters by reference when appropriate. References are preferred over pointers, smart pointers over member
variables of reference type.

This is substantially different from the corresponding
[GSG section](https://google.github.io/styleguide/cppguide.html#Reference_Arguments)

#### Function Overloading

Use overloaded functions (including constructors) only if a reader looking at a call site can get a good idea of what is happening without having to first figure out exactly which overload is being called.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Function_Overloading)

#### Default Arguments

Default arguments are allowed on non-virtual functions when the default is guaranteed to always have the same value.
Follow the same restrictions as for [function overloading](#function-overloading), and prefer overloaded functions if
the readability gained with default arguments doesn't outweigh the downsides below.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Default_Arguments)

#### Trailing Return Type Syntax

Use trailing return types only where using the ordinary syntax (leading return types) is impractical or much less
readable.

[link to GSG](https://google.github.io/styleguide/cppguide.html#trailing_return)

### General

#### Ownership and Smart Pointers

Prefer to have single, fixed owners for dynamically allocated objects. Prefer to transfer ownership with smart pointers.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Ownership_and_Smart_Pointers)


#### clang-tidy and clang-format

Automate formatting using `clang-format`, the project includes a properly configured `.clang-format` file. Automate
naming and readability rules using `clang-tidy`. The project includes a properly configured `.clang-tidy` file.
Make sure your CI builds include builds that enforce the guidelines embedded in these rules.

#### Rvalue references

Use Rvalue references to define move constructors and move assignment operators.  Use Rvalue references to represent
ownership transfer.

This is substantially different from the corresponding
[GSG section](https://google.github.io/styleguide/cppguide.html#Rvalue_references)

#### Friends

We allow use of `friend` classes and functions, within reason.

Friends should usually be defined in the same file so that the reader does not have to look in another file to find
uses of the private members of a class. A common use of `friend` is to have a `FooBuilder` class be a friend of `Foo` so
that it can construct the inner state of `Foo` correctly, without exposing this state to the world. In some cases it
may be useful to make a unittest class a friend of the class it tests.

Friends extend, but do not break, the encapsulation boundary of a class. In some cases this is better than making a
member public when you want to give only one other class access to it. However, most classes should interact with
other classes solely through their public members.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Friends)

#### Exceptions

Use exceptions to represent a failure to complete the desired work in a function. Your library should compile with 
exceptions disabled, follow [the accompanying design document](no-exception-support.md) to do so. In rare circumstances
you may need to provide APIs that perform error reporting without using exceptions, the
[same document](no-exception-support.md) covers how to design such APIs.

There are no hard and fast rules as to when is better to raise an exceptions vs. returning an error status, specially
for libraries that contact remote systems. In general, this guideline is useful:

```
Raise an exception when the function could not complete its work.
```

For purely local functions this is a broadly applicable guideline. If the preconditions are not met, or the arguments
are out of range or invalid one should raise an exception. It is a harder rule to apply for operations that need to 
contact a remote server. The following list is not exhaustive, but provides some general principles:

- Failing to contact the server should raise an exception.
- Receiving an invalid response from the server should raise an exception.
- Receiving a permanent error status from the server should raise an exception.
- Being unable to find an object, or row, or file in a remote server should
  not raise an exception.

As we said, there are no hard and fast rules, consult your colleagues and be
ready to change your mind.

This is substantially different from the corresponding
[GSG section](https://google.github.io/styleguide/cppguide.html#Exceptions)

#### RTTI

Avoid using Run Time Type Information (RTTI).

[link to GSG](https://google.github.io/styleguide/cppguide.html#Run-Time_Type_Information__RTTI_)

#### Casting

Use C++-style casts like `static_cast<float>(double_value)`, or brace initialization for conversion of arithmetic types
like `int64 y = int64{1} << 42`. Do not use cast formats like `int y = (int)x` or `int y = int(x)` (but the latter
is okay when invoking a constructor of a class type).

[link to GSG](https://google.github.io/styleguide/cppguide.html#Casting)

#### Streams

Use streams where appropriate, and stick to "simple" usages.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Streams)

#### Preincrement and Predecrement

Use prefix form (++i) of the increment and decrement operators with iterators and other template objects.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Preincrement_and_Predecrement)

#### Use of const

Use `const` whenever it makes sense. With C++11, `constexpr` is a better choice for some uses of `const`.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Use_of_const)

**Where to put the const**:  we have been putting the const to the right as in `std::string const&`.  It is easy to read
C++ types like this from right to left ("a reference to a constant string").

#### Use of constexpr

Use `constexpr` to define true constants or to ensure constant initialization.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Use_of_constexpr)

#### Integer Types

Use `<cstdint>` over `<stdint.h>`, qualify the integer types as `std::int16_t`.   The libraries in `google-cloud-cpp`
will be used by external users, who may have more restrictive rules about polluting the global namespace

This is substantially different from the corresponding
[GSG section](https://google.github.io/styleguide/cppguide.html#Integer_Types)

#### 64-bit Portability

Code should be 64-bit and 32-bit friendly. Bear in mind problems of printing, comparisons, and structure alignment.

[link to GSG](https://google.github.io/styleguide/cppguide.html#64-bit_Portability)

#### Preprocessor Macros

Avoid defining macros, especially in headers; prefer inline functions, enums, and const variables. Name macros with a
project-specific prefix. Do not use macros to define pieces of a C++ API.

Macros mean that the code you see is not the same as the code the compiler sees. This can introduce unexpected
behavior, especially since macros have global scope.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Preprocessor_Macros)

#### 0 and nullptr/NULL

Use `0` for integers, `0.0` for reals, `nullptr` for pointers, and `'\0'` for chars.

[link to GSG](https://google.github.io/styleguide/cppguide.html#0_and_nullptr/NULL)

#### sizeof

Prefer sizeof(varname) to sizeof(type).

[link to GSG](https://google.github.io/styleguide/cppguide.html#sizeof)

#### auto

Use auto to avoid type names that are noisy, obvious, or unimportant - cases where the type doesn't aid in clarity
for the reader. Continue to use manifest type declarations when it helps readability.

[link to GSG](https://google.github.io/styleguide/cppguide.html#auto)

#### Braced Initializer List

You may use braced initializer lists.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Braced_Initializer_List)

#### Lambda Expressions

Use lambda expressions where appropriate. Prefer explicit captures when the lambda will escape the current scope.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Lambda_expressions)

#### Template Metaprogramming

Avoid complicated template programming.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Template_metaprogramming)

#### Boost

We do not use Boost in `google-cloud-cpp`.  We do not want to introduce too many dependencies.  While it is
likely that our users are also using Boost, managing Boost versions creates problems for them, we should be a
source of solutions.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Boost)

#### std::hash

We do not specialize `std::hash` in `google-cloud-cpp`

[link to GSG](https://google.github.io/styleguide/cppguide.html#std_hash)

#### C++11

`google-cloud-cpp` is a C++11 project.  Use C++11 features where appropriate, do not use features for their own sake.
That applies to C++98 and C++03 features too.

* We use `std::chrono` in `google-cloud-cpp`, and indirectly `<ratio>`.  Avoid using `<ratio>` directly.
* We do not use `<cfenv>` or `<fenv.h>`, do not use without consultation.
* Do not use ref-qualifiers (i.e. `X::Foo()&` or `X::Foo()&&`).

[link to GSG](https://google.github.io/styleguide/cppguide.html#C++11)

#### Nonstandard Extensions

Nonstandard extensions to C++ may not be used unless otherwise specified.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Nonstandard_Extensions)

#### Aliases

Public aliases are for the benefit of an API's user, and should be clearly documented. Prefer the `using Foo = Bar;`
form over `typedef Bar Foo;` because it can be used more consistently.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Aliases)

### Naming

Naming rules are pretty arbitrary, but we feel that consistency is more important than individual preferences in this
area, so regardless of whether you find them sensible or not, the rules are the rules.

All the subsections here follow the guidelines in the corresponding
[GSG sections](https://google.github.io/styleguide/cppguide.html#Naming)

#### General Naming Rules

Names should be descriptive; avoid abbreviation.

Give as descriptive a name as possible, within reason. Do not worry about saving horizontal space as it is far more
important to make your code immediately understandable by a new reader. Do not use abbreviations that are ambiguous
or unfamiliar to readers outside your project, and do not abbreviate by deleting letters within a word.

[link to GSG](https://google.github.io/styleguide/cppguide.html#General_Naming_Rules)

#### File Names

Filenames should be all lowercase and can include underscores (`_`). Do not use dashes (`-`) in filenames.

[link to GSG](https://google.github.io/styleguide/cppguide.html#File_Names)

#### Type Names

Type names start with a capital letter and have a capital letter for each new word, with no underscores.  This is 
also known as `CamelCase`.

[link to CSG](https://google.github.io/styleguide/cppguide.html#Type_Names)

#### Variable Names

Variable names should be all lowercase with `_` to separate words.  This is also known as `snake_case`.

[link to CSG](https://google.github.io/styleguide/cppguide.html#Variable_Names)

#### Class Data Members

Class data member names should be all lowercase with `_` to separate words, they should end with a trailing `_`.
This is also known as `snake_case_`.

#### Struct Data Members

Struct data member names should be all lowercase with `_` to separate words, they should **not** have a trailing `_`.
This is also known as `snake_case`.

#### Constant Names

Variables declared constexpr or const, and whose value is fixed for the duration of the program, are named with a leading "k" followed by mixed case

[link to CSG](https://google.github.io/styleguide/cppguide.html#Constant_Names)

#### Function Names

Regular functions have mixed case; accessors and mutators may be named like variables.

[link to CSG](https://google.github.io/styleguide/cppguide.html#Function_Names)

#### Namespace Names

Namespace names are all lower-case. Top-level namespace names are based on the project name . Avoid collisions between nested namespaces and well-known top-level namespaces.

[link to CSG](https://google.github.io/styleguide/cppguide.html#Namespace_Names)

#### Enumerator Names

Enumerators (for both scoped and unscoped enums) should be named like [macros](#macro-names): `ENUM_NAME`.

This rule is more restrictive than:
[link to CSG](https://google.github.io/styleguide/cppguide.html#Enumerator_Names)

#### Macro Names

You're not really going to define a macro, are you? If you do, they're like this: `MY_MACRO_THAT_SCARES_SMALL_CHILDREN`.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Macro_Names)

#### Comments

Though a pain to write, comments are absolutely vital to keeping our code readable. The following rules describe what
you should comment and where. But remember: while comments are very important, the best code is self-documenting.
Giving sensible names to types and variables is much better than using obscure names that you must then explain
through comments.

When writing your comments, write for your audience: the next contributor who will need to understand your code. Be
generous — the next one may be you!

[link to GSG](https://google.github.io/styleguide/cppguide.html#Comments)

#### Comment Style

Use either the `//` or `/* */` syntax, as long as you are consistent.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Comment_Style)

#### Legal Notice and Author Line

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

#### File Contents

If a `.h` declares multiple abstractions, the file-level comment should broadly describe the contents of the file, and 
how the abstractions are related. A 1 or 2 sentence file-level comment may be sufficient. The detailed documentation 
about individual abstractions belongs with those abstractions, not at the file level. 

Do not duplicate comments in both the `.h` and the `.cc`. Duplicated comments diverge.

#### Class Comments

Use Doxygen-style comments to document classes.  Prefer `@directives` over `\directives`.  Do document the template
parameters for template classes.  Use `///` for one-line Doxygen comments, use `/** */` otherwise.
Document all classes exposed as part of the API of the library, even obvious ones.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Class_Comments)

#### Function Comments

Like for class comments, use Doxygen-style comments to document functions.
Declaration comments describe use of the function (when it is non-obvious); comments at the definition of a function
describe operation.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Function_Comments)

#### Function Definitions

If there is anything tricky about how a function does its job, the function definition should have an explanatory
comment. For example, in the definition comment you might describe any coding tricks you use, give an overview of the
steps you go through, or explain why you chose to implement the function in the way you did rather than using a
viable alternative. For instance, you might mention why it must acquire a lock for the first half of the function
but why it is not needed for the second half.

Note you should not just repeat the comments given with the function declaration, in the .h file or wherever. It's
okay to recapitulate briefly what the function does, but the focus of the comments should be on how it does it.

#### Variable Comments

In general the actual name of the variable should be descriptive enough to give a good idea of what the variable is
used for. In certain cases, more comments are required.
If you need to document the variable, remember to use Doxygen style comments for it.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Variable_Comments)

#### Implementation Comments

[link to GSG](https://google.github.io/styleguide/cppguide.html#Implementation_Comments)

#### Punctuation, Spelling and Grammar

Pay attention to punctuation, spelling, and grammar; it is easier to read well-written comments than badly written ones.

Comments should be as readable as narrative text, with proper capitalization and punctuation. In many cases, complete
sentences are more readable than sentence fragments. Shorter comments, such as comments at the end of a line of
code, can sometimes be less formal, but you should be consistent with your style.

Although it can be frustrating to have a code reviewer point out that you are using a comma when you should be using
a semicolon, it is very important that source code maintain a high level of clarity and readability. Proper
punctuation, spelling, and grammar help with that goal.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Punctuation,_Spelling_and_Grammar)

#### TODO Comments

Use `TODO` comments for code that is temporary, a short-term solution, or good-enough but not perfect. All TODO
comments should reference a github issue and a brief description:

```
// TODO(#123) - here we need to randomize the sleep delay….
```

#### Deprecation Comments

Mark deprecated interface points with `DEPRECATED` comments.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Deprecation_Comments)

#### Formatting

Coding style and formatting are pretty arbitrary, but a project is much easier to follow if everyone uses the same
style. Individuals may not agree with every aspect of the formatting rules, and some of the rules may take some
getting used to, but it is important that all project contributors follow the style rules so that they can all read
and understand everyone's code easily.

The project enforces formatting using `clang-format`, configured to follow the `Google` style guide.  Please
configure your editor or IDE to follow the same guidelines.  When the tool disagrees with this document please file a
bug either the tool configuration or the document needs adjustment. This document lists all the formatting rules to
compare them against the existing style guide, unless you are trying to learn how to configure `clang-format`, you
probably should not memorize the rules and trust the tool to do the formatting for you.

In a few cases where the GSG give latitude we have made a choice.

***Line length:***

Each line of text in your code should be at most 120 characters long.

#### Non-ASCII Characters

Non-ASCII characters should be rare, and must use UTF-8 formatting.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Non-ASCII_Characters)

#### Spaces vs. Tabs

Use only spaces, and indent 2 spaces at a time.

We use spaces for indentation. Do not use tabs in your code. You should set your editor to emit spaces when you hit
the tab key.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Spaces_vs._Tabs)

#### Function Declarations and Definitions

Return type on the same line as function name, parameters on the same line if they fit. Wrap parameter lists which do not fit on a single line as you would wrap arguments in a function call.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Function_Declarations_and_Definitions)

#### Lambda Expressions

Format parameters and bodies as for any other function, and capture lists like other comma-separated lists.

For by-reference captures, do not leave a space between the ampersand (&) and the variable name.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Formatting_Lambda_Expressions)

#### Function Calls

Either write the call all on a single line, wrap the arguments at the parenthesis, or start the arguments on a new line indented by four spaces and continue at that 4 space indent. In the absence of other considerations, use the minimum number of lines, including placing multiple arguments on each line where appropriate.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Function_Calls)

#### Braced Initializer List Format

Format a braced initializer list exactly like you would format a function call in its place.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Braced_Initializer_List_Format)

#### Conditionals

Prefer no spaces inside parentheses. The if and else keywords belong on separate lines.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Conditionals)

#### Loops and Switch Statements

Switch statements may use braces for blocks. Annotate non-trivial fall-through between cases. Braces are optional for single-statement loops. Empty loop bodies should use empty braces or continue.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Loops_and_Switch_Statements)

#### Pointer and Reference Expressions

No spaces around period or arrow. Pointer operators do not have trailing spaces. When declaring a pointer variable or
argument, place the asterisk adjacent to the type:

```C++
// These are fine, space following.
char* c;
string const& str;

// These are not fine, space preceding.
char *c;
string const &str;
```

[link to GSG](https://google.github.io/styleguide/cppguide.html#Pointer_and_Reference_Expressions)

#### Boolean Expressions

When you have a boolean expression that is longer than the standard line length, be consistent in how you break up
the lines.  Also note that you should always use the punctuation operators, such as `&&` and `~`, rather than the word
operators, such as `and` and `compl`.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Boolean_Expressions)

#### Return Values

Do not needlessly surround the return expression with parentheses.

Use parentheses in `return expr;` only where you would use them in `x = expr;`.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Return_Values)

#### Variable and Array Initialization

Your choice of `=`, `()`, or `{}`.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Variable_and_Array_Initialization)

#### Preprocessor Directives

The hash mark that starts a preprocessor directive should always be at the beginning of the line.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Preprocessor_Directives)

#### Class Format

Sections in public, protected and private order, each indented one space.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Class_Format)

#### Constructor Initializer Lists

Constructor initializer lists can be all on one line or with subsequent lines indented four spaces.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Constructor_Initializer_Lists)

#### Namespace Formatting

The contents of namespaces are not indented.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Namespace_Formatting)

#### Horizontal Whitespace

Use of horizontal whitespace depends on location. Never put trailing whitespace at the end of a line.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Horizontal_Whitespace)

#### Vertical Whitespace

Minimize use of vertical whitespace.

This is more a principle than a rule: don't use blank lines when you don't have to. In particular, don't put more
than one or two blank lines between functions, resist starting functions with a blank line, don't end functions with
a blank line, and be discriminating with your use of blank lines inside functions.

The basic principle is: The more code that fits on one screen, the easier it is to follow and understand the control
flow of the program. Of course, readability can suffer from code being too dense as well as too spread out, so use
your judgement. But in general, minimize use of vertical whitespace.

Some rules of thumb to help when blank lines may be useful:

* Blank lines at the beginning or end of a function very rarely help readability.
* Blank lines inside a chain of if-else blocks may well help readability.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Vertical_Whitespace)

#### Exceptions to the rules

None at this time. If you need to get an exception remember that you must also change the tooling that enforces these
rules to enforce your exception or at least ignore the section of code where you are not following these guidelines.

[link to GSG](https://google.github.io/styleguide/cppguide.html#Exceptions_to_the_Rules)

#### Parting Words

Use common sense and **BE CONSISTENT**.

If you are editing code, take a few minutes to look at the code around you and determine its style. If they use spaces
around their if clauses, you should, too. If their comments have little boxes of stars around them, make your comments
have little boxes of stars around them too.

The point of having style guidelines is to have a common vocabulary of coding so people can concentrate on what you are
saying, rather than on how you are saying it. We present global style rules here so people know the vocabulary. But
local style is also important. If code you add to a file looks drastically different from the existing code around it,
the discontinuity throws readers out of their rhythm when they go to read it. Try to avoid this.

OK, enough writing about writing code; the code itself is much more interesting. Have fun!
