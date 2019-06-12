# Setting up GKE Deployments for the GCS C++ Client Benchmarks.

This document describes how to setup GKE deployments to continuously run the
Google Cloud Storage C++ client benchmarks. Please see the general
[README](../README.md) about benchmarks for the motivations behind this design.

## Creating the Docker image

The benchmarks run from a single Docker image. To build this image we use
Google Cloud Build:

```console
$ cd $HOME/google-cloud-cpp
$ gcloud builds submit \
     --substitutions=SHORT_SHA=$(git rev-parse --short HEAD) \
     --config cloudbuild.yaml .
```

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
$ gcloud container clusters create --zone=${STORAGE_BENCHMARKS_ZONE} \
      --num-nodes=30 storage-benchmarks-cluster
```

Pick a name for the service account, for example:

```console
$ SA_NAME=storage-benchmark-sa
```

Then create the service account:

```console
$ gcloud iam service-accounts create ${SA_NAME} \
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
$ gcloud container clusters get-credentials \
          --zone "${STORAGE_BENCHMARKS_ZONE}" storage-benchmarks-cluster
$ kubectl create secret generic service-account-key \
        --from-file=key.json=/dev/shm/key.json
```

And then remove it from your machine:

```bash
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

Create a bucket to store the benchmark results:

```console
$ LOGGING_BUCKET=... # e.g. ${PROJECT_ID}-benchmark-logs
$ gsutil mb -l us gs://${LOGGING_BUCKET}
```

Start the deployment:

```console
$ gcloud container clusters get-credentials \
    --zone "${STORAGE_BENCHMARKS_ZONE}" storage-benchmarks-cluster
$ VERSION=$(git rev-parse --short HEAD) # The image version.
$ sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
      -e "s/@LOGGING_BUCKET@/${LOGGING_BUCKET}/" \
      -e "s/@REGION@/${STORAGE_BENCHMARKS_REGION}/" \
      -e "s/@VERSION@/${VERSION}/" \
    ci/benchmarks/storage/throughput-vs-cpu-job.yaml | kubectl apply -f -
```

To upgrade the deployment to a new image:

```console
$ VERSION= ... # New version
$ kubectl set image deployment/storage-throughput-vs-cpu \
    "benchmark-image=gcr.io/${PROJECT_ID}/google-cloud-cpp-benchmarks:${VERSION}"
$ kubectl rollout status -w deployment/storage-throughput-vs-cpu
```

To restart the deployment

```console
$ sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
      -e "s/@LOGGING_BUCKET@/${LOGGING_BUCKET}/" \
      -e "s/@REGION@/${STORAGE_BENCHMARKS_REGION}/" \
      -e "s/@VERSION@/${VERSION}/" \
    ci/benchmarks/storage/throughput-vs-cpu-job.yaml | \
  kubectl replace --force -f -
```

## Analyze Throughput-vs-CPU results.

If you haven't already, create a BigQuery dataset to hold the data:

```console
$ bq mk --dataset  \
     --description "Holds data and results from GCS C++ Client Library Benchmarks" \
     "${PROJECT_ID}:storage_benchmarks"
```

Create a table in this dataset to hold the benchmark results:

```console
$ TABLE_COLUMNS=(
    "op:STRING"
    "object_size:INT64"
    "chunk_size:INT64"
    "buffer_size:INT64"
    "elapsed_us:INT64"
    "cpu_us:INT64"
    "status:STRING"
    "version:STRING"
)
$ printf -v schema ",%s" "${TABLE_COLUMNS[@]}"
$ schema=${schema:1}
$ bq mk --table --description "Raw Data from throughput-vs-cpu benchmark" \
    "${PROJECT_ID}:storage_benchmarks.throughput_vs_cpu_data" \
    "${schema}"
```

Make a list of all the objects:

```console
$ objects=$(gsutil ls gs://${PROJECT_ID}-benchmark-logs/throughput-vs-cpu/ | wc -l)
$ max_errors=$(( ${objects} * 2 ))
```

Upload them to BigQuery:

```console
$ gsutil ls gs://${PROJECT_ID}-benchmark-logs/throughput-vs-cpu/ | \
  xargs -I{} bq load --noreplace --skip_leading_rows=16 --max_bad_records=2 \
       "${PROJECT_ID}:storage_benchmarks.throughput_vs_cpu_data" {}
```
