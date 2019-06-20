# Setting up GKE Deployments for the Bigtable Benchmarks.

This document describes how to setup GKE deployments to continuously run the
Cloud Bigtable C++ client benchmarks. Please see the general
[README](../README.md) about benchmarks for the motivations behind this design.

## Creating the Docker image

The benchmarks run from a single Docker image. This image is automatically built
by Google Cloud Build on each commit to `master`, but if you need to manually
build it consult the [../README.md](../README.md).

## Creating the Bigtable Benchmarks Cluster and Instance

We run all the benchmarks in a single GKE cluster, we want the GKE cluster and
the bigtable instances used by the tests to be in the same zone. Select a
project and zone to run the benchmarks:

```console
$ PROJECT_ID=... # The name of your project
                 # e.g., PROJECT_ID=$(gcloud config get-value project)
$ BIGTABLE_BENCHMARKS_ZONE=... # e.g. us-central1-f
```

Create the Bigtable instances.

```console
$ cbt -project "${PROJECT_ID}" createinstance \
    bm-endurance-instance "Test Endurance" bm-endurance-instance-c1 \
    "${BIGTABLE_BENCHMARKS_ZONE}" 3 HDD
$ cbt -project "${PROJECT_ID}" createinstance \
    bm-latency-instance "Test Latency" bm-latency-instance-c1 \
    "${BIGTABLE_BENCHMARKS_ZONE}" 3 HDD
$ cbt -project "${PROJECT_ID}" createinstance \
    bm-throughput-instance "Test Throughput" bm-throughput-instance-c1 \
    "${BIGTABLE_BENCHMARKS_ZONE}" 3 HDD
$ cbt -project "${PROJECT_ID}" createinstance \
    bm-scan-instance "Test Scant Throughput" bm-scan-instance-c1 \
    "${BIGTABLE_BENCHMARKS_ZONE}" 3 HDD
```

Create the GKE clusters. All the performance benchmarks run in one cluster,
because the work well with a single CPU for each. The endurance benchmark needs
multiple cores. We dedicate a cluster with larger virtual machines for it/

```console
$ gcloud beta container clusters create --project=${PROJECT_ID} \
      --zone=${BIGTABLE_BENCHMARKS_ZONE} --num-nodes=5 \
      --machine-type=n1-standard-4 --enable-stackdriver-kubernetes \
      bigtable-benchmarks-cluster
```

Pick a name for the service account, for example:

```console
$ SA_NAME=bigtable-benchmark-sa
```

Then create the service account:

```console
$ gcloud iam service-accounts create --project=${PROJECT_ID} ${SA_NAME} \
    --display-name="Service account to run Bigtable Benchmarks"
```

Create a custom role to define the permissions of this service account. It
basically needs the permissions for `roles/bigtable.user` and the permissions
to create and delete tables:

```console
$ gcloud iam roles copy "--dest-project=${PROJECT_ID}" \
    --source=roles/bigtable.user --destination=bigtable.benchmark.runner
$ gcloud iam roles "--project=${PROJECT_ID}" update bigtable.benchmark.runner \
    --add-permissions=bigtable.tables.create,bigtable.tables.delete \
    --stage=GA
```

Grant the service account these permissions:

```console
$ gcloud projects add-iam-policy-binding ${PROJECT_ID} \
      --member serviceAccount:${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com \
      --role projects/${PROJECT_ID}/roles/bigtable.benchmark.runner
```

Create a new key for this service account and download then to a temporary
place:

```console
$ gcloud iam service-accounts keys create /dev/shm/key.json \
      --iam-account ${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com
```

Copy the key to each of the clusters:

```console
$ kubectl create secret generic service-account-key \
        --from-file=key.json=/dev/shm/key.json
```

And then remove it from your machine:

```bash
$ rm /dev/shm/key.json
```

## Create the BigQuery Dataset for the logs

```console
$ bq mk --project=${PROJECT_ID} bigtable_benchmarks_raw_logs
```

## Starting the Endurance Benchmark

The endurance benchmark (which is not really a benchmark) verifies that the
library can work continuously for many hours. It creates a moderately large
table (approximately 10,000,000 rows), and then continuously reads or writes
random rows from the table.

```console
$ sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
    ci/benchmarks/bigtable/endurance-job.yaml | kubectl apply -f -
```

## Starting the Latency Benchmark

This benchmark measures the latency for both single row reads and single row
write operations. It creates a moderately large table (approximately 10,000,000
rows), and then continuously reads or writes random rows from the table.

```console
$ sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
    ci/benchmarks/bigtable/latency-job.yaml | kubectl apply -f -
```

## Starting the Throughput Benchmark

This is a different configuration of the latency benchmark. The intention is to
saturate the CPU and measure the latency in those conditions:

```console
$ sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
    ci/benchmarks/bigtable/throughput-job.yaml | kubectl apply -f -
```

## Starting the Scan Throughput Benchmark

This benchmark measures the throughput of performing scans (aka `ReadRows()`)
of different sizes.

```console
$ sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
    ci/benchmarks/bigtable/scan-job.yaml | kubectl apply -f -
```

## Create log sinks for the Bigtable Benchmarks

```console
$ for container in bigtable-endurance-test \
    bigtable-latency-benchmark bigtable-scan-benchmark \
    bigtable-throughput-benchmark; do
  cat >filter.txt <<_EOF_
resource.type="k8s_container"
resource.labels.container_name="${container}"
resource.labels.cluster_name="bigtable-benchmarks-cluster"
resource.labels.project_id="${PROJECT_ID}"
_EOF_
  SINK_DEST="projects/${PROJECT_ID}/datasets/bigtable_benchmarks_raw_logs"
  gcloud logging sinks create  --project=${PROJECT_ID} ${container}-logs \
    "bigquery.googleapis.com/${SINK_DEST}" \
    --log-filter="$(cat filter.txt)"
  echo gcloud projects add-iam-policy-binding ${PROJECT_ID} \
        --role roles/bigquery.dataEditor \
        --member "serviceAccount:<TODO-CAPTURE-AUTOMATICALLY>@gcp-sa-logging.iam.gserviceaccount.com"
 done
```
