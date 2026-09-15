
<details>
<summary>Alpine (Stable)</summary>
<br>

Install the minimal development tools, libcurl, and OpenSSL:

```bash
apk update && \
    apk add bash ca-certificates cmake curl git \
        gcc g++ make tar unzip zip zlib-dev
```

Alpine's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
when handling `.pc` files with lots of `Requires:` deps, which happens with
Abseil, so we use the normal `pkg-config` binary, which seems to not suffer
from this bottleneck. For more details see
https://github.com/pkgconf/pkgconf/issues/229 and
https://github.com/googleapis/google-cloud-cpp/issues/7052

```bash
mkdir -p $HOME/Downloads/pkgconf && cd $HOME/Downloads/pkgconf
curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr && \
    make -j ${NCPU:-4} && \
sudo make install && \
    cd /var/tmp && rm -fr build
```

The following steps will install libraries and tools in `/usr/local`. By
default, pkgconf does not search in these directories. We need to explicitly
set the search path.

```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
```

#### Dependencies

The versions of Abseil, Protobuf, gRPC, OpenSSL, and nlohmann-json included
with Alpine >= 3.19 meet `google-cloud-cpp`'s requirements. We can simply
install the development packages

```bash
apk update && \
    apk add abseil-cpp-dev c-ares-dev curl-dev grpc-dev \
        protobuf-dev nlohmann-json openssl-dev re2-dev
```

#### opentelemetry-cpp

```bash
mkdir -p $HOME/Downloads/opentelemetry-cpp && cd $HOME/Downloads/opentelemetry-cpp
curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.28.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_STL=CXX17 \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4}
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`:

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -S . -B cmake-out \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_WITH_MOCKS=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND="${DEMO_CORD_WORKAROUND:-OFF}" \
  -DGOOGLE_CLOUD_CPP_ENABLE=__ga_libraries__,opentelemetry
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Fedora (40)</summary>
<br>

Install the minimal development tools:

```bash
sudo dnf makecache && \
sudo dnf install -y cmake curl findutils gcc-c++ git make ninja-build \
        patch unzip tar wget zip dnf-utils zlib-devel
sudo dnf makecache && sudo dnf debuginfo-install -y glibc
```

Fedora:40 includes packages, with recent enough versions, for most of the
direct dependencies of `google-cloud-cpp`.

```bash
sudo dnf makecache && \
sudo dnf install -y json-devel libnghttp2-devel openssl-devel
```

#### Patching pkg-config

If you are not planning to use `pkg-config(1)` you can skip these steps.

Fedora's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
when handling `.pc` files with lots of `Requires:` deps, which happens with
Abseil. If you plan to use `pkg-config` with any of the installed artifacts,
you may want to use a recent version of the standard `pkg-config` binary. If
not, `sudo dnf install pkgconfig` should work.

```bash
mkdir -p $HOME/Downloads/pkgconf && cd $HOME/Downloads/pkgconf
curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr --with-system-libdir=/lib64:/usr/lib64 --with-system-includedir=/usr/include && \
    make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig && cd /var/tmp && rm -fr build
```

The following steps will install libraries and tools in `/usr/local`. By
default, pkgconf does not search in these directories. We need to explicitly
set the search path.

```bash
export PKG_CONFIG_PATH=/usr/local/share/pkgconfig:/usr/lib64/pkgconfig:/usr/local/lib64/pkgconfig
```

#### curl
#
Install curl from source to avoid a libcurl bug present in older versions.
See https://github.com/googleapis/google-cloud-cpp/issues/16343 for more
details.
#
```bash
mkdir -p $HOME/Downloads/curl && cd $HOME/Downloads/curl
curl -fsSL https://github.com/curl/curl/releases/download/curl-8_7_1/curl-8.7.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DCURL_USE_OPENSSL=ON \
        -DBUILD_CURL_EXE=OFF \
        -DBUILD_TESTING=OFF \
        -GNinja -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig && cd /var/tmp && rm -fr build
```

#### Protobuf

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v36.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
      -GNinja -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig && cd /var/tmp && rm -fr build
```

#### opentelemetry-cpp

```bash
mkdir -p $HOME/Downloads/opentelemetry-cpp && cd $HOME/Downloads/opentelemetry-cpp
curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.28.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_STL=CXX17 \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
sudo dnf makecache && sudo dnf install -y c-ares-devel re2-devel
curl -fsSL https://github.com/grpc/grpc/archive/v1.71.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_STANDARD=17 \
      -DBUILD_SHARED_LIBS=ON \
      -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_ABSL_PROVIDER=package \
      -DgRPC_CARES_PROVIDER=package \
      -DgRPC_PROTOBUF_PROVIDER=package \
      -DgRPC_PROTOBUF_PACKAGE_TYPE=CONFIG \
      -DgRPC_RE2_PROVIDER=package \
      -DgRPC_SSL_PROVIDER=package \
      -DgRPC_ZLIB_PROVIDER=package \
      -DgRPC_OPENTELEMETRY_PROVIDER=package \
      -DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=ON \
      -GNinja -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig && cd /var/tmp && rm -fr build
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`:

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -S . -B cmake-out \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_WITH_MOCKS=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND="${DEMO_CORD_WORKAROUND:-OFF}" \
  -DGOOGLE_CLOUD_CPP_ENABLE=__ga_libraries__,opentelemetry
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>openSUSE (Leap)</summary>
<br>

Install the minimal development tools.

**NOTE:** The default compiler on openSUSE (GCC 7.5.0) crashes while compiling
some of the files generated by Protobuf. Minor variations in the Protobuf
version or the libraries changes where the compiler crashes. We recommend you
use GCC 8 or higher to compile `google-cloud-cpp`.

```bash
sudo zypper refresh && \
sudo zypper install --allow-downgrade -y automake cmake curl \
        gcc gcc-c++ git gzip libtool make patch tar wget
```

Install some of the dependencies for `google-cloud-cpp`.

```bash
sudo zypper refresh && \
sudo zypper install --allow-downgrade -y \
        libcurl-devel libopenssl-devel nlohmann_json-devel
```

The following steps will install libraries and tools in `/usr/local`. openSUSE
does not search for shared libraries in these directories by default. There
are multiple ways to solve this problem, the following steps are one solution:

```bash
(echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
sudo tee /etc/ld.so.conf.d/usrlocal.conf
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
export PATH=/usr/local/bin:${PATH}
```

Use the following environment variables to configure the compiler used by
CMake.
export CC=gcc
export CXX=g++

#### Abseil

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -fsSL https://github.com/abseil/abseil-cpp/archive/20250814.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_STANDARD=17 \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v36.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig && \
    ln -s /usr/local/bin/protoc /usr/bin/protoc
```

#### opentelemetry-cpp

```bash
mkdir -p $HOME/Downloads/opentelemetry-cpp && cd $HOME/Downloads/opentelemetry-cpp
curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.28.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_STL=CXX17 \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

```bash
mkdir -p $HOME/Downloads/c-ares && cd $HOME/Downloads/c-ares
curl -fsSL https://github.com/c-ares/c-ares/archive/refs/tags/cares-1_17_1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=yes \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig
```

#### RE2

```bash
mkdir -p $HOME/Downloads/re2 && cd $HOME/Downloads/re2
curl -fsSL https://github.com/google/re2/archive/2025-11-05.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig
```

#### gRPC

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -fsSL https://github.com/grpc/grpc/archive/v1.71.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -DgRPC_OPENTELEMETRY_PROVIDER=package \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`:

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -S . -B cmake-out \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_WITH_MOCKS=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND="${DEMO_CORD_WORKAROUND:-OFF}" \
  -DGOOGLE_CLOUD_CPP_ENABLE=__ga_libraries__,opentelemetry
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Ubuntu (24.04 LTS - Noble Numbat)</summary>
<br>

Install the minimal development tools, OpenSSL and libc-ares:

```bash
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        cmake ca-certificates curl git gcc g++ m4 make tar libc6-dbg
```

Ubuntu:24 includes packages for most of the direct dependencies of
`google-cloud-cpp`:

```bash
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y  \
        libssl-dev libnghttp2-dev nlohmann-json3-dev zlib1g-dev \
        libsystemd-dev
```

#### Patching pkg-config

If you are not planning to use `pkg-config(1)` you can skip these steps.

Ubuntu's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
when handling `.pc` files with lots of `Requires:` deps, which happens with
Abseil. If you plan to use `pkg-config` with any of the installed artifacts,
you may want to use a recent version of the standard `pkg-config` binary. If
not, `sudo dnf install pkgconfig` should work.

```bash
mkdir -p $HOME/Downloads/pkgconf && cd $HOME/Downloads/pkgconf
rm -f /usr/bin/pkgconf /usr/bin/pkg-config
curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr -with-pkg-config-dir=/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig && \
    make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig && cd /var/tmp && rm -fr build
ln -s /usr/bin/pkgconf /usr/bin/pkg-config
```

#### curl
#
Install curl from source to avoid a libcurl bug present in older versions.
See https://github.com/googleapis/google-cloud-cpp/issues/16343 for more
details.
#
```bash
mkdir -p $HOME/Downloads/curl && cd $HOME/Downloads/curl
curl -fsSL https://github.com/curl/curl/releases/download/curl-8_7_1/curl-8.7.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DCURL_USE_OPENSSL=ON \
        -DBUILD_CURL_EXE=OFF \
        -DBUILD_TESTING=OFF \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig && cd /var/tmp && rm -fr build
```

#### Abseil

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -fsSL https://github.com/abseil/abseil-cpp/archive/20250814.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v36.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### opentelemetry-cpp

```bash
mkdir -p $HOME/Downloads/opentelemetry-cpp && cd $HOME/Downloads/opentelemetry-cpp
curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.28.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_STL=CXX17 \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

```bash
mkdir -p $HOME/Downloads/c-ares && cd $HOME/Downloads/c-ares
curl -fsSL https://github.com/c-ares/c-ares/archive/refs/tags/cares-1_17_1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=yes \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig
```

#### RE2

```bash
mkdir -p $HOME/Downloads/re2 && cd $HOME/Downloads/re2
curl -fsSL https://github.com/google/re2/archive/2025-11-05.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig
```

#### gRPC

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -fsSL https://github.com/grpc/grpc/archive/v1.71.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -DgRPC_OPENTELEMETRY_PROVIDER=package \
        -DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=ON \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`:

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -S . -B cmake-out \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_WITH_MOCKS=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND="${DEMO_CORD_WORKAROUND:-OFF}" \
  -DGOOGLE_CLOUD_CPP_ENABLE=__ga_libraries__,opentelemetry
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Ubuntu (22.04 LTS - Jammy Jellyfish)</summary>
<br>

Install the minimal development tools, OpenSSL and libc-ares:

```bash
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential cmake ca-certificates curl git \
        gcc g++ libc-ares-dev libc-ares2 libnghttp2-dev libre2-dev \
        libssl-dev m4 make pkg-config tar wget zlib1g-dev libc6-dbg
```

#### curl
#
Install curl from source to avoid a libcurl bug present in older versions.
See https://github.com/googleapis/google-cloud-cpp/issues/16343 for more
details.
#
```bash
mkdir -p $HOME/Downloads/curl && cd $HOME/Downloads/curl
curl -fsSL https://github.com/curl/curl/releases/download/curl-8_7_1/curl-8.7.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DCURL_USE_OPENSSL=ON \
        -DBUILD_CURL_EXE=OFF \
        -DBUILD_TESTING=OFF \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig && cd /var/tmp && rm -fr build
```

#### Abseil

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -fsSL https://github.com/abseil/abseil-cpp/archive/20250814.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_STANDARD=17 \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

We need to install a version of Protobuf that is recent enough to support the
Google Cloud Platform proto files:

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v36.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. We install it using:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -fsSL https://github.com/grpc/grpc/archive/v1.71.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -S . -B cmake-out && \
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
curl -fsSL https://github.com/nlohmann/json/archive/v3.11.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### opentelemetry-cpp

```bash
mkdir -p $HOME/Downloads/opentelemetry-cpp && cd $HOME/Downloads/opentelemetry-cpp
curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.28.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_STL=CXX17 \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`:

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -S . -B cmake-out \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_WITH_MOCKS=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND="${DEMO_CORD_WORKAROUND:-OFF}" \
  -DGOOGLE_CLOUD_CPP_ENABLE=__ga_libraries__,opentelemetry
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Debian (12 - Bookworm)</summary>
<br>

Install the minimal development tools.

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ca-certificates cmake curl git \
        gcc g++ m4 make ninja-build pkg-config tar wget zlib1g-dev libc6-dbg
```

Install the development packages for direct `google-cloud-cpp` dependencies:

```bash
sudo apt-get update && \
sudo apt-get --no-install-recommends install -y \
        libnghttp2-dev libssl-dev nlohmann-json3-dev
```

#### Patching pkg-config

If you are not planning to use `pkg-config(1)` you can skip these steps.

Debian's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is slow
when handling `.pc` files with lots of `Requires:` deps, which happens with
Abseil. If you plan to use `pkg-config` with any of the installed artifacts,
you may want to use a recent version of the standard `pkg-config` binary. If
not, `sudo dnf install pkgconfig` should work.

```bash
mkdir -p $HOME/Downloads/pkgconf && cd $HOME/Downloads/pkgconf
curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr --with-system-libdir=/lib:/usr/lib --with-system-includedir=/usr/include && \
    make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig && cd /var/tmp && rm -fr build
export PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig
```

#### curl
#
Install curl from source to avoid a libcurl bug present in older versions.
See https://github.com/googleapis/google-cloud-cpp/issues/16343 for more
details.
#
```bash
mkdir -p $HOME/Downloads/curl && cd $HOME/Downloads/curl
curl -fsSL https://github.com/curl/curl/releases/download/curl-8_7_1/curl-8.7.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DCURL_USE_OPENSSL=ON \
        -DBUILD_CURL_EXE=OFF \
        -DBUILD_TESTING=OFF \
        -GNinja -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig && cd /var/tmp && rm -fr build
```

#### Abseil

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -fsSL https://github.com/abseil/abseil-cpp/archive/20250814.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_STANDARD=17 \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v36.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig && \
    ln -s /usr/local/bin/protoc /usr/bin/protoc
```

#### opentelemetry-cpp

```bash
mkdir -p $HOME/Downloads/opentelemetry-cpp && cd $HOME/Downloads/opentelemetry-cpp
curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.28.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_STL=CXX17 \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### c-ares

```bash
mkdir -p $HOME/Downloads/c-ares && cd $HOME/Downloads/c-ares
curl -fsSL https://github.com/c-ares/c-ares/archive/refs/tags/cares-1_17_1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=yes \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig
```

#### RE2

```bash
mkdir -p $HOME/Downloads/re2 && cd $HOME/Downloads/re2
curl -fsSL https://github.com/google/re2/archive/2025-11-05.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig
```

#### gRPC

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -fsSL https://github.com/grpc/grpc/archive/v1.71.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -DgRPC_OPENTELEMETRY_PROVIDER=package \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`:

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -S . -B cmake-out \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_WITH_MOCKS=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND="${DEMO_CORD_WORKAROUND:-OFF}" \
  -DGOOGLE_CLOUD_CPP_ENABLE=__ga_libraries__,opentelemetry
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

<details>
<summary>Rocky Linux (9)</summary>
<br>

Install the minimal development tools, OpenSSL, and the c-ares
library (required by gRPC):

```bash
sudo dnf makecache && \
sudo dnf update -y && \
sudo dnf install -y epel-release && \
sudo dnf config-manager --set-enabled crb && \
sudo dnf makecache && \
sudo dnf install -y cmake findutils gcc-c++ git make openssl-devel \
        patch zlib-devel libnghttp2-devel c-ares-devel tar wget which \
        autoconf automake libtool binutils dnf-utils
sudo dnf makecache && sudo dnf debuginfo-install -y glibc

```

Set some useful environment variables.

```bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
```

Rocky Linux's version of `pkg-config` (https://github.com/pkgconf/pkgconf) is
slow when handling `.pc` files with lots of `Requires:` deps, which happens
with Abseil. If you plan to use `pkg-config` with any of the installed
artifacts, you may want to use a recent version of the standard `pkg-config`
binary. If not, `sudo dnf install pkgconfig` should work.

```bash
mkdir -p $HOME/Downloads/pkgconf && cd $HOME/Downloads/pkgconf
curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr --with-system-libdir=/lib64:/usr/lib64 --with-system-includedir=/usr/include && \
    make -j ${NCPU:-4} && \
sudo make install && \
sudo ldconfig && cd /var/tmp && rm -fr build
```

The following steps will install libraries and tools in `/usr/local`. By
default, Rocky Linux 9 does not search for shared libraries in these
directories, there are multiple ways to solve this problem, the following
steps are one solution:

```bash
(echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
sudo tee /etc/ld.so.conf.d/usrlocal.conf
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib64/pkgconfig
export PATH=/usr/local/bin:${PATH}
```

#### curl
#
Install curl from source to avoid a libcurl bug present in older versions.
See https://github.com/googleapis/google-cloud-cpp/issues/16343 for more
details.
#
```bash
mkdir -p $HOME/Downloads/curl && cd $HOME/Downloads/curl
curl -fsSL https://github.com/curl/curl/releases/download/curl-8_7_1/curl-8.7.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=ON \
        -DCURL_USE_OPENSSL=ON \
        -DBUILD_CURL_EXE=OFF \
        -DBUILD_TESTING=OFF \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install && \
sudo ldconfig && cd /var/tmp && rm -fr build
```

#### Abseil

```bash
mkdir -p $HOME/Downloads/abseil-cpp && cd $HOME/Downloads/abseil-cpp
curl -fsSL https://github.com/abseil/abseil-cpp/archive/20250814.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=yes \
      -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Protobuf

Rocky Linux ships with Protobuf 3.14.x.  Some of the libraries in
`google-cloud-cpp` require Protobuf >= 3.15.8.  For simplicity, we will just
install Protobuf (and any downstream packages) from source.

```bash
mkdir -p $HOME/Downloads/protobuf && cd $HOME/Downloads/protobuf
curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v36.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### RE2

The version of RE2 included with this distro hard-codes C++11 in its
pkg-config file. You can skip this build and use the system's package if
you are not planning to use pkg-config.

```bash
mkdir -p $HOME/Downloads/re2 && cd $HOME/Downloads/re2
curl -fsSL https://github.com/google/re2/archive/2025-11-05.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### gRPC

We also need a version of gRPC that is recent enough to support the Google
Cloud Platform proto files. Note that gRPC overrides the default C++ standard
version to C++14, we need to configure it to use the platform's default. We
manually install it using:

```bash
mkdir -p $HOME/Downloads/grpc && cd $HOME/Downloads/grpc
curl -fsSL https://github.com/grpc/grpc/archive/v1.71.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=yes \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -S . -B cmake-out && \
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
curl -fsSL https://github.com/nlohmann/json/archive/v3.11.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### opentelemetry-cpp

```bash
mkdir -p $HOME/Downloads/opentelemetry-cpp && cd $HOME/Downloads/opentelemetry-cpp
curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.28.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=17 \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_STL=CXX17 \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
sudo cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
sudo ldconfig
```

#### Compile and install the main project

We can now compile and install `google-cloud-cpp`:

```bash
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -S . -B cmake-out \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_WITH_MOCKS=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND="${DEMO_CORD_WORKAROUND:-OFF}" \
  -DGOOGLE_CLOUD_CPP_ENABLE=__ga_libraries__,opentelemetry
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
```

</details>

