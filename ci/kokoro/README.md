# Kokoro Configuration and Build scripts.

This directory contains the build scripts and configuration files
for Kokoro, Google's internal CI system for open source projects.

We use Kokoro because:

- It has more bandwidth than other CI systems supporting Open Source prjects,
  such as Travis.
- We can run Linux, Windows, and macOS builds from a single CI system.
- We can store secrets, such as key files or access tokens, and run integration
  builds against production.

The documentation for Kokoro is available to Googlers only (sorry) at go/kokoro,
there are extensive codelabs and detailed descriptions of the system there.

In brief, the Kokoro configuration is split in two:

1. A series of configuration files inside Google define what builds exist, and
   what resources these builds have access to.
   * These files are in a hierarchy that mirrors the ci/kokoro directory in this
     repo.
   * The `common.cfg` files are parsed first.
   * The `common.cfg` files are applied according to the directory hierarchy.
   * Finally any settings in `foobar.cfg` are applied.
1. A series of configuration files in the `ci/kokoro` directory further define
   the build configuration:
   * They define the build script for each build, though they are often common.
   * They define which of the resources *allowed* by the internal configuration
     are actually *used* by that build.

Somewhat unique to Kokoro one must define a separate *INTEGRATION* vs.
*PRESUBMIT* thus the duplication of configuration files for essentially the
same settings.

## Setup the Google Container Registry cache for Kokoro builds.

### Background

Most of our builds run in a Docker container inside the Kokoro build machines.
We use Docker to:

- Test with different toolchains from the ones provided by Kokoro.
- Test with the native toolchains in multiple distributions, to ensure the
  default experience works.
- Test our "README" and "INSTALL" instructions by running the builds with the
  minimal set of tools required to complete a build.
- To create more hermetic builds with respect to the Kokoro configuration.

One of the first steps in the build is to create the Docker image used by that
build. This ensures that changes made to the definitions of the images are
tested with any code changes that need them. But this means that the build can
be slowed down by the image creation step, and they often fail because the
image creation fails, even nothing has changed with the image itself. Moreover,
to create the image we need to download packages from multiple sources, this
can be slow or fail when the servers hosting packages for certain distros
experience an outage. Such outages can last hours, the usual approach of
"try N times with a backoff" do not work during these outages.

To minimize the impact of these outages, and to also speed up the builds a
little bit, we cache these images in Google Container Registry (GCR). Builds
triggered by GitHub pull requests download the cached image, then use that cache
to rebuild the image (which usually results in "nothing to rebuild") and then
run the build using the newly created image. Builds triggered by GitHub commits
to a branch (typically `main` or a release branch) only use the cache if
rebuilding the image from scratch fails. Such builds also push new versions of
the cached image to GCR.

### Instructions

The following instructions configure the GCR cache for a project, and create a
new service account with permissions to read and write to this cache. Pick a
project that will host the container image cache:

```console
$ PROJECT_ID=... # Most likely an existing project for builds.
```

We need to grant the service account full storage admin access to the bucket
that will contain the containers. To this end we need to create the
[bucket first](https://cloud.google.com/container-registry/docs/access-control):

```console
$ gcloud auth configure-docker
$ docker pull hello-world:latest
$ docker tag hello-world:latest gcr.io/${PROJECT_ID}/hello-world:latest
$ docker push gcr.io/${PROJECT_ID}/hello-world:latest
```

Create a service account to access the Google Container Registry:

```console
$ SA_NAME=kokoro-gcr-updater-sa
```

Then create the service account:

```console
$ gcloud iam service-accounts create --project ${PROJECT_ID} ${SA_NAME} \
    --display-name="Allows Kokoro builds to access GCR images."
$ FULL_SA_NAME="${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com"
```

Grant the service account `roles/storage.admin` permissions on the container
registry bucket.

```console
$ gsutil iam ch serviceAccount:${FULL_SA_NAME}:roles/storage.admin \
    gs://artifacts.${PROJECT_ID}.appspot.com/
```

Create new keys for this service account and download then to a temporary place:

```console
$ gcloud iam service-accounts keys create /dev/shm/gcr-service-account.json \
      --iam-account "${FULL_SA_NAME}"
```

Create a configuration file for the Kokoro script:

```bash
$ echo "PROJECT_ID=${PROJECT_ID}" >/dev/shm/gcr-configuration.sh
```

Copy the service account keys and configuration file to the location where we
keep Kokoro secrets (Google's internal KMS), then delete these files from your
workstation.
