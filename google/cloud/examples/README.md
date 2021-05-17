# Google Cloud C++ Client Examples

This directory contains examples that combine two or more Google Cloud C++
client libraries.

## Configuring a Hello World (gRPC) Service

## Prerequisites

Verify the [docker tool][docker] is functional on your workstation:

```shell
docker run hello-world
# Output: Hello from Docker! and then some more informational messages.
```

### Create the Docker image

We use Docker to create an image with our "Greeter" service:

```shell
docker build -t "gcr.io/${GOOGLE_CLOUD_PROJECT}/hello-world-grpc:latest" \
    -f google/cloud/examples/hello_world_grpc/Dockerfile \
    google/cloud/examples/hello_world_grpc
```

### Push the Docker image to GCR

```shell
docker push "gcr.io/${GOOGLE_CLOUD_PROJECT}/hello-world-grpc:latest"
```

### Deploy to Cloud Run

```shell
SA="hello-world-run-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
gcloud run deploy hello-world-grpc \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --image="gcr.io/${GOOGLE_CLOUD_PROJECT}/hello-world-grpc:latest" \
    --region="us-central1" \
    --platform="managed" \
    --service-account="${SA}" \
    --ingress=all \
    --no-allow-unauthenticated
```

### Getting the URL

```shell
GOOGLE_CLOUD_CPP_TEST_HELLO_WORLD_GRPC_URL="$(gcloud run services describe \
    hello-world-grpc \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --region="us-central1" --platform="managed" \
    --format='value(status.url)')"
```


## Configuring Hello World (HTTP) Service

The `grpc_credential_types` example uses IAM to obtain an *Identity Token*, and
then uses this token to authenticate a request. Identity Tokens are used by
**user** applications deployed to certain GCP environments, including Cloud Run
and Cloud Functions.

To run integration tests and to demonstrate how these tokens are used we deploy
a simple "Hello World" application to Cloud Run. The code for this application
is found in the `hello_world_http/` subdirectory.

### Prerequisites

Verify the [docker tool][docker] is functional on your workstation:

```shell
docker run hello-world
# Output: Hello from Docker! and then some more informational messages.
```

Verify the [pack tool][pack-install] is functional on your workstation. These
instructions were tested with `v0.17.0`, although they should work with newer
versions. Some commands may not work with older versions.

```shell
pack version
# Output: a version number, e.g., 0.17.0+git-d9cb4e7.build-2045
```

### Create the Docker image

We use buildpack to compile the code in the `hello_world_http/` subdirectory
into a Docker image. At the end of this build the docker image will reside on
your workstation. We will then push the image to GCR (Google Container Registry)
and use it from Cloud Run.

```shell
pack build  --builder gcr.io/buildpacks/builder:latest \
     --env "GOOGLE_FUNCTION_SIGNATURE_TYPE=http" \
     --env "GOOGLE_FUNCTION_TARGET=HelloWorldHttp" \
     --path "google/cloud/examples/hello_world_http/" \
     "gcr.io/${GOOGLE_CLOUD_PROJECT}/hello-world-http"
```

### Push the Docker image to GCR

```shell
docker push "gcr.io/${GOOGLE_CLOUD_PROJECT}/hello-world-http:latest"
```

### Deploy to Cloud Run

```shell
SA="hello-world-run-sa@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
gcloud run deploy hello-world-http \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --image="gcr.io/${GOOGLE_CLOUD_PROJECT}/hello-world-http:latest" \
    --region="us-central1" \
    --platform="managed" \
    --service-account="${SA}" \
    --ingress=all \
    --no-allow-unauthenticated
```

### Getting the URL

```shell
GOOGLE_CLOUD_CPP_TEST_HELLO_WORLD_HTTP_URL="$(gcloud run services describe \
    hello-world-http \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --region="us-central1" --platform="managed" \
    --format='value(status.url)')"
```

### One Time: Create Service Account and set Permissions

```shell
# The account in the Hello World service, has no permissions
gcloud iam service-accounts create hello-world-run-sa \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --display-name="Service account used by Cloud Run 'Hello World' Deployments"

# The account that calls the Hello World service, only has that permission
gcloud iam service-accounts create hello-world-caller \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --display-name="Service account used to *invoke* Hello World"
gcloud projects add-iam-policy-binding "${GOOGLE_CLOUD_PROJECT}" \
    --member="serviceAccount:hello-world-caller@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com" \
    --role="roles/run.invoker"

# Grant the account used in the CI builds permissions to impersonate this new
# account and to fetch the URL for the Hello World service. For more background
# see go/cloud-cxx:gcb-roles for details
PROJECT_NUMBER=$(gcloud projects list \
    --filter="project_id=${GOOGLE_CLOUD_PROJECT}" \
    --format="value(project_number)" \
    --limit=1)
gcloud iam service-accounts add-iam-policy-binding \
    "hello-world-caller@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com" \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --member="serviceAccount:kokoro-run@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com" \
    --role="roles/iam.serviceAccountTokenCreator"
gcloud iam service-accounts add-iam-policy-binding \
    "hello-world-caller@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com" \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --member="serviceAccount:${PROJECT_NUMBER}@cloudbuild.gserviceaccount.com" \
    --role="roles/iam.serviceAccountTokenCreator"

gcloud projects add-iam-policy-binding "${GOOGLE_CLOUD_PROJECT}" \
    --member="serviceAccount:kokoro-run@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com" \
    --role="roles/run.viewer"
gcloud projects add-iam-policy-binding "${GOOGLE_CLOUD_PROJECT}" \
    --member="serviceAccount:${PROJECT_NUMBER}@cloudbuild.gserviceaccount.com" \
    --role="roles/run.viewer"
```
