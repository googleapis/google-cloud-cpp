# Setting up GKE Deployments for the GCS C++ Client Benchmarks.

This document describes how to setup GKE deployments to continuously run the
Google Cloud Storage C++ client benchmarks. Please see the general
[README](../README.md) about benchmarks for the motivations behind this design.

## Creating the Docker image

The benchmarks run from a single Docker image. This image is automatically built
by Google Cloud Build on each commit to `master`, but if you need to manually
build it consult the [../README.md](../README.md).

## Create a GKE cluster for the Storage Benchmarks

Select the project and zone where you will run the benchmarks.

```console
$ PROJECT_ID=... # The name of your project
                 # e.g., PROJECT_ID=$(gcloud config get-value project)
$ STORAGE_BENCHMARKS_ZONE=... # e.g. us-central1-a
$ STORAGE_BENCHMARKS_REGION="$(gcloud compute zones list \
    "--filter=(name=${STORAGE_BENCHMARKS_ZONE})" \
    "--format=csv[no-heading](region)")"
```

Create the GKE clusters. All the performance benchmarks run in one cluster,
because the work well with a single CPU for each.

```console
$ gcloud beta container clusters create --project=${PROJECT_ID} \
      --zone=${STORAGE_BENCHMARKS_ZONE} --num-nodes=30 \
      --enable-stackdriver-kubernetes storage-benchmarks-cluster
```

Pick a name for the service account, for example:

```console
$ SA_NAME=storage-benchmark-sa
```

Then create the service account:

```console
$ gcloud iam service-accounts create --project=${PROJECT_ID} ${SA_NAME} \
    --display-name="Service account to run GCS C++ Client Benchmarks"
```

Grant the service account `roles/storage.admin` permissions. The benchmarks
create and delete their own buckets. This is the only role that grants these
permissions, and incidentally also grants permission to read, write, and delete
objects:

```console
$ gcloud projects add-iam-policy-binding ${PROJECT_ID} \
      --member serviceAccount:${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com \
      --role roles/storage.admin
```

Create new keys for this service account and download then to a temporary place:

```console
$ gcloud iam service-accounts keys create /dev/shm/key.json \
      --iam-account ${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com
```

Copy the key to the GKE cluster:

```console
$ gcloud container clusters get-credentials --project ${PROJECT_ID} \
          --zone "${STORAGE_BENCHMARKS_ZONE}" storage-benchmarks-cluster
$ kubectl create secret generic service-account-key \
        --from-file=key.json=/dev/shm/key.json
```

And then remove it from your machine:

```console
$ rm /dev/shm/key.json
```

### Starting the Throughput vs. CPU Benchmark

The throughput vs. CPU benchmark measures the effective upload and download
throughput for different object sizes, as well as the CPU required to achieve
this throughput. In principle, we want the client libraries to achieve high
throughput with minimal CPU utilization.

The benchmark uploads an object of random size, measures the throughput and CPU
usage, then downloads the same object and measures the throughput and CPU
utilization. The results are reported to the standard output as a CSV file.

Start the deployment:

```console
$ sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
      -e "s/@REGION@/${STORAGE_BENCHMARKS_REGION}/" \
    ci/benchmarks/storage/throughput-vs-cpu-job.yaml | kubectl apply -f -
```

To restart the deployment

```console
$ sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
      -e "s/@REGION@/${STORAGE_BENCHMARKS_REGION}/" \
    ci/benchmarks/storage/throughput-vs-cpu-job.yaml | \
  kubectl replace --force -f -
```

## Create the BigQuery Dataset for the logs

 ```console
$ bq mk --project=${PROJECT_ID} storage_benchmarks_raw_logs
```

## Create log sinks for the Storage Benchmarks

 ```console
$ for container in storage-throughput-vs-cpu; do
  cat >filter.txt <<_EOF_
resource.type="k8s_container"
resource.labels.container_name="${container}"
resource.labels.cluster_name="storage-benchmarks-cluster"
resource.labels.project_id="${PROJECT_ID}"
_EOF_
  SINK_DEST="projects/${PROJECT_ID}/datasets/storage_benchmarks_raw_logs"
  gcloud logging sinks create  --project=${PROJECT_ID} ${container}-logs \
    "bigquery.googleapis.com/${SINK_DEST}" \
    --log-filter="$(cat filter.txt)"
 done
```

You need to manually grant the "BigQuery Data Editor" role to a service account,
as described in the program output. Navigate to
`http://console.cloud.google.com/bigquery`, find the data set, client on
"SHARE DATASET" and set the right permissions.
