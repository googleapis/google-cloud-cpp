# Cloud Run for C++

This repository contains an example showing how to deploy C++ applications in
Google Cloud Run.

## Prerequisites

Install the command-line tools:

```bash
gcloud components install beta
gcloud components install docker-credential-gcr
gcloud auth configure-docker
```

## Start the Cloud Run Deployment

Pick the project to run the Cloud Run Deployment.

```bash
export GOOGLE_CLOUD_PROJECT=...
```

This script will enable the necessary APIs, build the docker image used by
Cloud Run using Cloud Build, create a service account for the Cloud Run
deployment, 
Run the program to bootstrap the indexer, this will create the Cloud Spanner
instance and databases to store the data, the Cloud Run deployments to run the
indexer, two service accounts to run the Cloud Run deployments, and a Cloud
Pub/Sub topic to receive GCS metadata updates. It will also setup the IAM
permissions so Cloud Pub/Sub can push these updates to Cloud Run.

```bash
cd google/cloud/examples/cloud_run_hello
./bootstrap-cloud-run-hello.sh
```

# Send a HTTP GET request to the server.
curl -H "Authorization: Bearer $(gcloud auth print-identity-token)" \
  "${SERVICE_URL}"

