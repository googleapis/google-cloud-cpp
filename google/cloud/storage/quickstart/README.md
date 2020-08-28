# HOWTO: using the Google Cloud Storage (GCS) C++ client in your project

This directory contains small examples showing how to use the GCS C++ client
library in your own project. These instructions assume that you have some
experience as a C++ developer and that you have a working C++ toolchain
(compiler, linker, etc.) installed on your platform.

## Before you begin

To run the quickstart examples you will need a working Google Cloud Platform
(GCP) project and an existing bucket.
The [GCS quickstarts](https://cloud.google.com/storage/docs/quickstarts) cover
the necessary steps in detail. Make a note of the GCP project id and the bucket
name as you will need them below.

## Configuring authentication for the C++ Client Library

Like most Google Cloud Platform services, GCS requires that
your application authenticates with the service before accessing any data. If
you are not familiar with GCP authentication please take this opportunity to
review the [Authentication Overview][authentication-quickstart]. This library
uses the `GOOGLE_APPLICATION_CREDENTIALS` environment variable to find the
credentials file. For example:

| Shell              | Command                                        |
| :----------------- | ---------------------------------------------- |
| Bash/zsh/ksh/etc.  | `export GOOGLE_APPLICATION_CREDENTIALS=[PATH]` |
| sh                 | `GOOGLE_APPLICATION_CREDENTIALS=[PATH];`<br> `export GOOGLE_APPLICATION_CREDENTIALS` |
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

1. Install Bazel using [the instructions][bazel-install] from the `bazel.build`
   website.

2. Compile this example using Bazel:

   ```bash
   cd $HOME/google-cloud-cpp/google/cloud/storage/quickstart
   bazel build ...
   ```

   Note that Bazel automatically downloads and compiles all dependencies of the
   project. As it is often the case with C++ libraries, compiling these
   dependencies may take several minutes.

3. Run the example, change the place holder to appropriate values:

   ```bash
   bazel run :quickstart -- [GCP PROJECT] [BUCKET NAME]
   ```

   Please use the plain bucket name. Do **not** include any `gs://` prefix, and
   keep in mind the [bucket naming restrictions][bucket-naming-link]. For
   example, bucket names cannot include forward slashes (`/`) or uppercase
   letters.

[bucket-naming-link]: https://cloud.google.com/storage/docs/naming-buckets

## Using with CMake

1. Install CMake. The package managers for most Linux distributions include a
   package for CMake. Likewise, you can install CMake on Windows using a package
   manager such as [chocolatey][choco-cmake-link], and on macOS using
   [homebrew][homebrew-cmake-link]. You can also obtain the software directly
   from the [cmake.org](https://cmake.org/download/) site.

2. Install the dependencies with your favorite tools. As an example, if you use
   [vcpkg](https://github.com/Microsoft/vcpkg.git):

   ```bash
   cd $HOME/vcpkg
   ./vcpkg install google-cloud-cpp
   ```

   Note that, as it is often the case with C++ libraries, compiling these
   dependencies may take several minutes.

3. Configure CMake, if necessary, configure the directory where you installed
   the dependencies:

   ```bash
   cd $HOME/gooogle-cloud-cpp/google/cloud/storage/quickstart
   cmake -H. -B.build -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build .build
   ```

4. Run the example, change the place holder to appropriate values:

   ```bash
   .build/quickstart [GCP PROJECT] [BUCKET NAME]
   ```

   Please use the plain bucket name. Do **not** include any `gs://` prefix, and
   keep in mind the [bucket naming restrictions][bucket-naming-link]. For
   example, bucket names cannot include forward slashes (`/`) or uppercase
   letters.

## Platform Specific Notes

### Windows

To correctly configure the MSVC runtime you should change the CMake minimum
required version to 3.15 or add `-DCMAKE_POLICY_DEFAULT_CMP0091=NEW` to the
CMake configuration step.

[bazel-install]: https://docs.bazel.build/versions/master/install.html
[choco-cmake-link]: https://chocolatey.org/packages/cmake
[homebrew-cmake-link]: https://formulae.brew.sh/formula/cmake
[cmake-download-link]: https://cmake.org/download/
[authentication-quickstart]: https://cloud.google.com/docs/authentication/getting-started 'Authentication Getting Started'
