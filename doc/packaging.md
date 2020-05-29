# Packaging `google-cloud-cpp`

<!-- This is an automatically generated file -->
<!-- Make changes in ci/test-markdown/generate-packaging.sh -->

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
<!-- Make changes in ci/test-markdown/generate-packaging.sh -->

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
| gRPC    | 1.16.x | gRPC++ for Cloud Bigtable |
| libcurl | 7.47.0  | HTTP client library for the Google Cloud Storage client |
| crc32c  | 1.0.6 | Hardware-accelerated CRC32C implementation |
| OpenSSL | 1.0.2 | Crypto functions for Google Cloud Storage authentication |

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
- [Ubuntu (20.04 - Bionic Beaver)](#ubuntu-2004---focal-fossa)
- [Ubuntu (18.04 - Bionic Beaver)](#ubuntu-1804---bionic-beaver)
- [Ubuntu (16.04 - Xenial Xerus)](#ubuntu-1604---xenial-xerus)
- [Debian (Buster)](#debian-buster)
- [Debian (Stretch)](#debian-stretch)
- [CentOS 8](#centos-8)
- [CentOS 7](#centos-7)

### Fedora (31)

Install the minimal development tools:

```bash
sudo dnf makecache && \
sudo dnf install -y ccache cmake gcc-c++ git make openssl-devel pkgconfig \
        zlib-devel
```

Fedora 31 includes packages for gRPC, libcurl, and OpenSSL that are recent
enough for the project. Install these packages and additional development
tools to compile the dependencies:

```bash
sudo dnf makecache && \
sudo dnf install -y grpc-devel grpc-plugins \
        libcurl-devel protobuf-compiler tar wget zlib-devel
```

The following steps will install libraries and tools in `/usr/local`. By
default pkg-config does not search in these directories.

```bash
export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig
```

#### Abseil

We need a recent version of Abseil.

```bash
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```


### openSUSE (Tumbleweed)

Install the minimal development tools, libcurl and OpenSSL:

```bash
sudo zypper refresh && \
sudo zypper install --allow-downgrade -y ccache cmake gcc gcc-c++ git gzip \
        libcurl-devel libopenssl-devel make tar wget zlib-devel
```

The versions of gRPC and Protobuf packaged with openSUSE/Tumbleweed are recent
enough to support the Google Cloud Platform proto files.

```bash
sudo zypper refresh && \
sudo zypper install -y grpc-devel
```

The following steps will install libraries and tools in `/usr/local`. By
default pkg-config does not search in these directories.

```bash
export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig
```

#### Abseil

We need a recent version of Abseil.

```bash
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```


### openSUSE (Leap)

Install the minimal development tools, libcurl and OpenSSL. The gRPC Makefile
uses `which` to determine whether the compiler is available. Install this
command for the extremely rare case where it may be missing from your
workstation or build server.

```bash
sudo zypper refresh && \
sudo zypper install --allow-downgrade -y automake ccache cmake gcc gcc-c++ git \
        gzip libcurl-devel libopenssl-devel libtool make tar wget which
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
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.11.3.tar.gz && \
    tar -xf v3.11.3.tar.gz && \
    cd protobuf-3.11.3/cmake && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.11, while openSUSE/Leap
distributes c-ares-1.9. Manually install a newer version:

```bash
cd $HOME/Downloads
wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz && \
    tar -xf cares-1_14_0.tar.gz && \
    cd c-ares-cares-1_14_0 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We manually install it using:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.29.1.tar.gz && \
    tar -xf v1.29.1.tar.gz && \
    cd grpc-1.29.1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
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
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```


### Ubuntu (20.04 - Focal Fossa)

Install the minimal development tools, libcurl, OpenSSL and libc-ares:

```bash
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ccache cmake ca-certificates git gcc g++ \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl-dev m4 make \
        pkg-config tar wget zlib1g-dev
```

#### Abseil

We need a recent version of Abseil.

```bash
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.11.3.tar.gz && \
    tar -xf v3.11.3.tar.gz && \
    cd protobuf-3.11.3/cmake && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We install it using:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.29.1.tar.gz && \
    tar -xf v1.29.1.tar.gz && \
    cd grpc-1.29.1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
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
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```


### Ubuntu (18.04 - Bionic Beaver)

Install the minimal development tools, libcurl, OpenSSL and libc-ares:

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ccache cmake ca-certificates git gcc g++ \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl-dev m4 make \
        pkg-config tar wget zlib1g-dev
```

#### Abseil

We need a recent version of Abseil.

```bash
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.11.3.tar.gz && \
    tar -xf v3.11.3.tar.gz && \
    cd protobuf-3.11.3/cmake && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We install it using:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.29.1.tar.gz && \
    tar -xf v1.29.1.tar.gz && \
    cd grpc-1.29.1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
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
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```


### Ubuntu (16.04 - Xenial Xerus)

Install the minimal development tools, OpenSSL and libcurl:

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ccache cmake ca-certificates git gcc g++ \
        libcurl4-openssl-dev libssl-dev libtool m4 make \
        pkg-config tar wget zlib1g-dev
```

#### Abseil

We need a recent version of Abseil.

```bash
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.11.3.tar.gz && \
    tar -xf v3.11.3.tar.gz && \
    cd protobuf-3.11.3/cmake && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.11, while Ubuntu-16.04
distributes c-ares-1.10. Manually install a newer version:

```bash
cd $HOME/Downloads
wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz && \
    tar -xf cares-1_14_0.tar.gz && \
    cd c-ares-cares-1_14_0 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We can install gRPC from source using:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.29.1.tar.gz && \
    tar -xf v1.29.1.tar.gz && \
    cd grpc-1.29.1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
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
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```


### Debian (Buster)

Install the minimal development tools, libcurl, and OpenSSL:

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ca-certificates ccache cmake git gcc g++ \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl-dev m4 make \
        pkg-config tar wget zlib1g-dev
```

Debian 10 includes versions of gRPC and Protobuf that support the
Google Cloud Platform proto files. We simply install these pre-built versions:

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y libgrpc++-dev libprotobuf-dev libc-ares-dev \
        protobuf-compiler protobuf-compiler-grpc
```

#### Abseil

We need a recent version of Abseil.

```bash
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### crc32c

The project depends on the Crc32c library, we need to compile this from
source:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```


### Debian (Stretch)

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
        automake build-essential ccache cmake ca-certificates git gcc g++ \
        libcurl4-openssl-dev libssl1.0-dev libtool make m4 pkg-config tar wget \
        zlib1g-dev
```

#### Abseil

We need a recent version of Abseil.

```bash
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.11.3.tar.gz && \
    tar -xf v3.11.3.tar.gz && \
    cd protobuf-3.11.3/cmake && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.13, while Debian Stretch
distributes c-ares-1.12. Manually install a newer version:

```bash
cd $HOME/Downloads
wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz && \
    tar -xf cares-1_14_0.tar.gz && \
    cd c-ares-cares-1_14_0 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

#### gRPC

To install gRPC we first need to configure pkg-config to find the version of
Protobuf we just installed in `/usr/local`:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.29.1.tar.gz && \
    tar -xf v1.29.1.tar.gz && \
    cd grpc-1.29.1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
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
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```


### CentOS (8)

Install the minimal development tools, libcurl, OpenSSL, and the c-ares
library (required by gRPC):

```bash
sudo dnf makecache && \
sudo dnf install -y epel-release && \
sudo dnf makecache && \
sudo dnf install -y ccache cmake gcc-c++ git make openssl-devel pkgconfig \
        zlib-devel libcurl-devel c-ares-devel tar wget which
```

The following steps will install libraries and tools in `/usr/local`. By
default CentOS-8 does not search for shared libraries in these directories,
there are multiple ways to solve this problem, the following steps are one
solution:

```bash
(echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
sudo tee /etc/ld.so.conf.d/usrlocal.conf
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
export PATH=/usr/local/bin:${PATH}
```

#### Abseil

We need a recent version of Abseil.

```bash
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.11.3.tar.gz && \
    tar -xf v3.11.3.tar.gz && \
    cd protobuf-3.11.3/cmake && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We manually install it using:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.29.1.tar.gz && \
    tar -xf v1.29.1.tar.gz && \
    cd grpc-1.29.1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
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
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```


### CentOS (7)

First install the development tools and OpenSSL. The development tools
distributed with CentOS 7 (notably CMake) are too old to build
the project. In these instructions, we use `cmake3` obtained from
[Software Collections](https://www.softwarecollections.org/).

```bash
sudo rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
sudo yum install -y centos-release-scl yum-utils
sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
sudo yum makecache && \
sudo yum install -y automake ccache cmake3 curl-devel gcc gcc-c++ git libtool \
        make openssl-devel pkgconfig tar wget which zlib-devel
sudo ln -sf /usr/bin/cmake3 /usr/bin/cmake && sudo ln -sf /usr/bin/ctest3 /usr/bin/ctest
```

The following steps will install libraries and tools in `/usr/local`. By
default CentOS-7 does not search for shared libraries in these directories,
there are multiple ways to solve this problem, the following steps are one
solution:

```bash
(echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
sudo tee /etc/ld.so.conf.d/usrlocal.conf
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
export PATH=/usr/local/bin:${PATH}
```

#### Abseil

We need a recent version of Abseil.

```bash
cd $HOME/Downloads
wget -q https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz && \
    tar -xf 20200225.2.tar.gz && \
    cd abseil-cpp-20200225.2 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h" && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out/abseil && \
sudo cmake --build cmake-out/abseil --target install -- -j ${NCPU} && \
sudo ldconfig && \
    cd /var/tmp && rm -fr build
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.11.3.tar.gz && \
    tar -xf v3.11.3.tar.gz && \
    cd protobuf-3.11.3/cmake && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.11, while CentOS-7
distributes c-ares-1.10. Manually install a newer version:

```bash
cd $HOME/Downloads
wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz && \
    tar -xf cares-1_14_0.tar.gz && \
    cd c-ares-cares-1_14_0 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We manually install it using:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.29.1.tar.gz && \
    tar -xf v1.29.1.tar.gz && \
    cd grpc-1.29.1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
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
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.1.0.tar.gz && \
    tar -xf 1.1.0.tar.gz && \
    cd crc32c-1.1.0 && \
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

#### Compile and install the main project

We can now compile, test, and install `google-cloud-cpp`.

```bash
cd $HOME/project
cmake -DBUILD_TESTING=OFF -H. -Bcmake-out
cmake --build cmake-out -- -j "${NCPU:-4}"
sudo cmake --build cmake-out --target install
```

