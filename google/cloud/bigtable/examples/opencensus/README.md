# OpenCensus example for GCP Bigtable C++ Client.
## Before you begin
### Setup StatckDriver
Instructions to setup StackDriver can be found [here](https://github.com/census-instrumentation/opencensus-cpp/tree/master/examples/grpc#stackdriver)

### Stats
One can view stats captured using OpenCensus at Google Cloud Console as explained [here](https://github.com/census-instrumentation/opencensus-cpp/tree/master/examples/grpc#stats)

### Traces
One can view traces captured using OpenCensus at Google Cloud Console as explained [here](https://github.com/census-instrumentation/opencensus-cpp/tree/master/examples/grpc#tracing)

## OpenCensus example
### Build examp[le
Binary `bigtable_opencensus` demonstrates the usage of OpenCensus for observability. This example current can be build using basel. One can build the binary using following command.

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
