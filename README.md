# Google Cloud Platform C++ Client Libraries

C++ Idiomatic Clients for [Google Cloud Platform][cloud-platform] services.

[![Travis CI status][travis-shield]][travis-link]
[![Kokoro CI status][kokoro-ubuntu-shield]][kokoro-ubuntu-link]
[![Kokoro CI status][kokoro-windows-shield]][kokoro-windows-link]
[![Kokoro CI status][kokoro-macos-shield]][kokoro-macos-link]
[![Kokoro CI status][kokoro-asan-shield]][kokoro-asan-link]
[![Kokoro CI status][kokoro-ubsan-shield]][kokoro-asan-link]
[![Kokoro CI status][kokoro-noex-shield]][kokoro-noex-link]
[![Kokoro CI status][kokoro-centos-7-shield]][kokoro-centos-7-link]
[![Codecov Coverage status][codecov-shield]][codecov-link]

- [Google Cloud Platform Documentation][cloud-platform-docs]
- [Client Library Documentation][client-library-docs]

[travis-shield]: https://img.shields.io/travis/googleapis/google-cloud-cpp/master.svg?label=travis
[travis-link]: https://travis-ci.com/googleapis/google-cloud-cpp/builds
[kokoro-ubuntu-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-ubuntu.svg
[kokoro-ubuntu-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-ubuntu-link.html
[kokoro-windows-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-windows.svg
[kokoro-windows-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-windows-link.html
[kokoro-macos-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-macos.svg
[kokoro-macos-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-macos-link.html
[kokoro-asan-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-asan.svg
[kokoro-asan-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-asan-link.html
[kokoro-ubsan-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-ubsan.svg
[kokoro-ubsan-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-ubsan-link.html
[kokoro-noex-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-noex.svg
[kokoro-noex-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-noex-link.html
[kokoro-centos-7-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-centos-7.svg
[kokoro-centos-7-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-centos-7-link.html
[codecov-shield]: https://codecov.io/gh/googleapis/google-cloud-cpp/branch/master/graph/badge.svg
[codecov-link]: https://codecov.io/gh/googleapis/google-cloud-cpp
[cloud-platform]: https://cloud.google.com/
[cloud-platform-docs]: https://cloud.google.com/docs/
[client-library-docs]: http://googleapis.github.io/google-cloud-cpp/

This library supports the following Google Cloud Platform services with clients
at the [Beta](#versioning) quality level:

- [Google Cloud Bigtable](google/cloud/bigtable/README.md#readme)
- [Google Cloud Storage](google/cloud/storage/README.md#readme)

## Table of Contents

- [Requirements](#requirements)
  - [Compiler](#compiler)
  - [Build Tools](#build-tools)
  - [Libraries](#libraries)
  - [Tests](#tests)
- [Install Dependencies](#install-dependencies)
  - [CentOS (7)](#centos-7)
  - [Debian (Stretch)](#debian-stretch)
  - [Fedora (29)](#fedora-29)
  - [OpenSuSE (Tumbleweed)](#opensuse-tumbleweed)
  - [OpenSuSE (Leap)](#opensuse-leap)
  - [Ubuntu (18.04 - Bionic Beaver)](#ubuntu-1804---bionic-beaver)
  - [Ubuntu (16.04 - Xenial Xerus)](#ubuntu-1604---xenial-xerus)
  - [Ubuntu (14.04 - Trusty Tahr)](#ubuntu-1404---trusty-tahr)
  - [macOS (using brew)](#macos-using-brew)
  - [Windows](#windows-using-vcpkg)
- [Build](#build)
  - [Linux](#linux)
  - [macOS](#macOS)
  - [Windows](#windows)
- [Install](#install)

## Requirements

#### Compiler

The Google Cloud C++ libraries are tested with the following compilers:

| Compiler    | Minimum Version |
| ----------- | --------------- |
| GCC         | 4.8 |
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
| Bazel      | 0.20.0 |

#### Libraries

The libraries also depend on gRPC, libcurl, and the dependencies of those
libraries. The Google Cloud C++ Client libraries are tested with the following
versions of these dependencies:

| Library | Minimum version |
| ------- | --------------- |
| gRPC    | v1.17.x |
| libcurl | 7.47.0  |

#### Tests

Integration tests at times use the
[Google Cloud SDK](https://cloud.google.com/sdk/). The integration tests run
against the latest version of the SDK on each commit and PR.

## Install Dependencies

### CentOS (7)

[![Kokoro install centos status][kokoro-install-centos-shield]][kokoro-install-centos-link]

[kokoro-install-centos-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-centos.svg
[kokoro-install-centos-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-centos-link.html

The development tools distributed with CentOS (notably CMake) are too old to
build `google-cloud-cpp`. In these instructions, we use `cmake3` obtained from
[Software Collections](https://www.softwarecollections.org/).

```bash
rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
sudo yum install -y centos-release-scl
sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
sudo yum makecache && \
sudo yum install -y automake cmake3 curl-devel gcc gcc-c++ git libtool make \
        openssl-devel pkgconfig tar wget which zlib-devel
ln -sf /usr/bin/cmake3 /usr/bin/cmake && ln -sf /usr/bin/ctest3 /usr/bin/ctest
```

### Debian (Stretch)

[![Kokoro install debian status][kokoro-install-debian-shield]][kokoro-install-debian-link]

[kokoro-install-debian-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-debian.svg
[kokoro-install-debian-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-debian-link.html

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

### Fedora (29)

[![Kokoro install fedora status][kokoro-install-fedora-shield]][kokoro-install-fedora-link]

[kokoro-install-fedora-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-fedora.svg
[kokoro-install-fedora-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-fedora-link.html

```bash
sudo dnf makecache && \
sudo dnf install -y cmake gcc-c++ git make openssl-devel pkgconfig zlib-devel
```

### OpenSUSE (Tumbleweed)

[![Kokoro install opensuse status][kokoro-install-opensuse-shield]][kokoro-install-opensuse-link]

[kokoro-install-opensuse-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-opensuse.svg
[kokoro-install-opensuse-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-opensuse-link.html

```bash
sudo zypper refresh && \
sudo zypper install -y cmake gcc gcc-c++ git libcurl-devel libopenssl-devel make
```

### OpenSUSE (Leap)

[![Kokoro install opensuse-leap status][kokoro-install-opensuse-leap-shield]][kokoro-install-opensuse-leap-link]

[kokoro-install-opensuse-leap-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-opensuse-leap.svg
[kokoro-install-opensuse-leap-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-opensuse-leap-link.html

```bash
sudo zypper refresh && \
sudo zypper install -y cmake gcc gcc-c++ git gzip libcurl-devel \
        libopenssl-devel make tar wget
```

### Ubuntu (18.04 - Bionic Beaver)

[![Kokoro install ubuntu status][kokoro-install-ubuntu-shield]][kokoro-install-ubuntu-link]

[kokoro-install-ubuntu-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-ubuntu.svg
[kokoro-install-ubuntu-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-ubuntu-link.html

```bash
sudo apt update && \
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl-dev make \
        pkg-config tar wget zlib1g-dev
```

### Ubuntu (16.04 - Xenial Xerus)

[![Kokoro install ubuntu-xenial status][kokoro-install-ubuntu-xenial-shield]][kokoro-install-ubuntu-xenial-link]

[kokoro-install-ubuntu-xenial-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-ubuntu-xenial.svg
[kokoro-install-ubuntu-xenial-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-ubuntu-xenial-link.html

```bash
sudo apt update && \
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libcurl4-openssl-dev libssl-dev make \
        pkg-config tar wget zlib1g-dev
```

### Ubuntu (14.04 - Trusty Tahr)

[![Kokoro install ubuntu-trusty status][kokoro-install-ubuntu-trusty-shield]][kokoro-install-ubuntu-trusty-link]

[kokoro-install-ubuntu-trusty-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-ubuntu-trusty.svg
[kokoro-install-ubuntu-trusty-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-ubuntu-trusty-link.html

We use the `ubuntu-toolchain-r` PPA to get a modern version of CMake:

```bash
sudo apt update && sudo apt install -y software-properties-common
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
sudo apt update && \
sudo apt install -y cmake3 git gcc g++ make pkg-config tar wget zlib1g-dev
```

Ubuntu:14.04 ships with a very old version of OpenSSL, this version is not
supported by gRPC. We need to compile and install OpenSSL-1.0.2 from source.

```bash
cd $HOME/Downloads
wget -q https://www.openssl.org/source/openssl-1.0.2n.tar.gz
tar xf openssl-1.0.2n.tar.gz
cd $HOME/Downloads/openssl-1.0.2n
./config --shared
make -j $(nproc)
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

#### macOS (using brew)

```bash
brew install curl cmake libressl c-ares
```

#### Windows (using vcpkg)

If you are already using [vcpkg][vcpkg-link], a package manager from Microsoft,
you can download and compile `google-cloud-cpp` in a single step:

```bash
.\vcpkg.exe install google-cloud-cpp:x64-windows-static
```

This command will also print out instructions on how to use the library from
your MSBuild or CMake-based projects. We try to keep the version of
`google-cloud-cpp` included with `vcpkg` up-to-date, our practice is to submit a
PR to update the version in `vcpkg` after each release of `google-cloud-cpp`.

See below for instructions to compile the code yourself.

[vcpkg-link]: https://github.com/Microsoft/vcpkg/

## Build

To build all available libraries and run the tests, run the following commands
after cloning this repo:

#### Linux

```bash
git submodule update --init
# Add -DBUILD_TESTING=OFF to disable tests
cmake -H. -Bbuild-output

# Adjust the number of threads used by modifying parameter for `-j 4`
cmake --build build-output -- -j 4

# Verify build by running tests
(cd build-output && ctest --output-on-failure)
```

You will find compiled binaries in `build-output/` respective to their source paths.

#### macOS

```bash
git submodule update --init
export OPENSSL_ROOT_DIR=/usr/local/opt/libressl
# Add -DBUILD_TESTING=OFF to disable tests
cmake -H. -Bbuild-output

# Adjust the number of threads used by modifying parameter for `-j 4`
cmake --build build-output -- -j 4

# Verify build by running tests
(cd build-output && ctest --output-on-failure)
```

You will find compiled binaries in `build-output/` respective to their source paths.

#### Windows

<!-- Last updated 2018-01-09 -->

If you prefer to manually compile on Windows the following instructions should
work, though there is a lot more variability on this platform. We welcome
suggestions to make this an easier process.

We will assume that you have installed CMake, Ninja, and "Microsoft Visual
Studio 2017". If you have not, install [Chocolatey](https://www.chocolatey.com)
using this command as the administrator:

```console
@"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command "iex (
(New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
```

Then you can easily install the necessary development tools.

```console
choco install -y cmake cmake.portable ninja visualstudio2017community
choco install -y visualstudio2017-workload-nativedesktop
choco install -y microsoft-build-tools
```

Then clone and compile `vcpkg`:

```console
set SOURCE="%cd%"
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

Use `vcpkg` to download and install `google-cloud-cpp`'s dependencies:

```console
.\vcpkg.exe install openssl:x64-windows-static ^
    grpc:x64-windows-static ^ 
    curl:x64-windows-static ^
    gtest:x64-windows-static ^
    crc32c:x64-windows-static
.\vcpkg.exe integrate install
```

Now clone `google-cloud-cpp`:

```console
cd ..
git clone https://github.com/googleapis/google-cloud-cpp.git
git submodule update --init
cd google-cloud-cpp
```

Load the environment variables needed to use Microsoft Visual Studio:

```console
call "c:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
```

Use CMake to create the build files:

```console
cmake -H. -Bbuild-output -GNinja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="%SOURCE%\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
    -DCMAKE_C_COMPILER=cl.exe ^
    -DCMAKE_CXX_COMPILER=cl.exe ^
    -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package ^
    -DCMAKE_MAKE_PROGRAM=ninja
```

And compile the code:

```console
cmake --build build-output
```

Finally, verify the unit tests pass:

```console
cd build-output
ctest --output-on-failure
```

You will find compiled binaries in `build-output\` respective to their
source directories.

### Install

By default `google-cloud-cpp` downloads and compiles all its dependencies.
The default configuration disables the `install` target, because the version of
the dependencies downloaded by `google-cloud-cpp` may conflict with the versions
already installed in your system, or with the versions you want to use for
development.

To install `google-cloud-cpp` you must first install all its dependencies. Then
you must configure `google-cloud-cpp` to find these dependencies, and install
it.

Installing the dependencies themselves may be as simple as using the package
manager for your platform, or may require manually downloading, compiling, and
installing said dependencies.  The [INSTALL.md](INSTALL.md) file describes how
to successfully install `google-cloud-cpp` on several platforms.

## Versioning

This library follows [Semantic Versioning](http://semver.org/). Please note it
is currently under active development. Any release versioned 0.x.y is subject to
backwards-incompatible changes at any time.

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

