# Google Cloud Bigtable Code Samples - Stock Backtesting

# Introduction
In this sample, we implement a stock backtesting system using Google Cloud's
Bigtable as the back storage. The Bigtable stores and serves the time series
of historical signals, i.e. price and dividend, which works together with the
"chasing ups and downs" strategy to decide the trading operations. A P&L is
given at the end of the program running.

# Populate Data

The `populate_data_main.cc` file is used to parse an input file name, read
the data, and populate it into a Bigtable.

> Note: the program is using the following assumptions:
> * The input file basename is in the format of
> `{ticker}_historical_{price|dividend}.csv`
> * The input file is a CSV file where each line contains the information in
> the order of `Date`, `Open Price` ,`High Price`, `Low Price`, `Close Price`,
> `Adj Close Price`, and `Volume`.
> * The user's Bigtable has two column families, `price` and `dividend`.

## Running Populate Data

```console
$ cd $HOME/google-cloud-cpp/google/cloud/bigtable/examples/stock_backtesting
$ bazel run populate_data <path_to_input_file> <project_id> <instance_id> <table_id>
```
And the program will print out some running details such as how many
rows/mutations were populated, the time consumed, etc.

# Backtesting

The `backtesting_main.cc` file is used to run a period of historical signals
against the strategy, and calculate the P&L.

## Running Backtesting

```console
$ cd $HOME/google-cloud-cpp/google/cloud/bigtable/examples/stock_backtesting
$ bazel run backtesting <path_to_strategy_file> <ticker> <start_date> <end_date> <project_id> <instance_id> <table_id>
```
The program will report errors if strategy is not defined correctly, the
start/end date is out of range, etc; otherwise it reports the end status at
the end date, including how many stocks and how much money are in hand.
