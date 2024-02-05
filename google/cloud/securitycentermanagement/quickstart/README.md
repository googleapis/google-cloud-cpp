# HOWTO: using the Security Center Management API C++ client in your project

This directory contains small examples showing how to use the Security Center
Management API C++ client library in your own project. These instructions assume
that you have some experience as a C++ developer and that you have a working C++
toolchain (compiler, linker, etc.) installed on your platform.

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
- Developers who prefer using a package manager such as
  [vcpkg](https://vcpkg.io), or [Conda](https://conda.io), should follow the
  instructions for their package manager.
- Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the current document. Note that there are similar
  documents for each library in their corresponding directories.
- Developers wanting to compile the library just to run some examples or tests
  should consult the
  [building and installing](/README.md#building-and-installing) section of the
  top-level README file.
- Contributors and developers to `google-cloud-cpp` should consult the guide to
  [set up a development workstation][howto-setup-dev-workstation].

## Configuring authentication for the C++ Client Library

Like most Google Cloud Platform (GCP) services, Security Center Management API
requires that your application authenticates with the service before accessing
any data. If you are not familiar with GCP authentication please take this
opportunity to review the [Authentication Overview][authentication-quickstart].
This library uses the `GOOGLE_APPLICATION_CREDENTIALS` environment variable to
find the credentials file. For example:

| Shell              | Command                                                                              |
| :----------------- | ------------------------------------------------------------------------------------ |
| Bash/zsh/ksh/etc.  | `export GOOGLE_APPLICATION_CREDENTIALS=[PATH]`                                       |
| sh                 | `GOOGLE_APPLICATION_CREDENTIALS=[PATH];`<br> `export GOOGLE_APPLICATION_CREDENTIALS` |
| csh/tsch           | `setenv GOOGLE_APPLICATION_CREDENTIALS [PATH]`                                       |
| Windows Powershell | `$env:GOOGLE_APPLICATION_CREDENTIALS=[PATH]`                                         |
| Windows cmd.exe    | `set GOOGLE_APPLICATION_CREDENTIALS=[PATH]`                                          |

Setting this environment variable is the recommended way to configure the
authentication preferences, though if the environment variable is not set, the
library searches for a credentials file in the same location as the
[Cloud SDK](https://cloud.google.com/sdk/). For more information about
*Application Default Credentials*, see
https://cloud.google.com/docs/authentication/production

## Using with Bazel

> :warning: If you are using Windows or macOS there are additional instructions
> at the end of this document.

1. Install Bazel using [the instructions][bazel-install] from the `bazel.build`
   website.

1. Compile this example using Bazel:

   ```bash
   cd $HOME/google-cloud-cpp/google/cloud/securitycentermanagement/quickstart
   bazel build ...
   ```

   Note that Bazel automatically downloads and compiles all dependencies of the
   project. As it is often the case with C++ libraries, compiling these
   dependencies may take several minutes.

1. Run the example, changing the placeholder(s) to appropriate values:

   ```bash
   bazel run :quickstart -- [project-id]
   ```

## Using with CMake

> :warning: If you are using Windows or macOS there are additional instructions
> at the end of this document.

1. Install CMake. The package managers for most Linux distributions include a
   package for CMake. Likewise, you can install CMake on Windows using a package
   manager such as [chocolatey][choco-cmake-link], and on macOS using
   [homebrew][homebrew-cmake-link]. You can also obtain the software directly
   from the [cmake.org](https://cmake.org/download/).

1. Install the dependencies with your favorite tools. As an example, if you use
   [vcpkg](https://github.com/Microsoft/vcpkg.git):

   ```bash
   cd $HOME/vcpkg
   ./vcpkg install google-cloud-cpp[core,securitycentermanagement]
   ```

   Note that, as it is often the case with C++ libraries, compiling these
   dependencies may take several minutes.

1. Configure CMake, if necessary, configure the directory where you installed
   the dependencies:

   ```bash
   cd $HOME/google-cloud-cpp/google/cloud/securitycentermanagement/quickstart
   cmake -S . -B .build -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build .build
   ```

1. Run the example, changing the placeholder(s) to appropriate values:

   ```bash
   .build/quickstart [project-id]
   ```

## Platform Specific Notes

### macOS

gRPC [requires][grpc-roots-pem-bug] an environment variable to configure the
trust store for SSL certificates, you can download and configure this using:

```bash
curl -Lo roots.pem https://pki.google.com/roots.pem
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="$PWD/roots.pem"
```

### Windows

Bazel tends to create very long file names and paths. You may need to use a
short directory to store the build output, such as `c:\b`, and instruct Bazel to
use it via:

```shell
bazel --output_user_root=c:\b build ...
```

gRPC [requires][grpc-roots-pem-bug] an environment variable to configure the
trust store for SSL certificates, you can download and configure this using:

```console
@powershell -NoProfile -ExecutionPolicy unrestricted -Command ^
    (new-object System.Net.WebClient).Downloadfile( ^
        'https://pki.google.com/roots.pem', 'roots.pem')
set GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=%cd%\roots.pem
```

[authentication-quickstart]: https://cloud.google.com/docs/authentication/getting-started "Authentication Getting Started"
[bazel-install]: https://docs.bazel.build/versions/main/install.html
[choco-cmake-link]: https://chocolatey.org/packages/cmake
[grpc-roots-pem-bug]: https://github.com/grpc/grpc/issues/16571
[homebrew-cmake-link]: https://formulae.brew.sh/formula/cmake
[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
