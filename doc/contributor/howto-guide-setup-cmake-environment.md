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

## Install the dependencies in `$HOME/local-cpp`.

In this guide we will install the dependencies in `$HOME/local-cpp`, you can
pick any directory where you have write permissions. We will then compile the
project against these dependencies. The installation needs to be done every time
the version of the dependencies is changed. Once installed you can use them for
any build or from any other clone.

Configure the super-build to install in `$HOME/local-cpp`. We recommend that
you use Ninja for this build because it is substantially faster than Make for
these use-cases. Note that if Ninja is not installed in your workstation you may
need to remove the `-GNinja` flag:

```shell
cmake -Hsuper -Bcmake-out/si \
    -DGOOGLE_CLOUD_CPP_EXTERNAL_PREFIX=$HOME/local-cpp -GNinja
```

Install the dependencies:

> **Note:** Ninja will parallelize the build to use all the cores in your
> workstation, if you are not using Ninja, or want to limit the number of cores
> used by the build use the `-j` option.

```shell
cmake --build cmake-out/si --target project-dependencies
```

## Building with the installed dependencies

Now you can use these dependencies from different builds. Configure your CMake
builds with `-DCMAKE_PREFIX_PATH=$HOME/local-cpp` to build `google-cloud-cpp`:

```shell
cd $HOME/google-cloud-cpp
cmake -H. -Bcmake-out/home -DCMAKE_PREFIX_PATH=$HOME/local-cpp
cmake --build cmake-out/home
```

You can use these dependencies from other locations too:

```shell
cd $HOME/another-google-cloud-cpp-clone
cmake -H. -Bcmake-out/home -DCMAKE_PREFIX_PATH=$HOME/local-cpp
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
    cmake -H. -Bcmake-out/clang -DCMAKE_PREFIX_PATH=$HOME/local-cpp

# Then compile and test normally:
cmake --build cmake-out/clang
(cd cmake-out/clang && ctest --output-on-failure -LE integration-test)
```

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
