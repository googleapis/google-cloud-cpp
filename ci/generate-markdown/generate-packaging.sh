#!/usr/bin/env bash
#
# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eu

readonly BINDIR=$(dirname "$0")

cat <<'END_OF_PREAMBLE'
# Packaging `google-cloud-cpp`

<!-- This is an automatically generated file -->
<!-- Make changes in ci/generate-markdown/generate-packaging.sh -->

This document is intended for package maintainers or anyone else who might like
to "install" the `google-cloud-cpp` libraries.

There are two primary ways of obtaining `google-cloud-cpp`. You can use git:

```bash
git clone https://github.com/googleapis/google-cloud-cpp.git $HOME/google-cloud-cpp
```

Or obtain the tarball release (see
https://github.com/googleapis/google-cloud-cpp/releases for the latest
release):

```bash
VERSION="v.X.Y.Z"
mkdir -p $HOME/google-cloud-cpp
wget -q https://github.com/googleapis/google-cloud-cpp/archive/${VERSION}.tar.gz
tar -xf ${VERSION}.tar.gz -C $HOME/google-cloud-cpp --strip=1
```

# Installing `google-cloud-cpp`

<!-- This is an automatically generated file -->
<!-- Make changes in ci/generate-markdown/generate-packaging.sh -->

By default `google-cloud-cpp` libraries download and compile all their
dependencies ([see below](#required-libraries) for a complete list). This makes
it easier for users to "take the library for a spin", and works well for users
that "Live at Head", but does not work for package maintainers or users that
prefer to compile their dependencies once and install them in `/usr/local/` or
a similar directory.

This document provides instructions to install the dependencies of
`google-cloud-cpp`.

**If** all the dependencies of `google-cloud-cpp` are installed and provide
CMake support files, then compiling and installing the libraries
requires two commands:

```bash
cmake -H. -Bcmake-out
cmake --build cmake-out --target install
```

You may choose to parallelize the build by appending `-- -j ${NCPU}` to the
build command, where `NCPU` is an environment variable set to the number of
processors on your system. On Linux, you can obtain this information using the
`nproc` command or `sysctl -n hw.physicalcpu` on Mac.

Unfortunately getting your system to this state may require multiple steps,
the following sections describe how to install `google-cloud-cpp` on several
platforms.

## Using `google-cloud-cpp` in CMake-based projects.

Once you have installed `google-cloud-cpp` you can use the libraries from
your own projects using `find_package()` in your `CMakeLists.txt` file:

```CMake
cmake_minimum_required(VERSION 3.5)

find_package(storage_client REQUIRED)

add_executable(my_program my_program.cc)
target_link_libraries(my_program storage_client)
```

You can use a similar `CMakeLists.txt` to use the Cloud Bigtable C++ client:

```CMake
cmake_minimum_required(VERSION 3.5)

find_package(bigtable_client REQUIRED)

add_executable(my_program my_program.cc)
target_link_libraries(my_program bigtable_client)
```

## Using `google-cloud-cpp` in Make-based projects.

Once you have installed `google-cloud-cpp` you can use the libraries in your
own Make-based projects using `pkg-config`:

```Makefile
GCS_CXXFLAGS   := $(shell pkg-config storage_client --cflags)
GCS_CXXLDFLAGS := $(shell pkg-config storage_client --libs-only-L)
GCS_LIBS       := $(shell pkg-config storage_client --libs-only-l)

my_storage_program: my_storage_program.cc
        $(CXX) $(CXXFLAGS) $(GCS_CXXFLAGS) $(GCS_CXXLDFLAGS) -o $@ $^ $(GCS_LIBS)

CBT_CXXFLAGS   := $(shell pkg-config bigtable_client --cflags)
CBT_CXXLDFLAGS := $(shell pkg-config bigtable_client --libs-only-L)
CBT_LIBS       := $(shell pkg-config bigtable_client --libs-only-l)

my_bigtable_program: my_bigtable_program.cc
        $(CXX) $(CXXFLAGS) $(CBT_CXXFLAGS) $(CBT_CXXLDFLAGS) -o $@ $^ $(CBT_LIBS)
```

## Using `google-cloud-cpp` in Bazel-based projects.

If you use `Bazel` for your builds you do not need to install
`google-cloud-cpp`. We provide a Starlark function to automatically download and
compile `google-cloud-cpp` as part of you Bazel build. Add the following
commands to your `WORKSPACE` file:

```Python
# Update the version and SHA256 digest as needed.
http_archive(
    name = "com_github_googleapis_google_cloud_cpp",
    url = "http://github.com/googleapis/google-cloud-cpp/archive/v0.10.0.tar.gz",
    strip_prefix = "google-cloud-cpp-0.10.0",
    sha256 = "fd0c3e3b50f32af332b53857f8cd1bfa009e33d1eeecabc5c79a4825d906a90c",
)

load("@com_github_googleapis_google_cloud_cpp//bazel:google_cloud_cpp_deps.bzl", "google_cloud_cpp_deps")
google_cloud_cpp_deps()
# Have to manually call the corresponding function for gRPC:
#   https://github.com/bazelbuild/bazel/issues/1550
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()
load("@upb//bazel:workspace_deps.bzl", "upb_deps")
upb_deps()
load("@build_bazel_rules_apple//apple:repositories.bzl", "apple_rules_dependencies")
apple_rules_dependencies()
load("@build_bazel_apple_support//lib:repositories.bzl", "apple_support_dependencies")
apple_support_dependencies()
```

Then you can link the libraries from your `BUILD` files:

```Python
cc_binary(
    name = "bigtable_install_test",
    srcs = [
        "bigtable_install_test.cc",
    ],
    deps = [
        "@com_github_googleapis_google_cloud_cpp//google/cloud/bigtable:bigtable_client",
    ],
)

cc_binary(
    name = "storage_install_test",
    srcs = [
        "storage_install_test.cc",
    ],
    deps = [
        "@com_github_googleapis_google_cloud_cpp//google/cloud/storage:storage_client",
    ],
)
```

## Required Libraries

`google-cloud-cpp` directly depends on the following libraries:

| Library | Minimum version | Description |
| ------- | --------------: | ----------- |
| [gRPC][gRPC-gh] | 1.16.x | gRPC++ for Cloud Bigtable |
| [libcurl][libcurl-gh] | 7.47.0  | HTTP client library for the Google Cloud Storage client |
| [crc32c][crc32c-gh]  | 1.0.6 | Hardware-accelerated CRC32C implementation |
| [OpenSSL][OpenSSL-gh] | 1.0.2 | Crypto functions for Google Cloud Storage authentication |
| [nlohmann/json][nlohmann-json-gh] | 3.4.0 | JSON for Modern C++ |

[gRPC-gh]: https://github.com/grpc/grpc
[libcurl-gh]: https://github.com/curl/curl
[crc32c-gh]: https://github.com/google/crc32c
[OpenSSL-gh]: https://github.com/openssl/openssl
[nlohmann-json-gh]: https://github.com/nlohmann/json

Note that these libraries may also depend on other libraries. The following
instructions include steps to install these indirect dependencies too.

When possible, the instructions below prefer to use pre-packaged versions of
these libraries and their dependencies. In some cases the packages do not exist,
or the package versions are too old to support `google-cloud-cpp`. If this is
the case, the instructions describe how you can manually download and install
these dependencies.

## Table of Contents

- [Fedora 31](#fedora-31)
- [openSUSE (Tumbleweed)](#opensuse-tumbleweed)
- [openSUSE (Leap)](#opensuse-leap)
- [Ubuntu (20.04 LTS - Focal Fossa)](#ubuntu-2004-lts---focal-fossa)
- [Ubuntu (18.04 LTS - Bionic Beaver)](#ubuntu-1804-lts---bionic-beaver)
- [Ubuntu (16.04 LTS - Xenial Xerus)](#ubuntu-1604-lts---xenial-xerus)
- [Debian (Buster)](#debian-buster)
- [Debian (Stretch)](#debian-stretch)
- [CentOS 8](#centos-8)
- [CentOS 7](#centos-7)
END_OF_PREAMBLE

readonly DOCKERFILES_DIR="${BINDIR}/../kokoro/install"

echo
echo "### Fedora (31)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.fedora"

echo
echo "### openSUSE (Tumbleweed)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.opensuse-tumbleweed"

echo
echo "### openSUSE (Leap)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.opensuse-leap"

echo
echo "### Ubuntu (20.04 LTS - Focal Fossa)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.ubuntu-focal"

echo
echo "### Ubuntu (18.04 LTS - Bionic Beaver)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.ubuntu-bionic"

echo
echo "### Ubuntu (16.04 LTS - Xenial Xerus)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.ubuntu-xenial"

echo
echo "### Debian (Buster)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.debian-buster"

echo
echo "### Debian (Stretch)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.debian-stretch"

echo
echo "### CentOS (8)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.centos-8"

echo
echo "### CentOS (7)"
"${BINDIR}/extract-install.sh" "${DOCKERFILES_DIR}/Dockerfile.centos-7"
