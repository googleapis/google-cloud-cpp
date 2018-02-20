# Cloud Bigtable C++ Client Libraries

## Quick Start

This page explains how to use the Cloud Bigtable C++ library to connect to a
Cloud Bigtable instance, perform basic administrative tasks, and read and write
data in a table.

### Before you begin

Please consult the Cloud Bigtable
[documentation](https://cloud.google.com/bigtable/docs/creating-instance)
on how to create an instance.

### Create a Table

First connect to Cloud Bigtable C++ using the Admin API:

@snippet quick_start/bigtable_create_table.cc connect

Then define the column families and garbage collection rules for your table:

@snippet quick_start/bigtable_create_table.cc define schema

And then create the table:

@snippet quick_start/bigtable_create_table.cc create table

@see `bigtable::v0::TableAdmin` for additional operations to list, read, modify,
and delete tables. 

### Write a Row

First connect to Cloud Bigtable C++ using the Data API:

@snippet quick_start/bigtable_write_row.cc connect data

Then write the row.

@snippet quick_start/bigtable_write_row.cc write row

@see `bigtable::v0::Table::BulkApply()` to modify multiple rows in a single
operation.  In addition to `SetCell()` there
are functions to delete columns (e.g., `DeleteFromFamily()`) or to delete the
whole row (`DeleteFromRow()`).

### Read a Table

First connect to Cloud Bigtable C++ using the Data API:

@snippet quick_start/bigtable_read_row.cc connect data

Request the row that you want:

@snippet quick_start/bigtable_read_row.cc read row

In this example we request all the columns, you could filter the column
families, columns, and timestamps to only obtain a subset. Check the
`bigtable::v0::Filter` documentation for additional filtering primitives.
In any case, you should verify if the row really exists:

@snippet quick_start/bigtable_read_row.cc check result

And if it does you can iterate over the returned cells:

@snippet quick_start/bigtable_read_row.cc use value

@see `bigtable::v0::Table::ReadRows()` to iterate over multiple rows.
