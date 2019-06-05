# Reproduce deadlocks in C++ Storage Client.

This document how to run the `object_write_deadlock_regression_test` at scale,
to reproduce a rare deadlock in the Google Cloud Storage (GCS) C++ client
library.

## Overview

We will run the test using Google Kubernetes Engine ([GKE][gke-link]). Using
this service we can run the same program in many Google Compute Engine (GCE)
virtual machines. To take advantage of the service we need to:

- Create a Docker image containing the program we want to execute.
- Create a GKE cluster to run this program.
- Grant the programs in the cluster permission to access GCS.
- Start the program in the cluster.
- Collect any results (we are interested in the failures).
- Analyze the results.

## Pre-requisites

You need a project with GCS and GKE enabled, follow the
[GKE Quickstart][gke-quickstart] and the [GCS Quickstart][gcs-quickstart]
guides.

Define the `PROJECT_ID` environment variable to contain your project, for
example:

```bash
$ export PROJECT_ID=my-test-project
```

## Creating a Docker image for the test program

This directory contains a Docker file to compile the test program and create a
(much smaller) image with the program.

First, create the VERSION environment variable to define the version number for
this image. As you modify the program you will want to increase this version
number, because it is easy to "upgrade" jobs in k8s using larger version
numbers:

```bash
export VERSION=1
```

Run the following command from the top-level directory of this project:

```bash
$ ./google/cloud/storage/tests/write_deadlock_regression/create-image.sh \
    ${VERSION}
```

Verify the image is created:

```bash
$ docker image ls gcr.io/${PROJECT_ID}/storage-deadlock-regression
```

Verify the program inside the image is executable:

```bash
$ docker run gcr.io/${PROJECT_ID}/storage-deadlock-regression:${VERSION} \
    /r/object_write_deadlock_regression_test
```

## Create the Kubernetes Cluster

Select a zone to run your cluster and then create it.

```bash
$ gcloud container clusters create --zone=us-central1-f repro-workers-01
```

## Create a service account to run the test

Pick a name for the service account, for example:

```bash
$ export SA_NAME=deadlock-repro-sa
```

Then create the service account:

```bash
$ gcloud iam service-accounts create ${SA_NAME} \
    --display-name="Service account to repro deadlock"
```

Grant the service account admin permissions on GCS, we do not need them, but
this is the easiest:

```bash
$ gcloud projects add-iam-policy-binding ${PROJECT_ID} \
  --member serviceAccount:${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com \
  --role roles/storage.admin
```

Create a new key for this service account and download it to a safe place:

```bash
$ gcloud iam service-accounts keys create /dev/shm/key.json \
      --iam-account ${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com
```

Copy the key to the cluster:

```bash
$ kubectl create secret generic storage-key \
    --from-file=key.json=/dev/shm/key.json
```

And then remove it from your machine:

```bash
$ rm /dev/shm/key.json
```

## Startup the program

This directory contains a "almost-ready" YAML file

```bash
$ sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
      -e "s/@BUCKET_NAME@/${BUCKET_NAME}/" \
      -e "s/@VERSION@/${VERSION}/" \
    google/cloud/storage/write_deadlock_regression/k8s.yaml | \
    kubectl apply -f -
```

## Grow the cluster to run more copies

```bash
$ gcloud container clusters resize repro-workers-01 --zone=us-central1-f \
    --num-nodes=30
```



## Parsing the output

Download the logs from Stackdriver for any errors.

```console
docker run --rm -it
    --volume=$PWD/logfile.csv:/l/test.log
    gcr.io/coryan-test/storage-deadlock-regression:latest \
    /bin/bash
> apt update && apt install -y binutils
$ egrep -o '^".*\[0x[0-9a-f]+\]' /l/test.log | \
  sed -n 's/.*(+\(0x[0-9a-f][0-9a-f]*\)).*/\1/p' | \
    addr2line --demangle --functions --inlines --pretty-print \
      --exe=/r/object_write_deadlock_regression_test
```

[gke-link]: https://cloud.google.com/kubernetes-engine/
[gke-quickstart]: https://cloud.google.com/kubernetes-engine/docs/quickstart
[gcs-quickstart]: https://cloud.google.com/storage/docs/quickstart-gsutil
