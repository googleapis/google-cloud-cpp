# GCS C++ client integration test resources

To run the integration tests against the production environment one must setup
a number of resources. This document describes how to do so, assuming you have
an existing project in Google Cloud Platform.

## Setting up a Project

TODO(#2420) - Complete this section.

## Create a Service Account to run the integration tests.

First, create a new service account to execute the integration tests:

```console
AGENT_ACCOUNT=... # e.g. integration-tests-robot
gcloud --project=${PROJECT_ID} iam service-accounts create ${AGENT_ACCOUNT}
```

Grant the new service account account full access to Google Cloud Storage:

```
gcloud projects add-iam-policy-binding ${PROJECT_ID} \
  --member serviceAccount:${AGENT_ACCOUNT}@${PROJECT_ID}.iam.gserviceaccount.com \
  --role roles/storage.admin
```

Grant the new service account permissions to sign URLs, policy documents, and
other blobs:

```
gcloud projects add-iam-policy-binding ${PROJECT_ID} \
  --member serviceAccount:${AGENT_ACCOUNT}@${PROJECT_ID}.iam.gserviceaccount.com \
  --role roles/iam.serviceAccountTokenCreator
```

Download key files for the new service account:

```console
gcloud --project=${PROJECT_ID} iam service-accounts keys create \
    /dev/shm/service-account.p12 --key-file-type=p12 \
    --iam-account=${AGENT_ACCOUNT}@${PROJECT_ID}.iam.gserviceaccount.com
gcloud --project=${PROJECT_ID} iam service-accounts keys create \
    /dev/shm/service-account.json --key-file-type=json \
    --iam-account=${AGENT_ACCOUNT}@${PROJECT_ID}.iam.gserviceaccount.com
```

## Setting up a Bucket

TODO(#2420) - Complete this section.

## Setting up a Destination Bucket

TODO(#2420) - Complete this section.

## Select a Storage Region

TODO(#2420) - Complete this section.

## Setting up a Pub/Sub Topic

TODO(#2420) - Complete this section.

## Setting up a Customer-Managed Encryption Key

TODO(#2420) - Complete this section.

## Setting up a Service Account for HMAC Keys

TODO(#2420) - Complete this section.

## Setting up a Service Account for Signing Blobs

Set the `PROJECT_ID` environment variable to your test project:

```console
PROJECT_ID=... # Your test project
```

Configure `gcloud` (aka the "Google Cloud SDK") to use this project:

```console
gcloud config set core/project ${PROJECT_ID}
```

List the current service accounts:

```console
gcloud iam service-accounts list
```

Create the new service account:

```console
gcloud iam service-accounts create test-sign-blob
```

Grant the new service account permissions to sign blobs:

```console
gcloud projects add-iam-policy-binding ${PROJECT_ID} \
  --member serviceAccount:test-sign-blob@${PROJECT_ID}.iam.gserviceaccount.com \
  --role roles/iam.serviceAccountTokenCreator
```

If needed, grant your account the same permissions, this is needed even if your
account is the owner of the project:

```console
gcloud projects add-iam-policy-binding ${PROJECT_ID} \
  --member user:$(gcloud config get-value account) \
  --role roles/iam.serviceAccountTokenCreator
```

If you are using a service account to access GCP (as opposed to an authorized
user), then replace the previous command with:


```console
gcloud projects add-iam-policy-binding ${PROJECT_ID} \
  --member serviceAccount:$(gcloud config get-value account) \
  --role roles/iam.serviceAccountTokenCreator
```

Verify the service account can sign blobs:

```console
echo 'Test string to sign' >to-sign.txt
gcloud iam service-accounts sign-blob \
    --iam-account=test-sign-blob@${PROJECT_ID}.iam.gserviceaccount.com \
    to-sign.txt signed.txt
```

You can now use `test-sign-blob@${PROJECT_ID}.iam.gserviceaccount.com` to run
the `SignBlob()` tests.
