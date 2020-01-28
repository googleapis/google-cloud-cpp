# Cloud Run for C++

This repository contains an example showing how to deploy C++ applications in
Google Cloud Run.

## Prerequisites

Install the command-line tools:

```bash
gcloud components install beta
gcloud components install kubectl
gcloud components install docker-credential-gcr
gcloud auth configure-docker
```

## Start the Cloud Run Deployment

Pick the project to run the Cloud Run Deployment.

```bash
export GOOGLE_CLOUD_PROJECT=...
```

This script will enable the necessary APIs, build a Docker image using Cloud
Build, create a service account for the Cloud Run deployment, and then create
a Cloud Run deployment using the Docker image referenced earlier.

```bash
cd google/cloud/examples/cloud_run_hello
./bootstrap-cloud-run-hello.sh
```

## Send a HTTP GET request to the server.

First capture the URL for the Cloud Run service:

```bash
SERVICE_URL=$(gcloud beta run services list \
    "--project=${GOOGLE_CLOUD_PROJECT}" \
    "--platform=managed" \
    '--format=csv[no-heading](URL)' \
    "--filter=SERVICE:cloud-run-hello")
```

Then send a request using `curl`:

```bash
curl -H "Authorization: Bearer $(gcloud auth print-identity-token)" \
  "${SERVICE_URL}"
```
