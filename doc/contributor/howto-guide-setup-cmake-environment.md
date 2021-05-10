# How-to Guide: Setup for CMake-based builds

This document describes how to setup your workstation to build the Google Cloud
C++ client libraries using CMake. The intended audience is developers of the
client libraries that want to verify their changes will work with CMake and/or
prefer to use CMake for whatever reason.

* Packaging maintainers or developers that prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
* Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](/README.md#quickstart) for the library
  or libraries they want to use.
* Developers wanting to compile the library just to run some of the examples or
  tests should read the [build and install](/README.md#building-and-installing)
  section from the top-level README.
* Contributors and developers to `google-cloud-cpp` wanting to work with CMake
  should read the current document.

The document assumes you are using a Linux workstation running Ubuntu, changing
the instructions for other distributions or operating systems is left as an
exercise to the reader. PRs to improve this document are most welcome!

Unless explicitly stated, this document assumes you running these commands at
the top-level directory of the project, as in:

```shell
cd $HOME
git clone git@github:<github-username>/google-cloud-cpp.git
cd google-cloud-cpp
```

## Download and bootstrap `vcpkg`

[vcpkg] is a package manager for C++ that builds from source and installs any
binary artifacts in `$HOME`. We recommend using `vcpkg` for local development.
In these instructions, we will install `vcpkg` descriptions in `$HOME/vcpkg`,
you can change the `vcpkg` location, just remember to adapt these instructions
as you go along. Download the `vcpkg` package descriptions using `git`:

```shell
git -C $HOME clone https://github.com/microsoft/vcpkg
```

Then bootstrap the `vcpkg` tool:

```shell
$HOME/vcpkg/bootstrap-vcpkg.sh
```

## Building `google-cloud-cpp` with vcpkg

Now you can use `vcpkg` to compile `google-cloud-cpp`. Just add one option
to the `cmake` configure step:

```shell
cd $HOME/google-cloud-cpp
cmake -H. -Bcmake-out/home -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-out/home
```

The first time you run this command it can take a significant time to download
and compile all the dependencies (Abseil, gRPC, Protobuf, etc). Note that vcpkg
caches binary artifacts (in `$HOME/.cache/vcpkg`) so a second build would be
much faster:

```shell
cd $HOME/another-google-cloud-cpp-clone
cmake -H. -Bcmake-out/home -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-out/home
```

## Running the unit tests

Once you have built `google-cloud-cpp` you can run the unit tests using:

```shell
env -C cmake-out/home ctest --output-on-failure -LE integration-test
```

If you also want to run the integration tests you need to setup multiple
[environment variables](/ci/etc/integration-tests-config.sh), and then run:

```shell
env -C cmake-out/home ctest --output-on-failure
```

## Other Build Options

You can configure and build with different compilers, build options, etc. all
sharing the same pre-built dependencies.

### Changing the compiler

If your workstation has multiple compilers (or multiple versions of a compiler)
installed, you can change the compiler using:

```shell
CXX=clang++ CC=clang \
    cmake -H. -Bcmake-out/home -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake

# Then compile and test normally:
cmake --build cmake-out/clang
(cd cmake-out/clang && ctest --output-on-failure -LE integration-test)
```

`vcpkg` uses the compiler as part of its binary cache inputs, that is, changing
the compiler will require rebuilding the dependencies from source. The good
news is that `vcpkg` can hold multiple versions of a binary artifact in its
cache.

### Changing the build type

By default, the system is compiled with optimizations on; if you want to compile
a debug version, use:

```shell
cmake -H. -Bcmake-out/manual -DCMAKE_BUILD_TYPE=Debug

# Then compile and test normally:
cmake --build cmake-out/manual
(cd cmake-out/manual && ctest --output-on-failure -LE integration-test)
```

`google-cloud-cpp` supports the standard CMake
[build types](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html).
