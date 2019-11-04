# Google Cloud Platform C++ Client Libraries

C++ Idiomatic Clients for [Google Cloud Platform][cloud-platform] services.

**Core Results:**
[![Kokoro CI status][kokoro-ubuntu-shield]][kokoro-ubuntu-link]
[![Kokoro CI status][kokoro-macos-shield]][kokoro-macos-link]
[![Kokoro CI status][kokoro-windows-shield]][kokoro-windows-link]
[![Codecov Coverage status][codecov-shield]][codecov-link]

**Test Install Instructions:**
[![Kokoro install centos status][kokoro-install-centos-shield]][kokoro-install-centos-link]
[![Kokoro install debian status][kokoro-install-debian-shield]][kokoro-install-debian-link]
[![Kokoro install fedora status][kokoro-install-fedora-shield]][kokoro-install-fedora-link]
[![Kokoro install opensuse status][kokoro-install-opensuse-shield]][kokoro-install-opensuse-link]
[![Kokoro install opensuse-leap status][kokoro-install-opensuse-leap-shield]][kokoro-install-opensuse-leap-link]
[![Kokoro install ubuntu status][kokoro-install-ubuntu-shield]][kokoro-install-ubuntu-link]
[![Kokoro install ubuntu-xenial status][kokoro-install-ubuntu-xenial-shield]][kokoro-install-ubuntu-xenial-link]
[![Kokoro install ubuntu-trusty status][kokoro-install-ubuntu-trusty-shield]][kokoro-install-ubuntu-trusty-link]

**Special Builds:**
[![Kokoro CI status][kokoro-asan-shield]][kokoro-asan-link]
[![Kokoro CI status][kokoro-ubsan-shield]][kokoro-ubsan-link]
[![Kokoro CI status][kokoro-noex-shield]][kokoro-noex-link]
[![Kokoro CI status][kokoro-centos-7-shield]][kokoro-centos-7-link]
[![Kokoro CI status][kokoro-clang-tidy-shield]][kokoro-clang-tidy-link]
[![Kokoro CI status][kokoro-libcxx-shield]][kokoro-libcxx-link]
[![Kokoro CI status][kokoro-check-abi-shield]][kokoro-check-abi-link]
[![Kokoro CI status][kokoro-shared-shield]][kokoro-shared-link]
[![Travis CI status][travis-shield]][travis-link]
[![Kokoro CI status][kokoro-bazel-shield]][kokoro-bazel-link]

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
[kokoro-check-abi-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-check-abi.svg
[kokoro-check-abi-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-check-abi-link.html
[kokoro-clang-tidy-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-clang-tidy.svg
[kokoro-clang-tidy-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-clang-tidy-link.html
[kokoro-libcxx-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-libcxx.svg
[kokoro-libcxx-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-libcxx-link.html
[kokoro-shared-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-shared.svg
[kokoro-shared-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-shared-link.html
[kokoro-bazel-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-bazel-dependency-linux.svg
[kokoro-bazel-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-bazel-dependency-linux-link.html

[codecov-shield]: https://codecov.io/gh/googleapis/google-cloud-cpp/branch/master/graph/badge.svg
[codecov-link]: https://codecov.io/gh/googleapis/google-cloud-cpp
[cloud-platform]: https://cloud.google.com/
[cloud-platform-docs]: https://cloud.google.com/docs/
[client-library-docs]: http://googleapis.github.io/google-cloud-cpp/

This library supports the following Google Cloud Platform services with clients
at the [GA](#versioning) quality level:

- [Google Cloud Storage](google/cloud/storage/README.md)
- [Google Cloud Bigtable](google/cloud/bigtable/README.md)

## Table of Contents

- [Requirements](#requirements)
  - [Compiler](#compiler)
  - [Build Tools](#build-tools)
  - [Libraries](#libraries)
  - [Tests](#tests)
- [Install Dependencies](#install-dependencies)
  - [CentOS (7)](#centos-7)
  - [Debian (Stretch)](#debian-stretch)
  - [Fedora (30)](#fedora-30)
  - [openSUSE (Tumbleweed)](#opensuse-tumbleweed)
  - [openSUSE (Leap)](#opensuse-leap)
  - [Ubuntu (18.04 - Bionic Beaver)](#ubuntu-1804---bionic-beaver)
  - [Ubuntu (16.04 - Xenial Xerus)](#ubuntu-1604---xenial-xerus)
  - [Ubuntu (14.04 - Trusty Tahr)](#ubuntu-1404---trusty-tahr)
  - [macOS (using brew)](#macos-using-brew)
  - [Windows (using vcpkg)](#windows-using-vcpkg)
- [Build](#build)
  - [Linux](#linux)
  - [macOS](#macOS)
  - [Windows](#windows)
- [Install](#install)
- [Versioning](#versioning)
- [Contributing changes](#contributing-changes)
- [Licensing](#licensing)

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
| Bazel      | 0.24.0 |

#### Libraries

The libraries also depend on gRPC, libcurl, and the dependencies of those
libraries. The Google Cloud C++ Client libraries are tested with the following
versions of these dependencies:

| Library | Minimum version | Notes |
| ------- | --------------- | ----- |
| gRPC    | v1.16.x | |
| libcurl | 7.47.0  | 7.64.0 has known problems for multi-threaded applications |

#### Tests

Integration tests at times use the
[Google Cloud SDK](https://cloud.google.com/sdk/). The integration tests run
against the latest version of the SDK on each commit and PR.

## Install Dependencies

### CentOS (7)

[![Kokoro install centos-7 status][kokoro-install-centos-7-shield]][kokoro-install-centos-7-link]

[kokoro-install-centos-7-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-centos-7.svg
[kokoro-install-centos-7-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-centos-7-link.html

Install the development tools and OpenSSL. The development tools distributed
with CentOS (notably CMake) are too old to build the
`google-cloud-cpp` project. We recommend you install cmake3 from
[Software Collections](https://www.softwarecollections.org/).

```bash
sudo rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
sudo yum install -y centos-release-scl
sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
sudo yum makecache && \
sudo yum install -y automake cmake3 curl-devel gcc gcc-c++ git libtool \
        make openssl-devel pkgconfig tar wget which zlib-devel
sudo ln -sf /usr/bin/cmake3 /usr/bin/cmake && sudo ln -sf /usr/bin/ctest3 /usr/bin/ctest
```

### Debian (Stretch)

[![Kokoro install debian-stretch status][kokoro-install-debian-stretch-shield]][kokoro-install-debian-stretch-link]

[kokoro-install-debian-stretch-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-debian-stretch.svg
[kokoro-install-debian-stretch-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-debian-stretch-link.html

First install the development tools.

```bash
sudo apt update && \
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl1.0-dev make \
        pkg-config tar wget zlib1g-dev
```

### Fedora (30)

[![Kokoro install fedora status][kokoro-install-fedora-shield]][kokoro-install-fedora-link]

[kokoro-install-fedora-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-fedora.svg
[kokoro-install-fedora-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-fedora-link.html

Install the minimal development tools:

```bash
sudo dnf makecache && \
sudo dnf install -y cmake gcc-c++ git make openssl-devel pkgconfig \
        zlib-devel
```

### openSUSE (Tumbleweed)

[![Kokoro install opensuse-tumbleweed status][kokoro-install-opensuse-tumbleweed-shield]][kokoro-install-opensuse-tumbleweed-link]

[kokoro-install-opensuse-tumbleweed-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-opensuse-tumbleweed.svg
[kokoro-install-opensuse-tumbleweed-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-opensuse-tumbleweed-link.html

Install the minimal development tools:

```bash
sudo zypper refresh && \
sudo zypper install --allow-downgrade -y cmake gcc gcc-c++ git gzip \
        libcurl-devel libopenssl-devel make tar wget zlib-devel
```

### openSUSE (Leap)

[![Kokoro install opensuse-leap status][kokoro-install-opensuse-leap-shield]][kokoro-install-opensuse-leap-link]

[kokoro-install-opensuse-leap-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-opensuse-leap.svg
[kokoro-install-opensuse-leap-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-opensuse-leap-link.html

Install the minimal development tools:

```bash
sudo zypper refresh && \
sudo zypper install --allow-downgrade -y cmake gcc gcc-c++ git gzip \
        libcurl-devel libopenssl-devel make tar wget
```

### Ubuntu (18.04 - Bionic Beaver)

[![Kokoro install ubuntu-bionic status][kokoro-install-ubuntu-bionic-shield]][kokoro-install-ubuntu-bionic-link]

[kokoro-install-ubuntu-bionic-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-ubuntu-bionic.svg
[kokoro-install-ubuntu-bionic-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-ubuntu-bionic-link.html

Install the minimal development tools:

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

Install the minimal development tools:

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

To automatically download the dependencies and compile the libraries and
examples you can use the CMake [super build][super-build-link]:

[super-build-link]: https://blog.kitware.com/cmake-superbuilds-git-submodules/

```bash
# Add -DBUILD_TESTING=OFF to disable tests
cmake -Hsuper -Bcmake-out

# Adjust the number of threads used by modifying parameter for `-j 4`
# following command will also invoke ctest at the end
cmake --build cmake-out -- -j 4
```

You will find compiled binaries in `cmake-out/` respective to their source
paths.

If you prefer to compile against installed versions of the dependencies please
check the [INSTALL.md file](INSTALL.md).

If you prefer to install dependencies at a custom location and want to compile
against it, please check the instruction at [build with CMake](./doc/setup-cmake-environment.md).

#### macOS

```bash
export OPENSSL_ROOT_DIR=/usr/local/opt/libressl
# Add -DBUILD_TESTING=OFF to disable tests
cmake -Hsuper -Bcmake-out

# Adjust the number of threads used by modifying parameter for `-j 4`
cmake --build cmake-out -- -j 4

# Verify build by running tests
(cd cmake-out && ctest --output-on-failure)
```

You will find compiled binaries in `cmake-out/` respective to their source paths.
You will find compiled binaries in `cmake-out/` respective to their source
paths.

If you prefer to compile against installed versions of the dependencies please
check the [INSTALL.md file](INSTALL.md).

##### Problems with `/usr/include` on macOS Mojave (and later).

If you see the following error:

```bash
CMake Error in google/cloud/storage/CMakeLists.txt:
  Imported target "CURL::libcurl" includes non-existent path

    "/usr/include"
```

you [need to update your Xcode version](https://apple.stackexchange.com/questions/337940/why-is-usr-include-missing-i-have-xcode-and-command-line-tools-installed-moja).
Install the package located at
`/Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg`

and run:

```bash
xcode-select -s /Library/Developer/CommandLineTools
```

#### Windows

<!-- Last updated 2018-01-09 -->

If you prefer to manually compile on Windows the following instructions should
work, though there is a lot more variability on this platform. We welcome
suggestions to make this an easier process.

We will assume that you have installed CMake, Ninja, and "Microsoft Visual
Studio 2017". If you have not, install [Chocolatey](https://www.chocolatey.org)
using this command as the administrator:

```console
@"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command ^
 "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
```

Then you can easily install the necessary development tools.

```console
choco install -y cmake cmake.portable ninja visualstudio2017community
choco install -y visualstudio2017-workload-nativedesktop
choco install -y microsoft-build-tools
choco install -y git
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
    googleapis:x64-windows-static ^
    crc32c:x64-windows-static
.\vcpkg.exe integrate install
```

Now clone `google-cloud-cpp`:

```console
cd ..
git clone https://github.com/googleapis/google-cloud-cpp.git
cd google-cloud-cpp
```

Load the environment variables needed to use Microsoft Visual Studio:

```console
call "c:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
```

Use CMake to create the build files:

```console
cmake -H. -Bcmake-out -GNinja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="%SOURCE%\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
    -DCMAKE_C_COMPILER=cl.exe ^
    -DCMAKE_CXX_COMPILER=cl.exe ^
    -DCMAKE_MAKE_PROGRAM=ninja
```

And compile the code:

```console
cmake --build cmake-out
```

Finally, verify the unit tests pass:

```console
cd cmake-out
ctest --output-on-failure
```

You will find compiled binaries in `cmake-out\` respective to their
source directories.

### Installing `google-cloud-cpp` using CMake

The default CMake builds for `google-cloud-cpp` assume that all the necessary
dependencies are installed in your system. Installing the dependencies may be as
simple as using the package manager for your platform, or may require manually
downloading, compiling, and installing a number of additional libraries. The
[INSTALL.md](INSTALL.md) file describes how to successfully install
`google-cloud-cpp` on several platforms.

If installing all the dependencies is not an option for you, consider using a
CMake [super build][super-build-link], an example of such can be found in the
`super/` directory.

Alternatively, you may be able to use `google-cloud-cpp` as a git submodule of
your source, and then use CMake's [`add_subdirectory()`][add-subdirectory-link]
command to compile the project as part of your build. However, this is **not**
a configuration that we test routinely and/or we recommend for teams working on
large projects.

[add-subdirectory-link]: https://cmake.org/cmake/help/latest/command/add_subdirectory.html

## Versioning

Please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](http://semver.org/).

**GA**: Libraries defined at a GA quality level are expected to be stable and
any backwards-incompatible changes will be noted in the documentation. Major
changes to the API will signaled by changing major version number
(e.g. 1.x.y -> 2.0.0).

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
