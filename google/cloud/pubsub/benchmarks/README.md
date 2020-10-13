# Cloud Pub/Sub C++ Client Library Benchmarks

This directory contains end-to-end benchmarks for the Cloud Pub/Sub C++ client
library. The benchmarks execute experiments against the production environment.
You need a working Google Cloud Platform project.

## CPU Overhead Experiment

TODO([#4565](https://github.com/googleapis/google-cloud-cpp/issues/4565)) -
describe the experiment in detail. Basically this should measure throughput for
publishers and subscribers and compare the "CPU overhead" of raw gRPC.
The experiment should randomize message sizes, batch sizes, and any other
parameters we consider relevant. Use an external script to analyze. Possibly use
an statistical model based on some linear assumptions (e.g. CPU usage grows
linearly with message count and with message size).

## Endurance Experiment

This experiment is largely a torture test for the library. The objective is to
detect bugs that escape unit and integration tests. Such tests are typically
short-lived and predictable, so we write a test that is long-lived and
unpredictable to find problems that would go otherwise unnoticed.

The test proceeds as follows:

- Create a randomly-named topic to run the test.
- Create N randomly-named subscriptions to the test.
- The test will keep a pool of publishers and a pool of subscribers.
- As the test executes, it may randomly replace one of the publishers.
- As the test executes, it may randomly replace one of the subscribers.
  The subscription associated with this subscriber is selected at random.

After running for T seconds or capturing N samples the test stops.
