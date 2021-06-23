# Google Cloud Bigtable Code Samples

## Table of Contents

- [Before you begin](#before-you-begin)
  - [Set Up Project](#set-up-project)
  - [Compile Examples](#compile-examples)
  - [Configure Environment](#configure-environment)
- [Samples](#samples)
  - [Hello World](#hello-world)
  - [Administer Instances](#administer-instances)
  - [Administer Tables](#administer-tables)
  - [Reading and Writing Operations](#reading-and-writing-operations)
  - [Credentials](#credentials)

## Before you begin

### Set Up Project

You will need a Google Cloud Platform (GCP) Project with billing and the
Bigtable Admin API enabled. The
[Cloud Bigtable quickstart][cbt-doc-quickstart]
covers the necessary steps in detail. Make a note of the GCP Project ID,
Instance ID, and Table ID as you will need them below.

### Compile Examples

These examples are compiled as part of the build for the Cloud Bigtable C++
Client.  The instructions on how to compile the code are in the
[top-level README](/README.md) file. The Bigtable client library
[quickstart](../quickstart/README.md) may also be relevant.

### Configure Environment

On Windows and macOS, gRPC [requires][grpc-roots-pem-bug] an environment variable
to find the root of trust for SSL. On macOS use:

```console
curl -Lo roots.pem https://pki.google.com/roots.pem
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="$PWD/roots.pem"
```

While on Windows use:

```console
@powershell -NoProfile -ExecutionPolicy unrestricted -Command ^
    (new-object System.Net.WebClient).Downloadfile( ^
        'https://pki.google.com/roots.pem', 'roots.pem')
set GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=%cd%\roots.pem
```

# Samples

## Hello World

View the [Hello World][hello-world-code] example to see sample usage of the
Bigtable client library. More details on this sample code can be found
[here][doxygen-hello-world].

### Running Hello World

```console
$ ./bigtable_hello_world hello-world <project_id> <instance_id> <table_id>
```

## Administer Instances

### Hello World - `InstanceAdmin`

View the [Hello Instance Admin][instance-admin-code] example to see sample usage of instance
administration of the Bigtable client library. More details on this sample code can be found
[here][doxygen-instance-admin].

### Running `InstanceAdmin` Samples

| Target                               | Description                         |
| ------------------------------------ | ----------------------------------- |
| `./bigtable_hello_instance_admin`    | Demonstration of basic operations   |
| `./bigtable_hello_app_profile`       | Demonstration of basic operations using [App Profile][cbt-doc-app-profiles] |
| `./bigtable_instance_admin_snippets` | Collection of individual operations |

Run the above targets with the `--help` flag to display the available commands and their usage.
Here is an example of one such command which will create an instance.

```console
$ ./bigtable_instance_admin_snippets create-instance <project-id> <instance-id> <zone-id>
```

## Administer Tables

### Hello World - `TableAdmin`

View the [Hello Table Admin][table-admin-code] example to see sample usage of table
administration of the Bigtable client library. More details on this sample code can be found [here][doxygen-table-admin].

### Running `TableAdmin` Samples

| Target                                   | Description                         |
| ---------------------------------------- | ----------------------------------- |
| `./bigtable_hello_table_admin`           | Demonstration of basic operations   |
| `./table_admin_snippets`                 | Collection of individual operations |
| `./bigtable_table_admin_backup_snippets` | Collection of [Backup][cbt-doc-backups] operations |

Run the above targets with the `--help` flag to display the available commands and their usage.
Here is an example of one such command which will create a table.
```console
$ ./table_admin_snippets create-table <project-id> <instance-id> <table-id>
```

## Reading and Writing Data Operations

| Target                   | Description                                      |
| ------------------------ | ------------------------------------------------ |
| `./read_snippets`        | Collection of synchronous read operations        |
| `./data_snippets`        | Collection of other synchronous table operations |
| `./data_async_snippets`  | Collection of asynchronous table operations      |
| `./data_filter_snippets` | Collection of operations using [Filters][cbt-doc-filters] |

The above samples demonstrate individual data operations. Running the targets with
the `--help` flag will display the available commands and their usage. Here is an example of one
such command which will read a row from a table.

```console
$ ./read_snippets read-row <project-id> <instance-id> <table-id> <row-key>
```

## Credentials

The following samples demonstrate use of [Authentication][cbt-doc-authentication]
and [Access Control][cbt-doc-access-control] for Bigtable. More details on the samples
can be found [here][doxygen-grpc].

| Target                              | Description                            |
| ----------------------------------- | -------------------------------------- |
| `./table_admin_iam_policy_snippets` | Examples to interact with IAM policies |
| `./bigtable_grpc_credentials`       | Examples of other types of credentials |

 Running the targets with the `--help` flag will display the available commands and their usage. Here is an example of one such command which will output the IAM Policy for a given table.

```console
$ ./table_admin_iam_policy_snippets get-iam-policy <project-id> <instance-id> <table-id>
```

[hello-world-code]: bigtable_hello_world.cc
[instance-admin-code]: bigtable_hello_instance_admin.cc
[table-admin-code]: bigtable_hello_table_admin.cc
[grpc-roots-pem-bug]: https://github.com/grpc/grpc/issues/16571
[cbt-doc-quickstart]: https://cloud.google.com/bigtable/docs/quickstart-cbt
[cbt-doc-app-profiles]: https://cloud.google.com/bigtable/docs/app-profiles
[cbt-doc-backups]: https://cloud.google.com/bigtable/docs/backups
[cbt-doc-filters]: https://cloud.google.com/bigtable/docs/filters
[cbt-doc-authentication]: https://cloud.google.com/bigtable/docs/authentication
[cbt-doc-access-control]: https://cloud.google.com/bigtable/docs/access-control
[doxygen-hello-world]: https://googleapis.dev/cpp/google-cloud-bigtable/latest/bigtable-hello-world.html
[doxygen-instance-admin]: https://googleapis.dev/cpp/google-cloud-bigtable/latest/bigtable-hello-instance-admin.html
[doxygen-table-admin]: https://googleapis.dev/cpp/google-cloud-bigtable/latest/bigtable-hello-table-admin.html
[doxygen-grpc]: https://googleapis.dev/cpp/google-cloud-bigtable/latest/bigtable-samples-grpc-credentials.html
