# Installing google-cloud-cpp

By default google-cloud-cpp libraries download and compile all their
dependencies ([see below](#required-libraries) for a complete list). This makes
it easier for users to "take the library for a spin", and works well for users
that "Live at Head", but does not work for package maintainers or users that
prefer to compile their dependencies once and install them in `/usr/local/` or a
similar directory.

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

- [Fedora 30](#fedora-30)
- [openSUSE (Tumbleweed)](#opensuse-tumbleweed)
- [openSUSE (Leap)](#opensuse-leap)
- [Ubuntu (18.04 - Bionic Beaver)](#ubuntu-1804---bionic-beaver)
- [Ubuntu (16.04 - Xenial Xerus)](#ubuntu-1604---xenial-xerus)
- [Ubuntu (16.04 - Trusty Tahr)](#ubuntu-1404---trusty-tahr)
- [Debian (Stretch)](#debian-stretch)
- [CentOS 7](#centos-7)

### Fedora (30)

Install the minimal development tools:

```bash
sudo dnf makecache && \
sudo dnf install -y cmake gcc-c++ git make openssl-devel pkgconfig \
        zlib-devel
```

Fedora includes packages for gRPC, libcurl, and OpenSSL that are recent enough
for `google-cloud-cpp`. Install these packages and additional development
tools to compile the dependencies:

```bash
sudo dnf makecache && \
sudo dnf install -y grpc-devel grpc-plugins \
        libcurl-devel protobuf-compiler tar wget zlib-devel
```

#### crc32c

There is no Fedora package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd $HOME/Downloads/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c
sudo cmake --build cmake-out/crc32c --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googleapis

There is no Fedora package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.5.tar.gz
tar -xf v0.1.5.tar.gz
cd $HOME/Downloads/cpp-cmakefiles-0.1.5
cmake \
    -DBUILD_SHARED_LIBS=YES \
    -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googletest

We need a recent version of GoogleTest to compile the unit and integration
tests.

```bash
cd $HOME/Downloads
wget -q https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
tar -xf b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
cd $HOME/Downloads/googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb
cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### google-cloud-cpp

We can now compile and install `google-cloud-cpp`.

```bash
cd $HOME/google-cloud-cpp
cmake -H. -Bcmake-out
cmake --build cmake-out -- -j ${NCPU:-4}
cd $HOME/google-cloud-cpp/cmake-out
ctest --output-on-failure
sudo cmake --build . --target install
```


### openSUSE (Tumbleweed)

Install the minimal development tools:

```bash
sudo zypper refresh && \
sudo zypper install --allow-downgrade -y cmake gcc gcc-c++ git gzip \
        libcurl-devel libopenssl-devel make tar wget zlib-devel
```

openSUSE Tumbleweed provides packages for gRPC, libcurl, and protobuf, and the
versions of these packages are recent enough to support the Google Cloud
Platform proto files.

```bash
sudo zypper refresh && \
sudo zypper install -y grpc-devel gzip libcurl-devel tar wget
```

#### crc32c

There is no openSUSE package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd $HOME/Downloads/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c
sudo cmake --build cmake-out/crc32c --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googleapis

There is no openSUSE package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.5.tar.gz
tar -xf v0.1.5.tar.gz
cd $HOME/Downloads/cpp-cmakefiles-0.1.5
cmake \
    -DBUILD_SHARED_LIBS=YES \
    -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googletest

We need a recent version of GoogleTest to compile the unit and integration
tests.

```bash
cd $HOME/Downloads
wget -q https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
tar -xf b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
cd $HOME/Downloads/googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb
cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### google-cloud-cpp

We can now compile and install `google-cloud-cpp`.

```bash
cd $HOME/google-cloud-cpp
cmake -H. -Bcmake-out
cmake --build cmake-out -- -j ${NCPU:-4}
cd $HOME/google-cloud-cpp/cmake-out
ctest --output-on-failure
sudo cmake --build . --target install
```


### openSUSE (Leap)

Install the minimal development tools:

```bash
sudo zypper refresh && \
sudo zypper install --allow-downgrade -y cmake gcc gcc-c++ git gzip \
        libcurl-devel libopenssl-devel make tar wget
```

#### crc32c

There is no openSUSE package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd $HOME/Downloads/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c
sudo cmake --build cmake-out/crc32c --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### Protobuf

openSUSE Leap includes a package for protobuf-2.6, but this is too old to
support the Google Cloud Platform proto files, or to support gRPC for that
matter. Manually install protobuf:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.6.1.tar.gz
tar -xf v3.6.1.tar.gz
cd $HOME/Downloads/protobuf-3.6.1/cmake
cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.11, while openSUSE Leap
distributes c-ares-1.9. We need some additional development tools to compile
this library:

```bash
sudo zypper refresh && \
sudo zypper install -y automake libtool
```

Manually install a newer version:

```bash
cd $HOME/Downloads
wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz
tar -xf cares-1_14_0.tar.gz
cd $HOME/Downloads/c-ares-cares-1_14_0
./buildconf && ./configure && make -j ${NCPU:-4}
sudo make install
sudo ldconfig
```

#### gRPC

The gRPC Makefile uses `which` to determine whether the compiler is available.
Install this command for the extremely rare case where it may be missing from
your workstation or build server:

```bash
sudo zypper refresh && \
sudo zypper install -y which
```

Then gRPC can be manually installed using:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.19.1.tar.gz
tar -xf v1.19.1.tar.gz
cd $HOME/Downloads/grpc-1.19.1
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
export PATH=/usr/local/bin:${PATH}
make -j ${NCPU:-4}
sudo make install
sudo ldconfig
```

#### googleapis

There is no openSUSE package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.5.tar.gz
tar -xf v0.1.5.tar.gz
cd $HOME/Downloads/cpp-cmakefiles-0.1.5
cmake \
    -DBUILD_SHARED_LIBS=YES \
    -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googletest

We need a recent version of GoogleTest to compile the unit and integration
tests.

```bash
cd $HOME/Downloads
wget -q https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
tar -xf b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
cd $HOME/Downloads/googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb
cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### google-cloud-cpp

We can now compile and install `google-cloud-cpp`.

```bash
cd $HOME/google-cloud-cpp
cmake -H. -Bcmake-out
cmake --build cmake-out -- -j ${NCPU:-4}
cd $HOME/google-cloud-cpp/cmake-out
ctest --output-on-failure
sudo cmake --build . --target install
```


### Ubuntu (18.04 - Bionic Beaver)

Install the minimal development tools:

```bash
sudo apt update && \
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl-dev make \
        pkg-config tar wget zlib1g-dev
```

#### crc32c

There is no Ubuntu package for this library. To install it use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd $HOME/Downloads/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c
sudo cmake --build cmake-out/crc32c --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### Protobuf

While protobuf-3.0 is distributed with Ubuntu, the Google Cloud Plaform proto
files require more recent versions (circa 3.4.x). To manually install a more
recent version use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.6.1.tar.gz
tar -xf v3.6.1.tar.gz
cd $HOME/Downloads/protobuf-3.6.1/cmake
cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### gRPC

Likewise, Ubuntu has packages for grpc-1.3.x, but this version is too old for
the Google Cloud Platform APIs:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.19.1.tar.gz
tar -xf v1.19.1.tar.gz
cd $HOME/Downloads/grpc-1.19.1
make -j ${NCPU:-4}
sudo make install
sudo ldconfig
```

#### googleapis

There is no Ubuntu package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.5.tar.gz
tar -xf v0.1.5.tar.gz
cd $HOME/Downloads/cpp-cmakefiles-0.1.5
cmake \
    -DBUILD_SHARED_LIBS=YES \
    -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googletest

We need a recent version of GoogleTest to compile the unit and integration
tests.

```bash
cd $HOME/Downloads
wget -q https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
tar -xf b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
cd $HOME/Downloads/googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb
cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### google-cloud-cpp

Finally we can install `google-cloud-cpp`.

```bash
cd $HOME/google-cloud-cpp
cmake -H. -Bcmake-out
cmake --build cmake-out -- -j ${NCPU:-4}
cd $HOME/google-cloud-cpp/cmake-out
ctest --output-on-failure
sudo cmake --build . --target install
```


### Ubuntu (16.04 - Xenial Xerus)

Install the minimal development tools:

```bash
sudo apt update && \
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libcurl4-openssl-dev libssl-dev make \
        pkg-config tar wget zlib1g-dev
```

#### crc32c

There is no Ubuntu-16.04 package for this library. To install it use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd $HOME/Downloads/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c
sudo cmake --build cmake-out/crc32c --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### Protobuf

While protobuf-2.6 is distributed with Ubuntu-16.04, the Google Cloud Plaform
proto files require more recent versions (circa 3.4.x). To manually install a
more recent version use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.6.1.tar.gz
tar -xf v3.6.1.tar.gz
cd $HOME/Downloads/protobuf-3.6.1/cmake
cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.11, while Ubuntu-16.04
distributes c-ares-1.10. We need some additional development tools to compile
this library:

```bash
sudo apt update && \
sudo apt install -y automake libtool
```

After installing these tools we can manually install a newer version
of c-ares:

```bash
cd $HOME/Downloads
wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz
tar -xf cares-1_14_0.tar.gz
cd $HOME/Downloads/c-ares-cares-1_14_0
./buildconf && ./configure && make -j ${NCPU:-4}
sudo make install
sudo ldconfig
```

#### gRPC

Ubuntu-16.04 does not provide packages for the C++ gRPC bindings, install the
library manually:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.19.1.tar.gz
tar -xf v1.19.1.tar.gz
cd $HOME/Downloads/grpc-1.19.1
make -j ${NCPU:-4}
sudo make install
sudo ldconfig
```

#### googleapis

There is no Ubuntu package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.5.tar.gz
tar -xf v0.1.5.tar.gz
cd $HOME/Downloads/cpp-cmakefiles-0.1.5
cmake \
    -DBUILD_SHARED_LIBS=YES \
    -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googletest

We need a recent version of GoogleTest to compile the unit and integration
tests.

```bash
cd $HOME/Downloads
wget -q https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
tar -xf b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
cd $HOME/Downloads/googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb
cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### google-cloud-cpp

Finally we can install `google-cloud-cpp`.

```bash
cd $HOME/google-cloud-cpp
cmake -H. -Bcmake-out
cmake --build cmake-out -- -j ${NCPU:-4}
cd $HOME/google-cloud-cpp/cmake-out
ctest --output-on-failure
sudo cmake --build . --target install
```


### Ubuntu (14.04 - Trusty Tahr)

Install the minimal development tools.
We use the `ubuntu-toolchain-r` PPA to get a modern version of CMake:

```bash
sudo apt update && sudo apt install -y software-properties-common
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
sudo apt update && \
sudo apt install -y cmake3 git gcc g++ make pkg-config tar wget \
        zlib1g-dev
```

Ubuntu:14.04 ships with a very old version of OpenSSL, this version is not
supported by gRPC. We need to compile and install OpenSSL-1.0.2 from source.

```bash
cd $HOME/Downloads
wget -q https://www.openssl.org/source/openssl-1.0.2n.tar.gz
tar xf openssl-1.0.2n.tar.gz
cd $HOME/Downloads/openssl-1.0.2n
./config --shared
make -j ${NCPU:-4}
sudo make install
```

Note that by default OpenSSL installs itself in `/usr/local/ssl`. Installing
on a more conventional location, such as `/usr/local` or `/usr`, can break
many programs in your system. OpenSSL 1.0.2 is actually incompatible with
with OpenSSL 1.0.0 which is the version expected by the programs already
installed by Ubuntu 14.04.

In any case, as the library installs itself in this non-standard location, we
also need to configure CMake and other build program to find this version of
OpenSSL:

```bash
export OPENSSL_ROOT_DIR=/usr/local/ssl
export PKG_CONFIG_PATH=/usr/local/ssl/lib/pkgconfig
```

#### libcurl.

Because google-cloud-cpp uses both gRPC and curl, we need to compile libcurl
against the same version of OpenSSL:

```bash
cd $HOME/Downloads
wget -q https://curl.haxx.se/download/curl-7.61.0.tar.gz
tar xf curl-7.61.0.tar.gz
cd $HOME/Downloads/curl-7.61.0
./configure --prefix=/usr/local/curl
make -j ${NCPU:-4}
sudo make install
sudo ldconfig
```

#### crc32c

There is no Ubuntu Trusty package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd $HOME/Downloads/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c
sudo cmake --build cmake-out/crc32c --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### Protobuf

While protobuf-2.5 is distributed with Ubuntu:trusty, the Google Cloud Plaform
proto files require more recent versions (circa 3.4.x). To manually install a
more recent version use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.6.1.tar.gz
tar -xf v3.6.1.tar.gz
cd $HOME/Downloads/protobuf-3.6.1/cmake
cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.11, while Ubuntu-16.04
distributes c-ares-1.10. We need some additional development tools to compile
this library:

```bash
sudo apt update && \
sudo apt install -y automake libtool
```

After installing these tools we can manually install a newer version
of c-ares:

```bash
cd $HOME/Downloads
wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz
tar -xf cares-1_14_0.tar.gz
cd $HOME/Downloads/c-ares-cares-1_14_0
./buildconf && ./configure && make -j ${NCPU:-4}
sudo make install
```

#### gRPC

Ubuntu:trusty does not provide a package for gRPC. Manually install this
library:

```bash
export PKG_CONFIG_PATH=/usr/local/ssl/lib/pkgconfig:/usr/local/curl/lib/pkgconfig
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.19.1.tar.gz
tar -xf v1.19.1.tar.gz
cd $HOME/Downloads/grpc-1.19.1
make -j ${NCPU:-4}
sudo make install
```

#### googleapis

There is no Ubuntu package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.5.tar.gz
tar -xf v0.1.5.tar.gz
cd $HOME/Downloads/cpp-cmakefiles-0.1.5
cmake \
    -DBUILD_SHARED_LIBS=YES \
    -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googletest

We need a recent version of GoogleTest to compile the unit and integration
tests.

```bash
cd $HOME/Downloads
wget -q https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
tar -xf b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
cd $HOME/Downloads/googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb
cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### google-cloud-cpp

We can now compile and install `google-cloud-cpp`.

```bash
cd $HOME/google-cloud-cpp
cmake -H. -Bcmake-out \
    -DCMAKE_FIND_ROOT_PATH="/usr/local/curl;/usr/local/ssl"
cmake --build cmake-out -- -j ${NCPU:-4}
cd $HOME/google-cloud-cpp/cmake-out
ctest --output-on-failure
sudo cmake --build . --target install
```


### Debian (Stretch)

First install the development tools and libcurl.
On Debian Stretch, libcurl links against openssl-1.0.2, and one must link
against the same version or risk an inconsistent configuration of the library.
This is especially important for multi-threaded applications, as openssl-1.0.2
requires explicitly setting locking callbacks. Therefore, to use libcurl one
must link against openssl-1.0.2. To do so, we need to install libssl1.0-dev.
Note that this removes libssl-dev if you have it installed already, and would
prevent you from compiling against openssl-1.1.0.

```bash
sudo apt update && \
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl1.0-dev make \
        pkg-config tar wget zlib1g-dev
```

#### crc32c

There is no Debian package for this library. To install it use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd $HOME/Downloads/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c
sudo cmake --build cmake-out/crc32c --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### Protobuf

While protobuf-3.0 is distributed with Ubuntu, the Google Cloud Plaform proto
files require more recent versions (circa 3.4.x). To manually install a more
recent version use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.6.1.tar.gz
tar -xf v3.6.1.tar.gz
cd $HOME/Downloads/protobuf-3.6.1/cmake
cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### gRPC

Likewise, Ubuntu has packages for grpc-1.3.x, but this version is too old for
the Google Cloud Platform APIs:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.19.1.tar.gz
tar -xf v1.19.1.tar.gz
cd $HOME/Downloads/grpc-1.19.1
make -j ${NCPU:-4}
sudo make install
sudo ldconfig
```

#### googleapis

There is no Debian package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.5.tar.gz
tar -xf v0.1.5.tar.gz
cd $HOME/Downloads/cpp-cmakefiles-0.1.5
cmake \
    -DBUILD_SHARED_LIBS=YES \
    -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googletest

We need a recent version of GoogleTest to compile the unit and integration
tests.

```bash
cd $HOME/Downloads
wget -q https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
tar -xf b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
cd $HOME/Downloads/googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb
cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### google-cloud-cpp

Finally we can install `google-cloud-cpp`.

```bash
cd $HOME/google-cloud-cpp
cmake -H. -Bcmake-out
cmake --build cmake-out -- -j ${NCPU:-4}
cd $HOME/google-cloud-cpp/cmake-out
ctest --output-on-failure
sudo cmake --build . --target install
```


### CentOS (7)

First install the development tools and OpenSSL. The development tools
distributed with CentOS (notably CMake) are too old to build
`google-cloud-cpp`. In these instructions, we use `cmake3` obtained from
[Software Collections](https://www.softwarecollections.org/).

```bash
rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
sudo yum install -y centos-release-scl
sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
sudo yum makecache && \
sudo yum install -y automake cmake3 curl-devel gcc gcc-c++ git libtool \
        make openssl-devel pkgconfig tar wget which zlib-devel
ln -sf /usr/bin/cmake3 /usr/bin/cmake && ln -sf /usr/bin/ctest3 /usr/bin/ctest
```

#### crc32c

There is no CentOS package for this library. To install it use:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd $HOME/Downloads/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -Bcmake-out/crc32c
sudo cmake --build cmake-out/crc32c --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### Protobuf

Likewise, manually install protobuf:

```bash
cd $HOME/Downloads
wget -q https://github.com/google/protobuf/archive/v3.6.1.tar.gz
tar -xf v3.6.1.tar.gz
cd $HOME/Downloads/protobuf-3.6.1/cmake
cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### c-ares

Recent versions of gRPC require c-ares >= 1.11, while CentOS-7
distributes c-ares-1.10. Manually install a newer version:

```bash
cd $HOME/Downloads
wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz
tar -xf cares-1_14_0.tar.gz
cd $HOME/Downloads/c-ares-cares-1_14_0
./buildconf && ./configure && make -j ${NCPU:-4}
sudo make install
sudo ldconfig
```

#### gRPC

Can be manually installed using:

```bash
cd $HOME/Downloads
wget -q https://github.com/grpc/grpc/archive/v1.19.1.tar.gz
tar -xf v1.19.1.tar.gz
cd $HOME/Downloads/grpc-1.19.1
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
export PATH=/usr/local/bin:${PATH}
make -j ${NCPU:-4}
sudo make install
sudo ldconfig
```

#### googleapis

There is no CentOS package for this library. To install it, use:

```bash
cd $HOME/Downloads
wget -q https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.5.tar.gz
tar -xf v0.1.5.tar.gz
cd $HOME/Downloads/cpp-cmakefiles-0.1.5
cmake \
    -DBUILD_SHARED_LIBS=YES \
    -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### googletest

We need a recent version of GoogleTest to compile the unit and integration
tests.

```bash
cd $HOME/Downloads
wget -q https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
tar -xf b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz
cd $HOME/Downloads/googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb
cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -H. -Bcmake-out
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
sudo ldconfig
```

#### google-cloud-cpp

Finally we can install `google-cloud-cpp`.

```bash
cd $HOME/Downloads/google-cloud-cpp
cmake -H. -Bcmake-out
cmake --build cmake-out -- -j ${NCPU:-4}
cd $HOME/Downloads/google-cloud-cpp/cmake-out
ctest --output-on-failure
sudo cmake --build . --target install
```

