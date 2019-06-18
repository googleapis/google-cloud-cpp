# Setting up GKE Deployments for the Benchmarks.

`google-cloud-cpp` has a number of benchmarks that cannot be executed as part
of the CI builds because:

- They take too long to run (multiple hours).
- Their results require an analysis pipeline to determine if there is a problem.
- They need production resources (and therefore production credentials) to run.

Instead, we execute these benchmarks in a Google Kubernetes Engine (GKE)
cluster. Currently these benchmarks are manually started. Eventually we would
like to setup a continuous deployment (CD) pipeline using Google Cloud Build and
GKE.

## Creating the Docker image

The benchmarks run from a single Docker image. To build this image we use
Google Cloud Build:

```console
$ gcloud builds submit \
    --substitutions=SHORT_SHA=$(git rev-parse --short HEAD) \
    --config ci/benchmarks/cloudbuild.yaml .
```

## Automate the Docker image creation

Finally we create a trigger to automatically build this Docker image on each
commit to master:

```console
$ gcloud alpha builds triggers create github \
    --project=${PROJECT_ID} \
    --repo_name=google-cloud-cpp \
    --repo_owner=googleapis \
    --branch_pattern='master' \
    --build_config=ci/benchmarks/cloudbuild.yaml
```

## Setting up GKE and other configuration

Please read the README file for the [storage](storage/README.md) benchmarks for
more information on how to setup a GKE cluster and execute the benchmarks in
this cluster.
