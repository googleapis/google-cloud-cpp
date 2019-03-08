# OpenCensus example for GCP Bigtable C++ Client.
## Before you begin
### About OpenCensus
[OpenCensus](https://opencensus.io) is a vendor-agnostic single distribution of libraries to provide metrics collection and tracing for your services. In this example we show how one can integrate OpenCensus with the Cloud Bigtable C++ client library.

### Setup Stackdriver
The OpenCensus example will publish metrics and traces to [Stackdriver](https://cloud.google.com/stackdriver/), if you have not previously configured Stackdriver, follow [these instructions](https://github.com/census-instrumentation/opencensus-cpp/tree/master/examples/grpc#stackdriver) to enable Stackdriver for your Google Cloud project.

### Stats
One can view stats captured using OpenCensus at Google Cloud Console as explained [here](https://github.com/census-instrumentation/opencensus-cpp/tree/master/examples/grpc#stats)

### Traces
One can view traces captured using OpenCensus at Google Cloud Console as explained [here](https://github.com/census-instrumentation/opencensus-cpp/tree/master/examples/grpc#tracing)

## OpenCensus example
### Build example
The `bigtable_opencensus` example demonstrates how to integrate the OpenCensus library with the Cloud Bigtable C++ client. OpenCensus for C++ only supports [Bazel](https://bazel.build/) builds, so you must build the Cloud Bigtable C++ client using Bazel too.  Follow the instructions on the [Bazel site](https://docs.bazel.build/versions/master/install.html) to install Bazel for your platform, then run this command-line from the top-level directory for `google-cloud-cpp`:

```
bazel build "//google/cloud/bigtable/...:all"
```

### Run example
This example uses:
 * The OpenCensus gRPC plugin to instrument the Bigtable gRPC calls.
 * The Stackdriver exporter to export stats and traces.
 * Debugging exporters to print stats and traces to stdout.

Usage:

```
bigtable_opencensus <project_id> <instance_id> <table_id>
```

Please note that data collected by OpenCensus can be exported to many analysis tool or storage backend, a comprehansive list of tools supported by OpenCensus can be found [here](https://opencensus.io/core-concepts/exporters/).
