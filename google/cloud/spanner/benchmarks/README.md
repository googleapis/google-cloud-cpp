# Cloud Spanner C++ Client Library Benchmarks

This directory contains end-to-end benchmarks for the Cloud Spanner C++ client
library. The benchmarks execute experiments against the production environment.
You need a working Google Cloud Platform project and Cloud Spanner instance to
run these benchmarks. We recommend that you use an isolated instance, that is
one without any other workloads, to run each experiment, and that you do not run
more than one experiment at a time in said instance.

## Creating an instance

Assuming you have an existing Google Cloud Project you can create a Cloud
Spanner instance using the console or:

```console
GOOGLE_CLOUD_PROJECT=...                      # Your project ID
GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID=benchmarks  # Choose your instance ID
INSTANCE_CONFIG=regional-us-central1          # Choose the instance location(s)
gcloud spanner instances create ${GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID} \
    --config=${INSTANCE_CONFIG} --description="An instance to run Benchmarks" \
    --nodes=3
```

## CPU Overhead Experiment

This experiment measures the CPU overhead of the client library vs. raw gRPC.
This overhead is never expected to be zero, the library is performing useful
work, but we want it to be low, unsurprising, and (once a baseline is
established) to remain stable unless there is good reason to add overhead.

We recommend that you compile and run these experiments on a VM running on the
same region as the spanner instance you will use for the tests. Create and
configure the VM instance, and then install the development tools for whatever
platform you chose. See [doc/packaging.md][packaging-doc-link] for install
instructions for your platform.

### Compiling the library

You must compile both the library and its dependencies with optimization. The
[contributor documentation](/doc/contributor/README.md) has the relevant
information.

### Configuring Authentication

You need to configure the Cloud Spanner client library so it can authenticate
with Cloud Spanner. While covering authentication in detail is beyond the scope
of this README, we assume the reader is familiar with the topic, and refer them
to the [Authentication methods at Google][authentication-quickstart] if they
need a more in-depth discussion.

Examine the [spanner roles][spanner-roles-link] to chose a role for the
principal used to run these benchmarks. The principa should have (at least) the
permissions granted by the `roles/spanner.databaseAdmin` role.

### Running the benchmark

By default the benchmarks run simple smoke tests, the intention is for these
benchmarks to run as part of the CI build, where we want them to finish quickly.
You must specify some options to control for how long the experiments run, for
example, to perform the experiment measuring the CPU overhead of reading columns
of type `STRING` you would run:

```bash
.build/google/cloud/spanner/benchmarks/multiple_rows_cpu_benchmark \
    --project=${GOOGLE_CLOUD_PROJECT} \
    --instance=${GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID} \
    --table-size=1000000 \
    --maximum-channels=8 \
    --maximum-threads=16 \
    --iteration-duration=5 \
    --samples=60 --experiment=read-string | tee mrcb-read-string.csv
```

The program can run the same experiment with other data types:

```bash
for exp in read-bool read-bytes read-date read-float64 \
           read-int64 read-string read-timestamp; do \
  .build/google/cloud/spanner/benchmarks/multiple_rows_cpu_benchmark \
    --project=${GOOGLE_CLOUD_PROJECT} \
    --instance=${GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID} \
    --table-size=1000000 \
    --maximum-channels=8 \
    --maximum-threads=16 \
    --iteration-duration=5 \
    --samples=60 --experiment=${exp} | tee mrcb-${exp}.csv; \
done
```

### Inspecting the results

At this time we have not developed scripts to analyze the benchmark results, but
some simple R commands can help, start R in your command line and then issue the
following commands:

```R
require(ggplot2) # may require install.packages("ggplot2") the first time
df <- data.frame()
for(file in Sys.glob("mrcb-read-*.csv")) {
    t <- read.csv(file, comment.char='#');
    name <- gsub('mrcb-([a-z-]+).*', '\\1', file);
    t$experiment = factor(name);
    df <- rbind(df, t);
}

df$CpuTimePerRow <- df$CpuTime / df$RowCount

aggregate(CpuTimePerRow ~ UsingStub + experiment, data=df, FUN=mean)

ggplot(data=df, aes(color=UsingStub, x=experiment, y=CpuTime)) + geom_boxplot()
ggsave('read-data-types.png')
```

## Single Row Throughput Experiment

This experiment measures the throughput of either single-row inserts or
single-row reads using different numbers of channels and threads. The objective
is to verify the client library scales well with more threads and those not
introduce bottlenecks.

To run the experiment reading data for approximately 5 minutes use 20 samples of
15 seconds each:

```bash
.build/google/cloud/spanner/benchmarks/single_row_throughput_benchmark \
    --project=${GOOGLE_CLOUD_PROJECT} \
    --instance=${GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID} \
    --iteration-duration=15 \
    --table-size=10000000 \
    --maximum-channels=32 \
    --maximum-threads=1024 \
    --samples=20 2>&1 \
    --experiment=read | tee srtp-read.csv
```

[authentication-quickstart]: https://cloud.google.com/docs/authentication/client-libraries "Authenticate for using client libraries"
[packaging-doc-link]: /doc/packaging.md
[spanner-roles-link]: https://cloud.google.com/spanner/docs/iam#roles
