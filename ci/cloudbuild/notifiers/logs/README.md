# Generate Log Index for External Developers


## Prerequisites

Verify the [docker tool][docker] is functional on your workstation:

```shell
docker run hello-world
# Output: Hello from Docker! and then some more informational messages.
```

Verify the [pack tool][pack-install] is functional on our workstation. These
instructions were tested with `v0.17.0`, although they should work with newer
versions. Some commands may not work with older versions.

```shell
pack version
# Output: a version number, e.g., 0.17.0+git-d9cb4e7.build-2045
```

## Create the Docker image

Compile the code in the `function/` directory into a Docker image:

```shell
GOOGLE_CLOUD_PROJECT=... # The project running the builds
pack build  --builder gcr.io/buildpacks/builder:latest \
     --env "GOOGLE_FUNCTION_SIGNATURE_TYPE=cloudevent" \
     --env "GOOGLE_FUNCTION_TARGET=index_build_logs" \
     --path "function" \
     "gcr.io/${GOOGLE_CLOUD_PROJECT}/index-build-logs"
```

## Deploy to Cloud Run

```shell
docker push "gcr.io/${GOOGLE_CLOUD_PROJECT}/index-build-logs:latest"

gcloud run deploy index-build-logs \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --image="gcr.io/${GOOGLE_CLOUD_PROJECT}/index-build-logs:latest" \
    --region="us-central1" \
    --platform="managed" \
    --no-allow-unauthenticated
     
gcloud beta eventarc triggers create index-build-logs-trigger \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --location="us-central1" \
    --destination-run-service="index-build-logs" \
    --destination-run-region="us-central1" \
    --transport-topic="cloud-builds" \
    --matching-criteria="type=google.cloud.pubsub.topic.v1.messagePublished"

# TODO(coryan) -- think about  --allow-unauthenticated
```

[docker]: https://docker.com/
[docker-install]: https://store.docker.com/search?type=edition&offering=community
[sudoless docker]: https://docs.docker.com/engine/install/linux-postinstall/
[pack-install]: https://buildpacks.io/docs/install-pack/
