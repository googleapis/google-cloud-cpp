# Cloud Bigtable C++ Client Quick Start Code Samples

This directory contains minimal examples to get started quickly, and are
referenced from the Doxygen landing page as as "Hello World".

## Compiling the Quickstart Examples

These examples are compiled as part of the build for the Cloud Bigtable C++
Client.

## Creating a Cloud Bigtable Instance

These examples can connect to a production instance of Cloud Bigtable or to the
Cloud Bigtable Emulator.  To use use a production instance, please consult the
Cloud Bigtable
[documentation](https://cloud.google.com/bigtable/docs/creating-instance).
To use the Cloud Bigtable emulator, please consult its
[documentation](https://cloud.google.com/bigtable/docs/emulator).

### Configuring gRPC Root Certificates

If you are using a production instance you may need to configure gRPC to accept
the Google server certificates.  gRPC expects to find a
[PEM](https://en.wikipedia.org/wiki/Privacy-enhanced_Electronic_Mail) file
with the list of trusted root certificates in `/usr/share/grpc/roots.pem`
(or `${PREFIX}/share/grpc/roots.pem` if you installed gRPC with a different
`PREFIX`, e.g. `/usr/local`).  If your system does not have this file installed
you need to set the `GRPC_DEFAULT_SSL_ROOTS_FILE_PATH` to the location of this
file. The gRPC source, included as a submodule of `google-cloud-cpp`, contains
a version of this file. If you want to use that version use:

```console
$ GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=$HOME/google-cloud-cpp/third_party/grpc/etc/roots.pem
$ export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH
```

### Authenticate with Google Cloud

If you are usig a production instance you may need to authenticate with Google
Cloud Platform. The Google Cloud SDK offers a simple way to do so:

```console
$ gcloud auth login
```

## Running the Quick Start Examples using a Cloud Bigtable Instance

After configuring gRPC, run the examples:

```console
$ export PROJECT_ID=... # The name of your project
$ export INSTANCE_ID=... # THe name of your instance
$ ./bigtable_create_table ${PROJECT_ID} ${INSTANCE_ID} example-table
$ ./bigtable_write_row ${PROJECT_ID} ${INSTANCE_ID} example-table
$ ./bigtable_read_row ${PROJECT_ID} ${INSTANCE_ID} example-table
```
