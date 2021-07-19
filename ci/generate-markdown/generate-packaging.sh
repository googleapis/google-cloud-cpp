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

BINDIR=$(dirname "$0")
readonly BINDIR

cat <<'END_OF_PREAMBLE'
# Packaging `google-cloud-cpp`

<!-- This is an automatically generated file -->
<!-- Make changes in ci/generate-markdown/generate-packaging.sh -->

This document is intended for package maintainers or for people who might like
to "install" the `google-cloud-cpp` libraries in `/usr/local` or a similar
directory.

* Packaging maintainers or developers that prefer to install the library in a
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
cmake -H. -Bcmake-out -DBUILD_TESTING=OFF -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
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

find_package(google_cloud_cpp_storage REQUIRED)

add_executable(my_program my_program.cc)
target_link_libraries(my_program google-cloud-cpp::storage)
```

You can use a similar `CMakeLists.txt` to use the Cloud Bigtable C++ client:

```CMake
cmake_minimum_required(VERSION 3.5)

find_package(google_cloud_cpp_bigtable REQUIRED)

add_executable(my_program my_program.cc)
target_link_libraries(my_program google-cloud-cpp::bigtable)
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
    url = "http://github.com/googleapis/google-cloud-cpp/archive/v1.19.0.tar.gz",
    strip_prefix = "google-cloud-cpp-1.19.0",
    sha256 = "33eb349cf5f033704a4299b0ac57e3a8b4973ca92f4491aef822bfeb41e69d27",
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
| [Abseil][abseil-gh] | 20200923, Patch 3 | Abseil C++ common library (Requires >= `20210324.2` for `pkg-config` files to work correctly)|
| [gRPC][gRPC-gh] | 1.35.x | An RPC library and framework (not needed for Google Cloud Storage client) |
| [libcurl][libcurl-gh] | 7.47.0  | HTTP client library for the Google Cloud Storage client |
| [crc32c][crc32c-gh]  | 1.0.6 | Hardware-accelerated CRC32C implementation |
| [OpenSSL][OpenSSL-gh] | 1.0.2 | Crypto functions for Google Cloud Storage authentication |
| [nlohmann/json][nlohmann-json-gh] | 3.4.0 | JSON for Modern C++ |
| [protobuf][protobuf-gh] | 3.15.8 | C++ Microgenerator support |

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
or the package versions are too old to support `google-cloud-cpp`. If this is
the case, the instructions describe how you can manually download and install
these dependencies.
END_OF_PREAMBLE

# Extracts the part of a file between the BEGIN/DONE tags.
function extract() {
  sed -e '0,/^.*\[BEGIN packaging.md\].*$/d' \
    -e '/^.*\[DONE packaging.md\].*$/,$d' "$1"
}

# A "map" (comma separated) of dockerfile -> summary.
DOCKER_DISTROS=(
  "demo-fedora.Dockerfile,Fedora (33)"
  "demo-opensuse-leap.Dockerfile,openSUSE (Leap)"
  "demo-ubuntu-focal.Dockerfile,Ubuntu (20.04 LTS - Focal Fossa)"
  "demo-ubuntu-bionic.Dockerfile,Ubuntu (18.04 LTS - Bionic Beaver)"
  "demo-ubuntu-xenial.Dockerfile,Ubuntu (16.04 LTS - Xenial Xerus)"
  "demo-debian-buster.Dockerfile,Debian (Buster)"
  "demo-debian-stretch.Dockerfile,Debian (Stretch)"
  "demo-rockylinux-8.Dockerfile,Rocky Linux (8)"
  "demo-centos-7.Dockerfile,CentOS (7)"
)
for distro in "${DOCKER_DISTROS[@]}"; do
  dockerfile="$(cut -f1 -d, <<<"${distro}")"
  summary="$(cut -f2- -d, <<<"${distro}")"
  echo
  echo "<details>"
  echo "<summary>${summary}</summary>"
  echo "<br>"
  extract "${BINDIR}/../cloudbuild/dockerfiles/${dockerfile}" |
    "${BINDIR}/dockerfile2markdown.sh"
  echo "#### Compile and install the main project"
  echo
  echo "We can now compile and install \`google-cloud-cpp\`"
  echo
  echo '```bash'
  extract "${BINDIR}/../cloudbuild/builds/demo-install.sh"
  echo '```'
  echo
  echo "</details>"
done
