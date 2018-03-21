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

First connect to Cloud Bigtable using the Admin API, define the desired
schema, and create the table:

@snippet bigtable_quick_start.cc create table

@see `bigtable::v0::TableAdmin` for additional operations to list, read, modify,
and delete tables. 

### Write a Row

First connect to Cloud Bigtable C++ using the Data API:

@snippet bigtable_quick_start.cc connect data

Then write the row.

@snippet bigtable_quick_start.cc write row

@see `bigtable::v0::Table::BulkApply()` to modify multiple rows in a single
operation.  In addition to `SetCell()` there
are functions to delete columns (e.g., `DeleteFromFamily()`) or to delete the
whole row (`DeleteFromRow()`).

### Read a Table

Request the row that you want, and verify that the row is found:

@snippet bigtable_quick_start.cc read row

Then iterate over the cells in the row:

@snippet bigtable_quick_start.cc use value

@see `bigtable::v0::Filter` to filter the column families, columns, and even
  the timestamps returned by `ReadRow()`.
@see `bigtable::v0::Table::ReadRows()` to iterate over multiple rows.
