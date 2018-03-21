# Example: C++ Hello World Application

This example is a very simple "hello world" application, written in C++, that
illustrates how to:

* Connect to a Cloud Bigtable instance.
* Create a new table.
* Write data to the table.
* Read the data back.
* Delete the table.

## Running the example

This example uses the
[Cloud Bigtable C++ Client Library](https://GoogleCloudPlatform.github.io/) to
communicate with Cloud Bigtable.

To run the example program, follow the
[instructions](https://github.com/GoogleCloudPlatform/google-cloud-cpp/tree/master/bigtable/examples/)
for the example on GitHub.

### Including the Necessary Headers

The example uses the following headers:

@snippet bigtable_hello_world.cc bigtable includes

### Connecting to Cloud Bigtable to manage tables

To manage tables, connect to Cloud Bigtable using
`bigtable::CreateDefaultAdminClient()`:

@snippet bigtable_hello_world.cc connect admin

### Creating a table

Create a table with `bigtable::AdminClient::CreateTable()`:

@snippet bigtable_hello_world.cc create table

@see `bigtable::v0::TableAdmin` for additional operations to list, read, modify,
and delete tables.

### Connecting to Cloud Bigtable to manage data

To manage data, connect to Cloud Bigtable using
`bigtable::CreateDefaultDataClient()`:

@snippet bigtable_hello_world.cc connect data


### Writing Rows to a table

First connect to Cloud Bigtable C++ using the Data API:

@snippet bigtable_hello_world.cc connect data

Then write the row.

@snippet bigtable_hello_world.cc write row

@see `bigtable::v0::Table::BulkApply()` to modify multiple rows in a single
operation.  In addition to `SetCell()` there
are functions to delete columns (e.g., `DeleteFromFamily()`) or to delete the
whole row (`DeleteFromRow()`).

### Read a Table

Request the row that you want, and verify that the row is found:

@snippet bigtable_hello_world.cc read row

Then iterate over the cells in the row:

@snippet bigtable_hello_world.cc use value

@see `bigtable::v0::Filter` to filter the column families, columns, and even
  the timestamps returned by `ReadRow()`.
@see `bigtable::v0::Table::ReadRows()` to iterate over multiple rows.
