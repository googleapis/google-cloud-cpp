# Google Cloud Bigtable Code Samples - Stock Backtesting

## Introduction
In this sample, we implement a stock backtesting system using Google Cloud
Bigtable as the back storage. Bigtable stores and serves the time series
of historical signals, i.e. price and dividend; this works together with the
"chasing ups and downs" strategy to decide the trading operations. Profits and
losses are reported at the end of the program.

## Setup

This example expands on the [quickstart](
https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/bigtable/quickstart/).
The quickstart contains relevant guidance for building and running the code on
various platforms. Please consult it if you run into any troubles building or
running this application.

## Populate Data

The `populate_data_main.cc` file is used to parse an input file name, read
the data, and populate it into a Bigtable table.

> Note: the program is using the following assumptions:
> * The input file basename is in the format of
> `{ticker}_historical_{price|dividend}.csv`
> * The input file is a CSV file where each line contains the information in
> the order of `Date`, `Open Price` ,`High Price`, `Low Price`, `Close Price`,
> `Adj Close Price`, and `Volume`.
> * The user's table has two column families: `price` and `dividend`.

### Running Populate Data

```console
$ cd $HOME/google-cloud-cpp/google/cloud/bigtable/examples/stock_backtesting
$ bazel run populate_data <path_to_input_file> <project_id> <instance_id> <table_id>
```
And the program will print out some running details such as how many
rows/mutations were populated, the time consumed, etc.

```console
Committing bulk mutation size 1000
Committing bulk mutation size 258
BigTable populated with data from file: /path/to/your/data/AMZN_historical_price.csv
Total num of row mutations: 1258
Total num of cell mutations: 6290
Total time used: 1.703300825s
```

### Data

We provide one set of Amazon (AMZN) historical price and dividend data. The
price data was downloaded from a Kaggle user's [upload](
https://www.kaggle.com/prasoonkottarathil/amazon-stock-price-20142019) with
[CC0](https://creativecommons.org/publicdomain/zero/1.0/) public domain
license. The dividend data is completely artificial.

## Backtesting

The `backtesting_main.cc` file is used to run the strategy against a period of
historical signals, and calculate the profit and losses.

### Running Backtesting

```console
$ cd $HOME/google-cloud-cpp/google/cloud/bigtable/examples/stock_backtesting
$ bazel run backtesting <path_to_strategy_file> <ticker> <YYYY-mm-dd(start_date)> <YYYY-mm-dd(end_date)> <project_id> <instance_id> <table_id>
```
The program will report errors if strategy is not defined correctly, the
start/end date is out of range, etc; otherwise it reports the end status at
the end date, including how many stocks and how much money are in hand.

```console
Shares in hand: 3.3717 @ 675.89
Money in hand: -1500
Total profit: 778.898
```
