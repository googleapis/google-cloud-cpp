# HOWTO: using the Cloud Spanner C++ client in your project

This directory contains small examples showing how to use the Cloud Spanner C++
client library in your own project. These instructions assume that you have
some experience as a C++ developer and that you have a working C++ toolchain
(compiler, linker, etc.) installed on your platform.

## Configuring authentication for the C++ Client Library

Like most Google Cloud Platform (GCP) services, Cloud Spanner requires that
your application authenticates with the service before accessing any data. If
you are not familiar with GCP authentication please take this opportunity to
review the [Authentication Overview][authentication-quickstart]. This library
uses the `GOOGLE_APPLICATION_CREDENTIALS` environment variable to find the
credentials file. For example:

| Shell              | Command                                        |
| :----------------- | ---------------------------------------------- |
| Bash/zsh/ksh/etc.  | `export GOOGLE_APPLICATION_CREDENTIALS=[PATH]` |
| sh                 | `GOOGLE_APPLICATION_CREDENTIALS=[PATH];` `export GOOGLE_APPLICATION_CREDENTIALS` |
| csh/tsch           | `setenv GOOGLE_APPLICATION_CREDENTIALS [PATH]` |
| Windows Powershell | `$env:GOOGLE_APPLICATION_CREDENTIALS=[PATH]`   |
| Windows cmd.exe    | `set GOOGLE_APPLICATION_CREDENTIALS=[PATH]`    |

Setting this environment variable is the recommended way to configure the
authentication preferences, though if the environment variable is not set, the
library searches for a credentials file in the same location as the [Cloud
SDK](https://cloud.google.com/sdk/). For more information about *Application
Default Credentials*, see
https://cloud.google.com/docs/authentication/production

## Using with Bazel

1. Install Bazel using [the instructions][bazel-install] from the `bazel.build` website.

2. Compile this example using Bazel:

   ```bash
   cd $HOME/google-cloud-cpp/quickstart
   bazel build ...
   ```

3. Run the example, change the place holder to appropriate values:

   ```bash
   bazel run :quickstart -- [GCP PROJECT] [CLOUD SPANNER INSTANCE] [CLOUD SPANNER DATABASE]
   ```

If you are using Windows or macOS there are additional instructions at the end of this document.

## Using with CMake

1. Install CMake. The package managers for most Linux distributions include a package for CMake.
   Likewise, you can install CMake on Windows using a package manager such as
   [chocolatey][choco-cmake-link], and on macOS using [homebrew][homebrew-cmake-link]. You can also obtain the software
   directly from the [cmake.org][https://cmake.org/download/].

2. Install the dependencies with your favorite tools. As an example, if you use
   [vcpkg](https://github.com/Microsoft/vcpkg.git):

   ```bash
   cd $HOME/vcpkg
   ./vcpkg install google-cloud-cpp
   ```

3. Configure CMake, if necessary, configure the directory where you installed the dependencies:

   ```bash
   cd $HOME/gooogle-cloud-cpp-spanner/quickstart
   cmake -H. -B.build -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build .build
   ```

4. Run the example, change the place holder to appropriate values:

   ```bash
   .build/quickstart [GCP PROJECT] [CLOUD SPANNER INSTANCE] [CLOUD SPANNER DATABASE]
   ```

If you are using Windows or macOS there are additional instructions at the end of this document.

## Platform Specific Notes

### macOS

gRPC [requires][grpc-roots-pem-bug] an environment variable to configure the trust store for SSL certificates, you
can download and configure this using:

```bash
curl -Lo roots.pem https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="$PWD/roots.pem"
```

To workaround a [bug in Bazel][bazel-grpc-macos-bug], gRPC requires this flag on macOS builds, you can add the option
manually or include it in your `.bazelrc` file:

```bash
bazel build --copt=-DGRPC_BAZEL_BUILD ...
```

### Windows

To correctly configure the MSVC runtime you should change the CMake minimum
required version to 3.15 or add `-DCMAKE_POLICY_DEFAULT_CMP0091=NEW` to the
CMake configuration step.

gRPC [requires][grpc-roots-pem-bug] an environment variable to configure the trust store for SSL certificates, you
can download and configure this using:

```console
@powershell -NoProfile -ExecutionPolicy unrestricted -Command ^
    (new-object System.Net.WebClient).Downloadfile( ^
        'https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem', ^
        'roots.pem')
set GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=%cd%\roots.pem
```

[bazel-install]: https://docs.bazel.build/versions/master/install.html
[spanner-quickstart-link]: https://cloud.google.com/spanner/docs/quickstart-console
[grpc-roots-pem-bug]: https://github.com/grpc/grpc/issues/16571
[choco-cmake-link]: https://chocolatey.org/packages/cmake
[homebrew-cmake-link]: https://formulae.brew.sh/formula/cmake
[cmake-download-link]: https://cmake.org/download/
[bazel-grpc-macos-bug]: https://github.com/bazelbuild/bazel/issues/4341
[authentication-quickstart]: https://cloud.google.com/docs/authentication/getting-started 'Authentication Getting Started'

