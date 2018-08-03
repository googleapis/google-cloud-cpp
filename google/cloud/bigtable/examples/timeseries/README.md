# Cloud Bigtable C++ Client Time Series Demo

This demo shows how an application could potentially capture timeseries data in
real-time, and then perform some lightweight analysis on the data.

This demo uses historical US Equities "Trade and Quote" data.  Trade data
represents the actual stock transaction across the US equity markets.  Quote
data represents the best bid and offers (aka BBO) on each equity market.

In this demo we:

* Load the quote data into a `taq-quotes-YYYYMMDD` table, where each row
  represents a single quote.  The row key for this table is
  `SYMBOL#YYYYMMDD#TIMESTAMP`.
* Create a derived `shuffled-data` table, where each row represents **all**
  the quotes for a security in a day.  The row key for this table is
  `SYMBOL#YYYYMMDD`.
* Compute the securities with the highest average spread for each security using
  the `shuffled-quotes` table.
  
## Running the Demo

Please read the top-level [README.md](../../../../README.md) for instructions on
how to compile the code.  You should also create a Cloud Bigtable instance,
please follow the
[online instructions](https://cloud.google.com/bigtable/docs/quickstart-cbt)
and then set:

```console
PROJECT=...  # Your project id
INSTANCE=... # Your instance id
```

### Loading Trade Data

A separate program can be used to load trade data:

```console
./bigtable/demos/timeseries/upload_taq_trades $PROJECT $INSTANCE 20161024 TRADE.sorted.txt
```

You can use this data to compute the volume-weighted average price (VWAP) of a
few securities:

```console
./bigtable/demos/timeseries/compute_vwap $PROJECT $INSTANCE GOOG 20161024
./bigtable/demos/timeseries/compute_vwap $PROJECT $INSTANCE SPY 20161024
./bigtable/demos/timeseries/compute_vwap $PROJECT $INSTANCE IWM 20161024
```

This data can be shuffled such that the timeseries for each security is in a
single row:

```console
./bigtable/demos/timeseries/shuffle_taq_trades $PROJECT $INSTANCE
```

### Loading Quote Data

Go to the directory where you compiled the code and download the source data,
for example:

```console
cd google-cloud-cpp/.build
../bigtable/demos/timeseries/download_taq.sh
```

Then run the program to parse the source data and upload it to Cloud Bigtable:

```console
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=$HOME/google-cloud-cpp/third_party/grpc/etc/roots.pem
./bigtable/demos/timeseries/upload_taq_nbbo $PROJECT $INSTANCE 20161024 NBBO.sorted.txt
```

You can examine the data using the Cloud Bigtable command-line tool:

```console
cbt -project $PROJECT -instance $INSTANCE read taq-quotes count=3
```

Note that the results are sorted by row key.  Now shuffle the data so all the
data points for a single timeseries are in a single row:

```console
./bigtable/demos/timeseries/shuffle_taq_nbbo $PROJECT $INSTANCE
```

Get the securities with the largest average spread:

```console
./bigtable/demos/timeseries/compute_spread $PROJECT $INSTANCE 20161024
```

## Future Work

* Join the trade data and quote data with the best quote 'as-of' each trade.
