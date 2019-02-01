# Installing google-cloud-cpp

By default google-cloud-cpp libraries download and compile all their
dependencies. This makes it easier for users to "take the library for a spin",
and works well for users that "Live at Head", but does not work for package
maintainers or users that prefer to compile their dependencies once and install
them in `/usr/local/` or a similar directory.

This document provides instructions to install the dependencies of
google-cloud-cpp.

## Installing google-cloud-cpp

*If*<br>
-- The Spartan ephors to Philip of Macedon.<br><br>

**If** all the dependencies of `google-cloud-cpp` are installed and provide
CMake support files, then compiling and installing the libraries
requires two commands:

```bash
cmake -H. -Bbuild-output-for-install \
    -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package
cmake --build build-output-for-install --target install
```

Unfortunately getting your system to this state may require multiple steps,
the following sections describe how to install `google-cloud-cpp` on several
platforms.

## Table of Contents

- [Installing google-cloud-cpp](#installing-google-cloud-cpp)
  - [Ubuntu (Bionic Beaver)](#ubuntu-bionic-beaver)
  - [CentOS 7](#ubuntu-bionic-beaver)

## Installing google-cloud-cpp

#### Required Libraries

`google-cloud-cpp` directly depends on the following libraries:

| Library | Minimum version | Description |
| ------- | --------------: | ----------- |
| gRPC    | 1.17.x | gRPC++ for Cloud Bigtable |
| libcurl | 7.47.0  | HTTP client library for the Google Cloud Storage client |
| crc32c  | 1.06 | Hardware-accelerated CRC32C implementation |
| OpenSSL | 1.0.2 | Crypto functions for Google Cloud Storage authentication |

Note that these libraries may have complex dependencies themselves, the
following instructions include steps to install these dependencies too.

#### Ubuntu (Bionic Beaver)

First install the development tools, libcurl, and OpenSSL.

```bash
sudo apt update
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl-dev make \
        pkg-config tar wget zlib1g-dev
```

**crc32c**: there is no Ubuntu package for this library, install it using:

```bash
cd /var/tmp/build
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd build/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -B.build/crc32c
sudo cmake --build .build/crc32c --target install -- -j $(nproc)
sudo ldconfig
```

**Protobuf**: while protobuf-3.0.0 is distributed with Ubuntu, the Google Cloud
Plaform proto files require more recent versions (circa 3.4.x). To manually
install a more recent version use:

```bash
cd /var/tmp/build
wget -q https://github.com/google/protobuf/archive/v3.6.1.tar.gz
tar -xf v3.6.1.tar.gz
cd protobuf-3.6.1/cmake
cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -B.build
sudo cmake --build .build --target install -- -j $(nproc)
sudo ldconfig
```

**gRPC**: likewise, Ubuntu has packages for grpc-1.3.x, but this version is too
old for the Google Cloud Platform APIs:

```bash
cd /var/tmp/build
wget -q https://github.com/grpc/grpc/archive/v1.17.2.tar.gz
tar -xf v1.17.2.tar.gz
cd /var/tmp/build/grpc-1.17.2
cd make -j $(nproc)
sudo make install
ldconfig
```

**google-cloud-cpp**: finally we can install `google-cloud-cpp`, note that
we use `pkg-config` to discover the options for gRPC:

```bash
cd google-cloud-cpp
cmake -H. -Bbuild-output \
    -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package \
    -DGOOGLE_CLOUD_CPP_GRPC_PROVIDER=pkg-config \
    -DGOOGLE_CLOUD_CPP_GMOCK_PROVIDER=external
cmake --build build-output -- -j $(nproc)
cd build-output
ctest --output-on-failure
cmake --build . --target install
```

If you would rather not build the tests (which might speed up your build time):

```bash
cd google-cloud-cpp
cmake -H. -Bbuild-output-for-install \
    -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package \
    -DGOOGLE_CLOUD_CPP_GRPC_PROVIDER=pkg-config \
    -DBUILD_TESTS=OFF
cmake --build build-output-for-install -- -j $(nproc)
cmake --build build-output-for-install --target install
```

#### CentOS

First install the development tools an OpenSSL. The development tools
distributed with CentOS (notably CMake) are too old to build `google-cloud-cpp`
and its dependencies. In these instructions, we use `cmake3` via
[Software Collections](https://www.softwarecollections.org/).

```bash
# Extra Packages for Enterprise Linux used to install cmake3
rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm

yum install centos-release-scl
yum-config-manager --enable rhel-server-rhscl-7-rpms

yum makecache
yum install -y cmake3 gcc gcc-c++ git make openssl-devel

# Install cmake3 & ctest3 as cmake & ctest respectively.
ln -sf /usr/bin/cmake3 /usr/bin/cmake && ln -sf /usr/bin/ctest3 /usr/bin/ctest
```

**crc32c**: there is no CentOS package for this library, install it using:

```bash
cd /var/tmp/build
wget -q https://github.com/google/crc32c/archive/1.0.6.tar.gz
tar -xf 1.0.6.tar.gz
cd /var/tmp/build/crc32c-1.0.6
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -H. -B.build/crc32c
cmake --build .build/crc32c --target install -- -j $(nproc)
ldconfig
```

**Protobuf**: likewise, manually install protobuf:

```bash
cd /var/tmp/build
wget -q https://github.com/google/protobuf/archive/v3.6.1.tar.gz
tar -xf v3.6.1.tar.gz
cd /var/tmp/build/protobuf-3.6.1/cmake
cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -H. -B.build
cmake --build .build --target install -- -j $(nproc)
ldconfig
```

**c-ares**: recent versions of gRPC require c-ares >= 1.11, but CentOS-7
distributes c-ares-1.10. Manually install a newer version:

```bash
cd /var/tmp/build
wget -q https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz
tar -xf cares-1_14_0.tar.gz
cd /var/tmp/build/c-ares-cares-1_14_0
./buildconf && ./configure && make -j $(nproc) && make install
ldconfig
```

**gRPC**: can manually installed using:

```bash
cd /var/tmp/build
wget -q https://github.com/grpc/grpc/archive/v1.17.2.tar.gz
tar -xf v1.17.2.tar.gz
cd /var/tmp/build/grpc-1.17.2
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
export PATH=/usr/local/bin:${PATH}
RUN make -j $(nproc) && make install
RUN ldconfig
```

**google-cloud-cpp**: finally we can install `google-cloud-cpp`, note that
we use `pkg-config` to discover the options for gRPC:

```bash
cd google-cloud-cpp
cmake -H. -Bbuild-output \
    -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package \
    -DGOOGLE_CLOUD_CPP_GRPC_PROVIDER=pkg-config \
    -DGOOGLE_CLOUD_CPP_GMOCK_PROVIDER=external
cmake --build build-output -- -j $(nproc)
cd build-output
ctest --output-on-failure
cmake --build . --target install
```
