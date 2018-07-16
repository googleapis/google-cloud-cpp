# Google Cloud Platform C++ Client Libraries

C++ Idiomatic Clients for [Google Cloud Platform][cloud-platform] services.

[![Travis CI status][travis-shield]][travis-link]
[![AppVeyor CI status][appveyor-shield]][appveyor-link]
[![Codecov Coverage status][codecov-shield]][codecov-link]

- [Google Cloud Platform Documentation][cloud-platform-docs]
- [Client Library Documentation][client-library-docs]

[travis-shield]: https://travis-ci.org/GoogleCloudPlatform/google-cloud-cpp.svg?branch=master
[travis-link]: https://travis-ci.org/GoogleCloudPlatform/google-cloud-cpp/builds
[appveyor-shield]: https://ci.appveyor.com/api/projects/status/d6srbtprnie4ufrx/branch/master?svg=true
[appveyor-link]: https://ci.appveyor.com/project/coryan/google-cloud-cpp/branch/master
[codecov-shield]: https://codecov.io/gh/GoogleCloudPlatform/google-cloud-cpp/branch/master/graph/badge.svg
[codecov-link]: https://codecov.io/gh/GoogleCloudPlatform/google-cloud-cpp
[cloud-platform]: https://cloud.google.com/
[cloud-platform-docs]: https://cloud.google.com/docs/
[client-library-docs]: http://GoogleCloudPlatform.github.io/google-cloud-cpp/

This library supports the following Google Cloud Platform services with clients
at an [Alpha](#versioning) quality level:

- [Google Cloud Bigtable](google/cloud/bigtable#google-cloud-bigtable)

The libraries in this code base likely do not (yet) cover all the available
APIs.

## Table of Contents

- [Requirements](#requirements)
  - [Compiler](#compiler)
  - [Build Tools](#build-tools)
  - [Libraries](#libraries)
  - [Tests](#tests)
- [Install Dependencies](#install-dependencies)
  - [CentOS](#centos)
  - [Fedora](#fedora)
  - [Ubuntu (Bionic Beaver)](#ubuntu-bionic-beaver)
  - [Ubuntu (Trusty)](#ubuntu-trusty)
  - [macOS (using brew)](#macos-using-brew)
  - [Windows](#windows)
- [Build](#build)
  - [Linux and macOS](#linux-and-macos)
  - [Windows](#windows-1)

## Requirements

#### Compiler

The Google Cloud C++ libraries are tested with the following compilers:

| Compiler    | Minimum Version |
| ----------- | --------------- |
| GCC         | 4.9 |
| Clang       | 3.8 |
| MSVC++      | 14.1 |
| Apple Clang | 8.1 |

#### Build Tools

The Google Cloud C++ Client Libraries can be built with
[CMake](https://cmake.org) or [Bazel](https://bazel.io).  The minimal versions
of these tools we test with are:

| Tool       | Minimum Version |
| ---------- | --------------- |
| CMake      | 3.5 |
| Bazel      | 0.12.0 |

#### Libraries

The libraries also depends on gRPC, libcurl, and the dependencies of those
libraries. The Google Cloud C++ Client libraries are tested with the following
versions of these dependencies:

| Library | Minimum version |
| ------- | --------------- |
| gRPC    | v1.10.x |
| libcurl | 7.47.0  |

#### Tests

Integration tests at times use the
[Google Cloud SDK](https://cloud.google.com/sdk/). The integration tests run
against the latest version of the SDK on each commit and PR.

## Install Dependencies

#### CentOS

```bash
# Extra Packages for Enterprise Linux used to install cmake3
rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm

yum install centos-release-scl
yum-config-manager --enable rhel-server-rhscl-7-rpms

yum makecache
yum install -y devtoolset-7 c-ares-devel ccache cmake3 curl curl-devel git golang graphviz openssl-devel pkgconfig python python-pip python-gunicorn shtool unzip wget which zlib-devel

pip install httpbin

# Install cmake3 & ctest3 as cmake & ctest respectively.
ln -sf /usr/bin/cmake3 /usr/bin/cmake && ln -sf /usr/bin/ctest3 /usr/bin/ctest
```

#### Fedora

```bash
sudo dnf makecache
sudo dnf install autoconf automake c-ares-devel ccache clang clang-tools-extra cmake curl dia doxygen gcc-c++ git golang graphviz  lcov libcurl-devel libtool make ncurses-term openssl-devel pkgconfig python python-gunicorn python-httpbin shtool unzip wget  which zlib-devel
```

#### Ubuntu (Bionic Beaver)

```bash
sudo apt update
sudo apt install abi-compliance-checker abi-dumper automake build-essential ccache clang clang-format cmake curl doxygen  gawk git gcc g++ golang cmake libcurl4-openssl-dev libssl-dev libtool lsb-release make python-gunicorn python-httpbin tar wget zlib1g-dev

# By default, Ubuntu 18.04 does not install the alternatives for clang-format
# so we need to manually install those.
sudo apt update
sudo apt install -y clang-tidy clang-format clang-tools       
sudo update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-6.0 100
sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-6.0 100
sudo update-alternatives --install /usr/bin/scan-build scan-build /usr/bin/scan-build-6.0 100
/usr/bin/env GOPATH=/usr go get github.com/bazelbuild/buildtools/buildifier
```

#### Ubuntu (Trusty)

```bash
sudo apt update
sudo apt install -y software-properties-common
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
sudo apt update
sudo apt install abi-compliance-checker abi-dumper automake build-essential ccache clang clang-format cmake curl doxygen  gawk git gcc g++ golang cmake libcurl4-openssl-dev libssl-dev libtool lsb-release make python-gunicorn python-httpbin tar wget zlib1g-dev
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-3.8 100
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-3.8 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.9 100
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.9 100
```

#### macOS (using brew)

```bash
brew install curl cmake
```

#### Windows

```bash
set PROVIDER=vcpkg
set GENERATOR="Visual Studio 15 2017 Win64"
powershell -exec bypass .\ci\install-windows.ps1
```
## Build

To build all available libraries and run the tests, run the following commands
after cloning this repo:

#### Linux and macOS

```bash
git submodule update --init --recursive
cmake -H. -Bbuild-output

# Adjust the number of threads used by modifying parameter for `-j 4`
cmake --build build-output -- -j 4

# Verify build by running tests
(cd build-output && ctest --output-on-failure)
```

You will find compiled binaries in `build-output/` respective to their source paths.

#### Windows

On Windows with MSVC use:

```bash
cmake --build build-output -- /m

# Verify build by running tests
cd build-output
ctest --output-on-failure
```

You will find compiled binaries in `build-output\` respective to their source directories.


## Versioning

This library follows [Semantic Versioning](http://semver.org/). Please note it
is currently under active development. Any release versioned 0.x.y is subject to
backwards incompatible changes at any time.

**GA**: Libraries defined at a GA quality level are expected to be stable and
all updates in the libraries are guaranteed to be backwards-compatible. Any
backwards-incompatible changes will lead to the major version increment
(1.x.y -> 2.0.0).

**Beta**: Libraries defined at a Beta quality level are expected to be mostly
stable and we're working towards their release candidate. We will address issues
and requests with a higher priority.

**Alpha**: Libraries defined at an Alpha quality level are still a
work-in-progress and are more likely to get backwards-incompatible updates.
Additionally, it's possible for Alpha libraries to get deprecated and deleted
before ever being promoted to Beta or GA.

## Contributing changes

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details on how to contribute to
this project, including how to build and test your changes as well as how to
properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](LICENSE) for details.

