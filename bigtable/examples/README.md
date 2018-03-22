# Cloud Bigtable C++ Client Quick Start Code Samples

This directory contains minimal examples to get started quickly, and are
referenced from the Doxygen landing page as "Hello World".

## Compiling the Examples

These examples are compiled as part of the build for the Cloud Bigtable C++
Client.  The instructions on how to compile the code are in the
[top-level README](../../README.md) file.

## Running the examples against the Cloud Bigtable Emulator

The easiest way to run these examples is to use the Cloud Bigtable Emulator.
This emulator is included as part of the
[Google Cloud SDK](https://cloud.google.com/sdk/).

### Installing the Emulator

Follow the relevant
[documentation](https://cloud.google.com/bigtable/docs/emulator) to install
the emulator.

### Running the Emulator

Start the emulator as described in the documentation:

```console
$ gcloud beta emulators bigtable start
```

On a separate shell setup the necessary environment variables:

```console
$ $(gcloud beta emulators bigtable env-init)
```

## Running the examples against a production version of Cloud Bigtable

You first need to create a production instance, the easiest way is to
follow the
[instructions](https://cloud.google.com/bigtable/docs/creating-instance)
on the Cloud Bigtable site.

### Configuring gRPC Root Certificates

You may need to configure gRPC to accept the Google server certificates.
gRPC expects to find a
[PEM](https://en.wikipedia.org/wiki/Privacy-enhanced_Electronic_Mail) file
with the list of trusted root certificates in `/usr/share/grpc/roots.pem`
(or `${PREFIX}/share/grpc/roots.pem` if you installed gRPC with a different
`PREFIX`, e.g. `/usr/local`).  If your system does not have this file installed
you need to set the `GRPC_DEFAULT_SSL_ROOTS_FILE_PATH` to the location of this
file.

The gRPC source, included as a submodule of `google-cloud-cpp`, contains
a version of this file. If you want to use that version use:

```console
$ GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=$HOME/google-cloud-cpp/third_party/grpc/etc/roots.pem
$ export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH
```

### Authenticate with Google Cloud

You may need to authenticate with Google Cloud Platform. The Google Cloud SDK
offers a simple way to do so:

```console
$ gcloud auth login
```

## Running the Quick Start Examples using a Cloud Bigtable Instance

After configuring gRPC, you can run the examples using:

```console
$ export PROJECT_ID=... # The name of your project
$ export INSTANCE_ID=... # THe name of your instance
$ ./bigtable_hello_world ${PROJECT_ID} ${INSTANCE_ID} example-table
```
