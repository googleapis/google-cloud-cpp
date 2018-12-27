# Cloud Bigtable C++ Client Quick Start Code Samples

This directory contains minimal examples to get started quickly, and are
referenced from the Doxygen landing page as "Hello World".

## Table of Contents

- [Before you begin](#before-you-begin)
  - [Compile the Examples](#compile-the-examples)  
  - [Run the Examples](#run-the-examples)
- [Samples](#samples)
  - [Hello World](#hello-world)
  - [Administer Instances](#administer-instances)
  - [Administer Tables](#administer-tables)
  - [Reading and Writing Operations](#reading-and-writing-operations)

## Before you begin

### Compile the Examples

These examples are compiled as part of the build for the Cloud Bigtable C++
Client.  The instructions on how to compile the code are in the
[top-level README](../../README.md) file.

### Run the Examples

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
$ export INSTANCE_ID=... # Your Cloud Bigtable instance ID
$ ./bigtable_hello_world ${PROJECT_ID} ${INSTANCE_ID} example-table
```

### Administer Instances

View the [example][instance_admin_code] to see sample usage of instance administration of
the Bigtable client library.

#### Usage

```console
$ ./bigtable_samples_instance_admin_ext

Usage: bigtable_samples_instance_admin_ext <command> <project_id> [arguments]

Examples:
  bigtable_samples_instance_admin_ext run my-project my-instance my-cluster us-central1-f
  bigtable_samples_instance_admin_ext create-dev-instance my-project my-instance us-central1-f
  bigtable_samples_instance_admin_ext delete-instance my-project my-instance
  bigtable_samples_instance_admin_ext create-cluster my-project my-instance my-cluster us-central1-a
  bigtable_samples_instance_admin_ext delete-cluster my-project my-instance my-cluster
```

**Note:** In the `create-cluster` line, a different zone is chosen as an instance's clusters must be placed in unique zones that are within the same region. See the [documentation](https://cloud.google.com/bigtable/docs/replication-overview#how-it-works) for more details.

#### Run instance admin samples

After configuring gRPC, you can run the examples using:

```console
$ export PROJECT_ID=... # Your Google Cloud Platform project ID
$ export INSTANCE_ID=... # Your Cloud Bigtable instance ID
$ export CLUSTER_ID=... # Your Cloud Bigtable cluster ID
$ export ZONE=... # Name of the zone where the example will create a new instance
$ ./bigtable_samples_instance_admin_ext run ${PROJECT_ID} ${INSTANCE_ID} ${CLUSTER_ID} ${ZONE}
```

[hello_world_code]: bigtable_hello_world.cc
[instance_admin_code]: bigtable_samples_instance_admin_ext.cc

### Administer Tables

This sample showcases the basic table / column family operations:

- Create a table
- List tables in the current project
- Retrieve table metadata
- List table column families and GC rules
- Update a column family GC rule
- Delete all the rows
- Delete a table

#### Usage

```console
$ ./bigtable_samples

Usage: bigtable_samples <command> <project_id> <instance_id> <table_id> [arguments]

Examples:
  bigtable_samples run my-project my-instance my-table 
  table_admin_snippets create-table my-project my-instance my-table
  table_admin_snippets list-tables my-project my-instance
  table_admin_snippets get-table my-project my-instance my-table
  table_admin_snippets modify-table my-project my-instance my-table
  table_admin_snippets drop-all-rows my-project my-instance my-table
  table_admin_snippets delete-table my-project my-instance my-table  
```
#### Run admin table samples
After configuring gRPC, you can run the examples using:

```console
$ export PROJECT_ID=... # Your Google Cloud Platform project ID
$ export INSTANCE_ID=... # Cloud Bigtable instance ID
$ export TABLE_ID=... # Cloud Bigtable table ID
$ ./bigtable_samples run ${PROJECT_ID} ${INSTANCE_ID} ${TABLE_ID}
```

### Reading and Writing Operations

This sample showcases reading and writing operations:

- Use Apply to write a single row
- Use BulkApply to write several rows in an operation
- Use ReadRow to read a single row
- Use ReadRows to read several rows in a range
- Use ReadRows to read 'N' rows in a range
- Use CheckAndMutate to modify value
- Use ReadModifyWrite to increament a value and append a string to a value
- Use SampleRows to fetch some sample rows


#### Usage

```console
$ ./data_snippets

Usage: data_snippets <command> <project_id> <instance_id> <table_id> [arguments]

Examples:
  data_snippets apply my-project my-instance my-table
  data_snippets bulk-apply my-project my-instance my-table
  data_snippets read-row my-project my-instance
  data_snippets read-rows my-project my-instance my-table
  data_snippets read-rows-with-limit my-project my-instance my-table
  data_snippets check-and-mutate my-project my-instance my-table
  data_snippets read-modify-write my-project my-instance my-table
  data_snippets sample-rows my-project my-instance my-table
  data_snippets sample-rows-collections my-project my-instance my-table
```

#### Run reading and writing samples
After configuring gRPC, you can run the examples using:

```console
$ export PROJECT_ID=... # Your Google Cloud Platform project ID
$ export INSTANCE_ID=... # Cloud Bigtable instance ID
$ export TABLE_ID=... # Cloud Bigtable table ID
$ ./data_snippets apply ${PROJECT_ID} ${INSTANCE_ID} ${TABLE_ID}
```

[hello_world_code]: bigtable_hello_world.cc
[instance_admin_code]: bigtable_samples_instance_admin.cc
[table_admin_code]: table_admin_snippets.cc
[data_snippets]: data_snippets.cc
