# Cloud Bigtable C++ Client Quick Start Code Samples

This directory contains minimal examples to get started quickly, and are
referenced from the Doxygen landing page as "Hello World".

## Table of Contents

- [Before you begin](#before-you-begin)
  - [Compile the Examples](#compile-the-examples)  
  - [Run the examples against the Cloud Bigtable Service](#run-the-examples-against-the-cloud-bigtable-service)
- [Samples](#samples)
  - [Hello World](#hello-world)
  - [Administer Instances](#administer-instances)

## Before you begin

### Compile the Examples

These examples are compiled as part of the build for the Cloud Bigtable C++
Client.  The instructions on how to compile the code are in the
[top-level README](../../README.md) file.

### Run the examples against the Cloud Bigtable Service

You first need to create a production instance, the easiest way is to
follow the
[instructions](https://cloud.google.com/bigtable/docs/creating-instance)
on the Cloud Bigtable site.

#### Configure gRPC Root Certificates

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

#### Authenticate with Google Cloud Platform

You may need to authenticate with Google Cloud Platform. The Google Cloud SDK
offers a simple way to do so:

```console
$ gcloud auth login
```

## Samples

### Hello world

View the [Hello World][hello_world_code] example to see sample usage of the Bigtable client library.

#### Usage

```console
$ ./bigtable_hello_world
Usage: bigtable_hello_world <project_id> <instance_id> <table_id>
```

#### Run Hello world
After configuring gRPC, you can run the examples using:

```console
$ export PROJECT_ID=... # Your Google Cloud Platform project ID
$ export INSTANCE_ID=... # Cloud Bigtable instance ID
$ ./bigtable_hello_world ${PROJECT_ID} ${INSTANCE_ID} example-table
```

### Administer Instances

View the [example][instance_admin_code] to see sample usage of instance administration of
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

#### Run instance admin samples

After configuring gRPC, you can run the examples using:

```console
$ export PROJECT_ID=... # # Your Google Cloud Platform project ID
$ export INSTANCE_ID=... # Cloud Bigtable instance ID
$ export CLUSTER_ID=... # Cloud Bigtable cluster ID
$ export ZONE=... # Name of the zone where the example will create a new instance
$ ./bigtable_samples_instance_admin run ${PROJECT_ID} ${INSTANCE_ID} ${CLUSTER_ID} ${ZONE}
```

[hello_world_code]: bigtable_hello_world.cc
[instance_admin_code]: bigtable_samples_instance_admin.cc
