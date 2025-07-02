# HOWTO: using the Google Cloud Storage (GCS) C++ client in your project

This directory contains small examples showing how to use the GCS C++ client
library in your own project. These instructions assume that you have some
experience as a C++ developer and that you have a working C++ toolchain
(compiler, linker, etc.) installed on your platform.

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

## Before you begin

To run the quickstart examples you will need a working Google Cloud Platform
(GCP) project and an existing bucket. The
[GCS quickstarts](https://cloud.google.com/storage/docs/introduction#quickstarts)
cover the necessary steps in detail. Make a note of the GCP project id and the
bucket name as you will need them below.

## Configuring authentication for the C++ Client Library

Like most Google Cloud Platform services, GCS requires that your application
authenticates with the service before accessing any data. If you are not
familiar with GCP authentication please take this opportunity to review the
[Authentication methods at Google][authentication-quickstart].

## Using with Bazel

1. Install Bazel using [the instructions][bazel-install] from the `bazel.build`
   website.

1. Compile this example using Bazel:

   ```bash
   cd $HOME/google-cloud-cpp/google/cloud/storage/quickstart
   bazel build ...
   ```

   Note that Bazel automatically downloads and compiles all dependencies of the
   project. As it is often the case with C++ libraries, compiling these
   dependencies may take several minutes.

1. Run the example, change the placeholder to appropriate values:

   ```bash
   bazel run :quickstart -- [BUCKET NAME]
   ```

   Please use the plain bucket name. Do **not** include any `gs://` prefix, and
   keep in mind the [bucket naming restrictions][bucket-naming-link]. For
   example, bucket names cannot include forward slashes (`/`) or uppercase
   letters.

## Using with CMake

1. Install CMake. The package managers for most Linux distributions include a
   package for CMake. Likewise, you can install CMake on Windows using a package
   manager such as [chocolatey][choco-cmake-link], and on macOS using
   [homebrew][homebrew-cmake-link]. You can also obtain the software directly
   from the [cmake.org](https://cmake.org/download/) site.

1. Install the dependencies with your favorite tools. As an example, if you use
   [vcpkg](https://github.com/Microsoft/vcpkg.git):

   ```bash
   cd $HOME/vcpkg
   ./vcpkg install google-cloud-cpp[core,storage]
   ```

   Note that, as it is often the case with C++ libraries, compiling these
   dependencies may take several minutes.

1. Configure CMake, if necessary, configure the directory where you installed
   the dependencies:

   ```bash
   cd $HOME/google-cloud-cpp/google/cloud/storage/quickstart
   cmake -S . -B .build -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build .build
   ```

1. Run the example, changing the placeholder(s) to appropriate values:

   ```bash
   .build/quickstart [BUCKET NAME]
   ```

   Please use the plain bucket name. Do **not** include any `gs://` prefix, and
   keep in mind the [bucket naming restrictions][bucket-naming-link]. For
   example, bucket names cannot include forward slashes (`/`) or uppercase
   letters.

## The gRPC plugin

The Google Cloud Storage client library includes a plugin to use gRPC as the
transport to access GCS. For the most part, only applications with very large
workloads (several Tbits/s of upload and/or download bandwidth) benefit from
GCS+gRPC.

To enable the GCS+gRPC plugin you need to (a) link your application with an
additional library, and (b) use a different function to initialize the
`google::cloud::storage::Client` object. Both changes are illustrated in the
`quickstart_grpc` program included in this directory.

### Using with CMake

The GCS+gRPC plugin is disabled in CMake builds, this minimized the impact in
build times and dependency management for customers not wanting to use the
feature. To enable the feature add `-DGOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC=ON`
to your CMake configuration step. Using the previous example:

```sh
cd $HOME/google-cloud-cpp/google/cloud/storage/quickstart
cmake -S . -B .build -DGOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC=ON \
    -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .build --target quickstart_grpc
```

Then run the additional program:

```sh
.build/quickstart_grpc [BUCKET NAME]
```

### Using with Bazel

Run the example, change the placeholder to appropriate values:

```bash
bazel run :quickstart_grpc -- [BUCKET NAME]
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

[authentication-quickstart]: https://cloud.google.com/docs/authentication/client-libraries "Authenticate for using client libraries"
[bazel-install]: https://docs.bazel.build/versions/main/install.html
[bucket-naming-link]: https://cloud.google.com/storage/docs/naming-buckets
[choco-cmake-link]: https://chocolatey.org/packages/cmake
[grpc-roots-pem-bug]: https://github.com/grpc/grpc/issues/16571
[homebrew-cmake-link]: https://formulae.brew.sh/formula/cmake
[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
