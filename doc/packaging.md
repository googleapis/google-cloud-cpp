# Packaging `google-cloud-cpp`

<!-- This is an automatically generated file -->
<!-- Make changes in ci/generate-markdown/generate-packaging.sh -->

This document is intended for package maintainers or for people who might like
to "install" the `google-cloud-cpp` libraries in `/usr/local` or a similar
directory.

* Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the current
  document.
* Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](/README.md#quickstart) for the
  library or libraries they want to use.
* Developers wanting to compile the library just to run some of the examples or
  tests should consult the
  [building and installing](/README.md#building-and-installing) section of the
  top-level README file.
* Contributors and developers to `google-cloud-cpp` should consult the guide to
  [setup a development workstation][howto-setup-dev-workstation].

[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
[ninja-build]: https://ninja-build.org/
[ccmake]: https://cmake.org/cmake/help/latest/manual/ccmake.1.html
[issues-5489]: https://github.com/googleapis/google-cloud-cpp/issues/5849

There are two primary ways of obtaining `google-cloud-cpp`. You can use git:

```bash
git clone https://github.com/googleapis/google-cloud-cpp.git $HOME/google-cloud-cpp
```

Or obtain the tarball release (see
https://github.com/googleapis/google-cloud-cpp/releases for the latest
release):

```bash
VERSION="vX.Y.Z"
mkdir -p $HOME/google-cloud-cpp
wget -q https://github.com/googleapis/google-cloud-cpp/archive/${VERSION}.tar.gz
tar -xf ${VERSION}.tar.gz -C $HOME/google-cloud-cpp --strip=1
```

# Installing `google-cloud-cpp`

<!-- This is an automatically generated file -->
<!-- Make changes in ci/generate-markdown/generate-packaging.sh -->

This document provides instructions to install the dependencies of
`google-cloud-cpp`.

**If** all the dependencies of `google-cloud-cpp` are installed and provide
CMake support files, then compiling and installing the libraries
requires two commands:

```bash
cmake -H. -Bcmake-out -DBUILD_TESTING=OFF -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out --target install
```

Unfortunately getting your system to this state may require multiple steps,
the following sections describe how to install `google-cloud-cpp` on several
platforms.

## Common Configuration Variables for CMake

As is often the case, the CMake build can be configured using a number of
options and command-line flags.  A full treatment of these options is outside
the scope of this document, but here are a few highlights:

* Consider using `-GNinja` to switch the generator from `make` (or msbuild on
  Windows) to [`ninja`][ninja-build]. In our experience `ninja` takes better
  advantage of multi-core machines. Be aware that `ninja` is often not
  installed in development workstations, but it is available through most
  package managers.
* If you use the default generator, consider appending `-- -j ${NCPU}` to the
  build command, where `NCPU` is an environment variable set to the number of
  processors on your system. You can obtain this information using
  the `nproc` command on Linux, or `sysctl -n hw.physicalcpu` on macOS.
* By default, CMake compiles the `google-cloud-cpp` as static libraries. The
  standard `-DBUILD_SHARED_LIBS=ON` option can be used to switch this to shared
  libraries.  Having said this, on Windows there are [known issues][issues-5489]
  with DLLs and generated protos.
* You can compile a subset of the libraries using
  `-DGOOGLE_CLOUD_CPP_ENABLE=lib1,lib2`.

To find out about other configuration options, consider using
[`ccmake`][ccmake], or `cmake -L`.

## Using `google-cloud-cpp` after it is installed

Once installed, follow any of the [quickstart guides](/README.md#quickstart) to
use `google-cloud-cpp` in your CMake or Make-based project. If you are planning
to use Bazel for your own project, there is no need to install
`google-cloud-cpp`, we provide `BUILD.bazel` files for this purpose. The
quickstart guides also cover this use-case.

## Required Libraries

`google-cloud-cpp` directly depends on the following libraries:

| Library                           |   Minimum version | Description                                                                                   |
|-----------------------------------|------------------:|-----------------------------------------------------------------------------------------------|
| [Abseil][abseil-gh]               | 20200923, Patch 3 | Abseil C++ common library (Requires >= `20210324.2` for `pkg-config` files to work correctly) |
| [gRPC][gRPC-gh]                   |            1.35.x | An RPC library and framework (not needed for Google Cloud Storage client)                     |
| [libcurl][libcurl-gh]             |            7.47.0 | HTTP client library for the Google Cloud Storage client                                       |
| [crc32c][crc32c-gh]               |             1.0.6 | Hardware-accelerated CRC32C implementation                                                    |
| [OpenSSL][OpenSSL-gh]             |             1.0.2 | Crypto functions for Google Cloud Storage authentication                                      |
| [nlohmann/json][nlohmann-json-gh] |             3.4.0 | JSON for Modern C++                                                                           |
| [protobuf][protobuf-gh]           |             v21.0 | C++ Micro-generator support                                                                   |

[abseil-gh]: https://github.com/abseil/abseil-cpp
[gRPC-gh]: https://github.com/grpc/grpc
[libcurl-gh]: https://github.com/curl/curl
[crc32c-gh]: https://github.com/google/crc32c
[OpenSSL-gh]: https://github.com/openssl/openssl
[nlohmann-json-gh]: https://github.com/nlohmann/json
[protobuf-gh]: https://github.com/protocolbuffers/protobuf

Note that these libraries may also depend on other libraries. The following
instructions include steps to install these indirect dependencies too.

When possible, the instructions below prefer to use pre-packaged versions of
these libraries and their dependencies. In some cases the packages do not exist,
or the packaged versions are too old to support `google-cloud-cpp`. If this is
the case, the instructions describe how you can manually download and install
these dependencies.

<details>
<summary>Fedora (34)</summary>
<br>

Install the minimal development tools:

```bash
sudo dnf makecache && \
sudo dnf install -y ccache cmake curl findutils gcc-c++ git make ninja-build \
        openssl-devel patch unzip tar wget zip zlib-devel
```

Fedora 34 includes packages for gRPC and Protobuf, but they are not
recent enough to support the protos published by Google Cloud. The indirect
dependencies of libcurl, Protobuf, and gRPC are recent enough for our needs.

```bash
sudo dnf makecache && \
sudo dnf install -y c-ares-devel re2-devel libcurl-devel
```

Fedora's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
when handling `.pc` files with lots of `Requires:` deps, which happens with
Abseil. If you plan to use `pkg-config` with any of the installed artifacts,
you may want to use a recent version of the standard `pkg-config` binary. If
not, `sudo dnf install pkgconfig` should work.

```bash
mkdir -p $HOME/Downloads/pkg-config-cpp && cd $HOME/Downloads/pkg-config-cpp
curl -sSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

The following steps will install libraries and tools in `/usr/local`. By
default pkg-config does not search in these directories.

```bash
export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig
```

#### Abseil

We need a recent version of Abseil.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

The project depends on the nlohmann_json library. We use CMake to
install it as this installs the necessary CMake configuration files.
Note that this is a header-only library, and often installed manually.
This leaves your environment without support for CMake pkg-config.

```bash
mkdir -p $HOME/Downloads/json && cd $HOME/Downloads/json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
sudo cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

Unless you are only using the Google Cloud Storage library the project
needs Protobuf and gRPC. Unfortunately the version of Protobuf that ships
with Fedora 34 is not recent enough to support the protos published by
Google Cloud. We need to build from source:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

Finally we build gRPC from source also:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>openSUSE (Leap)</summary>
<br>

Install the minimal development tools, libcurl and OpenSSL. The gRPC Makefile
uses `which` to determine whether the compiler is available. Install this
command for the extremely rare case where it may be missing from your
workstation or build server.

```bash
sudo zypper refresh && \
sudo zypper install --allow-downgrade -y automake ccache cmake curl \
        gcc gcc-c++ git gzip libcurl-devel libopenssl-devel \
        libtool make patch re2-devel tar wget which zlib zlib-devel-static
```

The following steps will install libraries and tools in `/usr/local`. openSUSE
does not search for shared libraries in these directories by default, there
are multiple ways to solve this problem, the following steps are one solution:

```bash
(echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
sudo tee /etc/ld.so.conf.d/usrlocal.conf
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
export PATH=/usr/local/bin:${PATH}
```

#### Abseil

We need a recent version of Abseil.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.11, while openSUSE/Leap
distributes c-ares-1.9. Manually install a newer version:

```bash
mkdir -p $HOME/Downloads/c-ares && cd $HOME/Downloads/c-ares
curl -sSL https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We manually install it using:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

The project depends on the nlohmann_json library. We use CMake to
install it as this installs the necessary CMake configuration files.
Note that this is a header-only library, and often installed manually.
This leaves your environment without support for CMake pkg-config.

```bash
mkdir -p $HOME/Downloads/json && cd $HOME/Downloads/json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
sudo cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Ubuntu (22.04 LTS - Jammy Jellyfish)</summary>
<br>

Install the minimal development tools, libcurl, OpenSSL and libc-ares:

```bash
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ccache cmake ca-certificates curl git \
        gcc g++ libc-ares-dev libc-ares2 libcurl4-openssl-dev libre2-dev \
        libssl-dev m4 make pkg-config tar wget zlib1g-dev
```

#### Abseil

We need a recent version of Abseil.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We install it using:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

The project depends on the nlohmann_json library. We use CMake to
install it as this installs the necessary CMake configuration files.
Note that this is a header-only library, and often installed manually.
This leaves your environment without support for CMake pkg-config.

```bash
mkdir -p $HOME/Downloads/json && cd $HOME/Downloads/json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
sudo cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Ubuntu (20.04 LTS - Focal Fossa)</summary>
<br>

Install the minimal development tools, libcurl, OpenSSL and libc-ares:

```bash
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ccache cmake ca-certificates curl git \
        gcc g++ libc-ares-dev libc-ares2 libcurl4-openssl-dev libre2-dev \
        libssl-dev m4 make pkg-config tar wget zlib1g-dev
```

#### Abseil

We need a recent version of Abseil.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We install it using:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

The project depends on the nlohmann_json library. We use CMake to
install it as this installs the necessary CMake configuration files.
Note that this is a header-only library, and often installed manually.
This leaves your environment without support for CMake pkg-config.

```bash
mkdir -p $HOME/Downloads/json && cd $HOME/Downloads/json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
sudo cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Ubuntu (18.04 LTS - Bionic Beaver)</summary>
<br>

Install the minimal development tools, libcurl, OpenSSL and libc-ares:

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ccache cmake ca-certificates curl git \
        gcc g++ libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl-dev m4 \
        make pkg-config tar wget zlib1g-dev
```

#### Abseil

We need a recent version of Abseil.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### RE2

We need a newer version of RE2 than the system package provides.

```bash
mkdir -p $HOME/Downloads/re2 && cd $HOME/Downloads/re2
curl -sSL https://github.com/google/re2/archive/2020-11-01.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We install it using:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

The project depends on the nlohmann_json library. We use CMake to
install it as this installs the necessary CMake configuration files.
Note that this is a header-only library, and often installed manually.
This leaves your environment without support for CMake pkg-config.

```bash
mkdir -p $HOME/Downloads/json && cd $HOME/Downloads/json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
sudo cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Debian (11 - Bullseye)</summary>
<br>

Install the minimal development tools, libcurl, and OpenSSL:

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ca-certificates ccache cmake curl git \
        gcc g++ libc-ares-dev libc-ares2 libcurl4-openssl-dev libre2-dev \
        libssl-dev m4 make ninja-build pkg-config tar wget zlib1g-dev
```

#### Abseil

Debian 11 ships with Abseil==20200923.3.  Unfortunately, the current gRPC version needs
Abseil>=20210324.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

Debian 11 also ships with nlohmann-json==3.9.1, which is recent enough for our needs:

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y nlohmann-json3-dev
```

#### Protobuf

Unless you are only using the Google Cloud Storage library the project
needs Protobuf and gRPC. Unfortunately the version of Protobuf that ships
with Debian 11 is not recent enough to support the protos published by
Google Cloud. We need to build from source:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

Finally we build gRPC from source also:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Debian (10 - Buster)</summary>
<br>

Install the minimal development tools, libcurl, and OpenSSL:

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ca-certificates ccache cmake curl git \
        gcc g++ libc-ares-dev libc-ares2 libcurl4-openssl-dev libre2-dev \
        libssl-dev m4 make ninja-build pkg-config tar wget zlib1g-dev
```

#### Abseil

We need a recent version of Abseil.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

The project depends on the nlohmann_json library. We use CMake to
install it as this installs the necessary CMake configuration files.
Note that this is a header-only library, and often installed manually.
This leaves your environment without support for CMake pkg-config.

```bash
mkdir -p $HOME/Downloads/json && cd $HOME/Downloads/json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
sudo cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

Unless you are only using the Google Cloud Storage library the project
needs Protobuf and gRPC. Unfortunately the version of Protobuf that ships
with Debian 10 is not recent enough to support the protos published by
Google Cloud. We need to build from source:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

Finally we build gRPC from source also:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Debian (9 - Stretch)</summary>
<br>

First install the development tools and libcurl.
On Debian 9, libcurl links against openssl-1.0.2, and one must link
against the same version or risk an inconsistent configuration of the library.
This is especially important for multi-threaded applications, as openssl-1.0.2
requires explicitly setting locking callbacks. Therefore, to use libcurl one
must link against openssl-1.0.2. To do so, we need to install libssl1.0-dev.
Note that this removes libssl-dev if you have it installed already, and would
prevent you from compiling against openssl-1.1.0.

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ccache cmake ca-certificates curl git \
        gcc g++ libcurl4-openssl-dev libssl1.0-dev libtool make m4 pkg-config \
        tar wget zlib1g-dev
```

#### Abseil

We need a recent version of Abseil.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.13, while Debian Stretch
distributes c-ares-1.12. Manually install a newer version:

```bash
mkdir -p $HOME/Downloads/c-ares && cd $HOME/Downloads/c-ares
curl -sSL https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

#### RE2

We need a newer version of RE2 than the system package provides.

```bash
mkdir -p $HOME/Downloads/re2 && cd $HOME/Downloads/re2
curl -sSL https://github.com/google/re2/archive/2020-11-01.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

To install gRPC we first need to configure pkg-config to find the version of
Protobuf we just installed in `/usr/local`:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

The project depends on the nlohmann_json library. We use CMake to
install it as this installs the necessary CMake configuration files.
Note that this is a header-only library, and often installed manually.
This leaves your environment without support for CMake pkg-config.

```bash
mkdir -p $HOME/Downloads/json && cd $HOME/Downloads/json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i \
        -e '1s/VERSION 3.8/VERSION 3.5/' \
        -e '/^target_compile_features/d' \
        CMakeLists.txt && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
sudo cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Rocky Linux (8)</summary>
<br>

Install the minimal development tools, libcurl, OpenSSL, and the c-ares
library (required by gRPC):

```bash
sudo dnf makecache && \
sudo dnf update -y && \
sudo dnf install -y epel-release && \
sudo dnf makecache && \
sudo dnf install -y ccache cmake curl findutils gcc-c++ git make openssl-devel \
        patch re2-devel zlib-devel libcurl-devel c-ares-devel tar wget which
```

Rocky Linux's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is
slow when handling `.pc` files with lots of `Requires:` deps, which happens
with Abseil. If you plan to use `pkg-config` with any of the installed
artifacts, you may want to use a recent version of the standard `pkg-config`
binary. If not, `sudo dnf install pkgconfig` should work.

```bash
mkdir -p $HOME/Downloads/pkg-config-cpp && cd $HOME/Downloads/pkg-config-cpp
curl -sSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

The following steps will install libraries and tools in `/usr/local`. By
default Rocky Linux 8 does not search for shared libraries in these
directories, there are multiple ways to solve this problem, the following
steps are one solution:

```bash
(echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
sudo tee /etc/ld.so.conf.d/usrlocal.conf
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib64/pkgconfig
export PATH=/usr/local/bin:${PATH}
```

#### Abseil

We need a recent version of Abseil.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We manually install it using:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

The project depends on the nlohmann_json library. We use CMake to
install it as this installs the necessary CMake configuration files.
Note that this is a header-only library, and often installed manually.
This leaves your environment without support for CMake pkg-config.

```bash
mkdir -p $HOME/Downloads/json && cd $HOME/Downloads/json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
sudo cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>CentOS (7)</summary>
<br>

First install the development tools and OpenSSL. The development tools
distributed with CentOS 7 are too old to build the project. In these
instructions, we use `cmake3` and `gcc-7` obtained from
[Software Collections](https://www.softwarecollections.org/).

```bash
sudo rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
sudo yum install -y centos-release-scl yum-utils
sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
sudo yum makecache && \
sudo yum install -y automake ccache cmake3 curl-devel devtoolset-7 gcc gcc-c++ \
        git libtool make openssl-devel patch re2-devel tar wget which zlib-devel
sudo ln -sf /usr/bin/cmake3 /usr/bin/cmake && sudo ln -sf /usr/bin/ctest3 /usr/bin/ctest
```

Start a bash shell with its environment configured to use the tools installed
by `devtoolset-7`.
**IMPORTANT**: All the following commands should be run from this new shell.
```bash
scl enable devtoolset-7 bash
```

CentOS-7 ships with `pkg-config` 0.27.1, which has a
[bug](https://bugs.freedesktop.org/show_bug.cgi?id=54716) that can make
invocations take extremely long to complete. If you plan to use `pkg-config`
with any of the installed artifacts, you'll want to upgrade it to something
newer. If not, `sudo yum install pkgconfig` should work instead.

```bash
mkdir -p $HOME/Downloads/pkg-config-cpp && cd $HOME/Downloads/pkg-config-cpp
curl -sSL https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --with-internal-glib && \
    make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

The following steps will install libraries and tools in `/usr/local`. By
default CentOS-7 does not search for shared libraries in these directories,
there are multiple ways to solve this problem, the following steps are one
solution:

```bash
(echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
sudo tee /etc/ld.so.conf.d/usrlocal.conf
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib64/pkgconfig
export PATH=/usr/local/bin:${PATH}
```

#### Abseil

We need a recent version of Abseil.

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_CXX_STANDARD=11 \
      -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -sSL https://github.com/protocolbuffers/protobuf/archive/v21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.11, while CentOS-7
distributes c-ares-1.10. Manually install a newer version:

```bash
mkdir -p $HOME/Downloads/c-ares && cd $HOME/Downloads/c-ares
curl -sSL https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We manually install it using:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -sSL https://github.com/grpc/grpc/archive/v1.46.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
mkdir -p $HOME/Downloads/crc32c && cd $HOME/Downloads/crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### nlohmann_json library

The project depends on the nlohmann_json library. We use CMake to
install it as this installs the necessary CMake configuration files.
Note that this is a header-only library, and often installed manually.
This leaves your environment without support for CMake pkg-config.

```bash
mkdir -p $HOME/Downloads/json && cd $HOME/Downloads/json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.5.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -H. -Bcmake-out/nlohmann/json && \
sudo cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -H. -Bcmake-out \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_INSTALL_MESSAGE=NEVER \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>
<details>
<summary>macOS</summary>
<br>

First install Xcode to get all the needed development tools. These instructions
also use [Homebrew](https://brew.sh/) to install the needed third-party
dependencies.

If you don't already have Xcode installed:
```bash
xcode-select --install
```

Follow the instructions at https://brew.sh to install Homebrew. Then install
the needed dependencies:

```bash
# Some additional build tools
brew install cmake ninja
# Installs google-cloud-cpp's needed deps
brew install abseil protobuf grpc nlohmann-json crc32c openssl@1.1
```

Now configure, build, and install the `google-cloud-cpp` libraries that you need.
In this example, we install the [storage][storage-link] and
[spanner][spanner-link] libraries.
```bash
cmake -S . -B cmake-out \
  -GNinja \
  -DGOOGLE_CLOUD_CPP_ENABLE="storage;spanner" \
  -DCMAKE_CXX_STANDARD=11 \
  -DCMAKE_BUILD_TYPE=release \
  -DBUILD_TESTING=OFF \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@1.1)" \
  -DCMAKE_INSTALL_PREFIX=/tmp/test-install
cmake --build cmake-out
cmake --build cmake-out --target install
```

</details>

[storage-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/storage#readme
[spanner-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/spanner#readme
