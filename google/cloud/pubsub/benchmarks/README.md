# Cloud Pub/Sub C++ Client Library Benchmarks

This directory contains end-to-end benchmarks for the Cloud Pub/Sub C++ client
library. The benchmarks execute experiments against the production environment.
You need a working Google Cloud Platform project.

## Throughput Experiment

This benchmark creates a publisher and/or subscriber and measures the throughput
for each. To simplify testing, the experiment can run both the subscriber and
publisher in the same application. For better isolation the experiment can be
configured to run only one of the roles. Typically the publisher and subscribers
will be running on different machines.

In the publisher role the benchmark creates a single `pubsub::Publisher` object,
and then creates a configurable number of threads to publish messages to this
object. A simple flow-control loop is used to avoid unbounded memory usage:
the publishing threads stop publishing if the number of pending messages (that
is, not acked by Cloud Pub/Sub) goes over some high watermark value. The threads
resume publishing once the number of pending messages goes below a low
watermark.

A separate thread reports the number of messages successfully published, the
estimated number of bytes, and the throughput in MB/s (not MiB/s). This report
is generated periodically (the period is configurable) in CSV form. At the end
of the benchmark the publisher reports how many messages failed to be published,
and how many times the high watermark ceiling was hit.

All the publisher batching options are configurable via the command-line.

In the subscriber role the benchmark creates a single `pubsub::Subscriber`
object and calls `Subscribe()` a configurable number of times on this object.
The handler in the benchmark simply counts the number of messages received.

A separate thread reports the number of messages received every so many seconds,
the estimated number of bytes, and the throughput in MB/s (not MiB/s). This
report is generated periodically (the period is configurable) in CSV form.

In both the publisher and subscriber role the benchmark will not stop until a
minimum number of samples is reported. Likewise, the benchmark will not stop
until a minimum running time has elapsed. However, the benchmark will stop
running if the number of collected samples is higher than a maximum **or** the
total running time is over a maximum. All these parameters are configurable via
command-line options.

The experiment can create a topic and subscription or receive their names from
the command-line. Typically integration tests will create the topic and
subscription while manual execution will use pre-existing Pub/Sub resources.

### Running the Experiment

The main audience for this guide are `google-cloud-cpp` developers that want to
reproduce the benchmark results. This document assumes that you are familiar
with the steps to compile the code in this repository, please check the
top-level [README](../../../../README.md) if necessary.

:warning: Running these benchmarks can easily exceed the free tier for Cloud
Pub/Sub, generating hundreds of dollars in charges may take only a few minutes.
Please consult the [pricing guide][pubsub-pricing].

[pubsub-pricing]: https://cloud.google.com/pubsub/pricing

In this example we assume that `GOOGLE_CLOUD_PROJECT` is an environment variable
set to the GCP project you want to use. We also assume you have compiled the
code and it is located in `$BINARY_DIR`.

#### Create the Cloud Pub/Sub Topic and Subscriptions

Before running the experiment setup a cloud topic and subscription. You only
need to do this once, you can run the experiment multiple times using the same
resources.

```sh
gcloud pubsub topics create --project=${GOOGLE_CLOUD_PROJECT} bench
gcloud pubsub subscriptions create --project=${GOOGLE_CLOUD_PROJECT} \
    --topic bench bench
```

#### Start the Publisher

You would typically run the publisher and subscriber on separate GCE instances,
but you can run this on any workstation. For best results use a regional
endpoint, in the same region as your instance:

```sh
ENDPOINT=us-central1-pubsub.googleapis.com  # example, change as needed
```

Then start the publisher, note that these settings far exceed the
[default quota][pubsub-quota] for a topic (which was 200MB/s when we wrote this
document):

[pubsub-quota]: https://cloud.google.com/pubsub/quotas#quotas

```sh
/usr/bin/time ${BINARY_DIR}/google/cloud/pubsub/benchmarks/throughput \
    --endpoint=${ENDPOINT} \
    --project-id=${GOOGLE_CLOUD_PROJECT} \
    --topic-id=bench \
    --subscription-id=bench \
    --maximum-runtime=24h \
    --publisher=true \
    --publisher-pending-lwm=224MiB \
    --publisher-pending-hwm=256MiB \
    --publisher-target-messages-per-second=0 \
    --minimum-runtime=1h \
    --iteration-duration=5m \
    --publisher-max-batch-bytes=10MB \
    --publisher-io-channels=16 \
    --publisher-io-threads=1 \
    --publisher-thread-count=12 \
    --payload-size=1KiB
```

#### Start the Subscriber

Start the subscriber, note that these settings far exceed the
[default quota][pubsub-quota] for a subscription (which was 200MB/s when we
wrote this document):

```sh
/usr/bin/time ${BINARY_DIR}/google/cloud/pubsub/benchmarks/throughput \
    --endpoint=${ENDPOINT} \
    --project-id=${GOOGLE_CLOUD_PROJECT} \
    --topic-id=bench \
    --subscription-id=bench \
    --maximum-runtime=24h \
    --subscriber=true \
    --subscriber-max-outstanding-messages=0 \
    --subscriber-max-outstanding-bytes=100MiB \
    --minimum-runtime=1h \
    --iteration-duration=5m \
    --subscriber-io-channels=32 \
    --subscriber-io-threads=4 \
    --subscriber-thread-count=128
```

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
