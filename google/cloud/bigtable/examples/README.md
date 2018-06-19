# Cloud Bigtable C++ Client Quick Start Code Samples

This directory contains minimal examples to get started quickly, and are
referenced from the Doxygen landing page as "Hello World".

## Table of Contents

- [Before you begin](#before-you-begin)
  - [Compiling the Examples](#compiling-the-examples)
  - [Running the examples against the Cloud Bigtable Emulator](#running-the-examples-against-the-cloud-bigtable-emulator)
  - [Running the examples against a production version of Cloud Bigtable](#running-the-examples-against-a-production-version-of-cloud-bigtable)
- [Samples](#samples)
  - [Hello World](#hello-world)
  - [Administer Instances](#administer-instances)

## Before you begin

### Compiling the Examples

These examples are compiled as part of the build for the Cloud Bigtable C++
Client.  The instructions on how to compile the code are in the
[top-level README](../../README.md) file.

### Running the examples against the Cloud Bigtable Emulator

The easiest way to run these examples is to use the Cloud Bigtable Emulator.
This emulator is included as part of the
[Google Cloud SDK](https://cloud.google.com/sdk/).

#### Installing the Emulator

Follow the relevant
[documentation](https://cloud.google.com/bigtable/docs/emulator) to install
the emulator.

#### Running the Emulator

Start the emulator as described in the documentation:

```console
$ gcloud beta emulators bigtable start
```

On a separate shell setup the necessary environment variables:

```console
$ $(gcloud beta emulators bigtable env-init)
```

### Running the examples against a production version of Cloud Bigtable

You first need to create a production instance, the easiest way is to
follow the
[instructions](https://cloud.google.com/bigtable/docs/creating-instance)
on the Cloud Bigtable site.

#### Configuring gRPC Root Certificates

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

#### Authenticate with Google Cloud

You may need to authenticate with Google Cloud Platform. The Google Cloud SDK
offers a simple way to do so:

```console
$ gcloud auth login
```

## Samples

### Hello world

View the [Hello World][hello_world_code] sample to see a basic usage of
the Bigtable client library.

#### Usage

```console
$ ./bigtable_hello_world
Usage: bigtable_hello_world <project_id> <instance_id> <table_id>
```
#### Running Hello world
After configuring gRPC, you can run the examples using:

```console
$ export PROJECT_ID=... # The name of your project
$ export INSTANCE_ID=... # The name of your instance
$ ./bigtable_hello_world ${PROJECT_ID} ${INSTANCE_ID} example-table
```
### Administer Instances

View the [sample][instances_code] to see basic usage of instance administration of
the Bigtable client library.

#### Usage

```console
$ ./bigtable_samples_instance_admin

Usage: bigtable_samples_instance_admin <command> <project_id> [arguments]

Examples:
  bigtable_samples_instance_admin run my-project my-instance my-cluster us-central1-f
  bigtable_samples_instance_admin create-dev-instance my-project my-instance us-central1-f
  bigtable_samples_instance_admin delete-instance my-project my-instance
  bigtable_samples_instance_admin create-cluster my-project my-instance my-cluster us-central1-a
  bigtable_samples_instance_admin delete-cluster my-project my-instance my-cluster
```
#### Running instance admin samples

After configuring gRPC, you can run the examples using:

```console
$ export PROJECT_ID=... # The name of your project
$ export INSTANCE_ID=... # The name of your instance
$ export CLUSTER_ID=... # The name of cluster
$ export ZONE=... # The name of zone
$ ./bigtable_samples_instance_admin run ${PROJECT_ID} ${INSTANCE_ID} ${CLUSTER_ID} ${ZONE}
```

[hello_world_code]: bigtable_hello_world.cc
[instances_code]: bigtable_samples_instance_admin.cc
