# GCS C++ client integration test resources

To run the integration tests against the production environment one must setup a
number of resources. This document describes how to do so, assuming you have an
existing project in Google Cloud Platform.

## Setting up a Project

Set the `PROJECT_ID` environment variable to your test project:

```console
PROJECT_ID=... # Your test project
```

Create the project:

```console
gcloud projects create ${PROJECT_ID}
```

Configure `gcloud` (aka the "Google Cloud SDK") to use this project:

```console
gcloud config set core/project ${PROJECT_ID}
```

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

Select a region that's close to your location. The
[documentation](https://cloud.google.com/storage/docs/locations) has the full
list of allowed locations. In this example, we'll use `us-central1`.

```console
BUCKET=... # e.g. cloud-cpp-testing-bucket
# Gsutil command could not be translated. Reason: The top-level flag "-l" is not a valid gsutil flag and is not found in the migration guide.
gsutil -l us-central1 gs://${BUCKET}
```

## Setting up a Destination Bucket

For testing purposes, we'll use a bucket that's in a different region from our
initial bucket.

```console
DESTINATION_BUCKET=... # e.g. cloud-cpp-testing-destination-bucket
gcloud storage buckets create gs://${DESTINATION_BUCKET} --location=us-east1
```

## Setting up a Pub/Sub Topic

Create the topic:

```console
gcloud pubsub topics create cloud-cpp-testing-topic
```

## Setting up a Customer-Managed Encryption Key

First, create the keyring and key:

```
KR=... # e.g cloud-cpp-testing-keyring
KEYID=... # e.g cloud-cpp-testing-key
gcloud kms keyrings create ${KR} --location=global
gcloud kms keys create ${KEYID} --purpose=encryption --location=global --keyring=${KR}
gcloud kms keys list --location=global --keyring=${KR}
```

Authorize our service account to use the new key:

```
KEYNAME=projects/${PROJECT_NAME}/locations/global/keyRings/${KR}/cryptoKeys/${KEYID}
gcloud storage service-agent --project ${PROJECT_NAME} --authorize-cmek=${KEYNAME}
```

Configure the bucket to use this new key as the default:

```
gcloud storage buckets update gs://${BUCKET} --default-encryption-key=${KEYNAME}
```

## Setting up a Service Account for HMAC Keys

List the current service accounts:

```console
gcloud iam service-accounts list
```

Create the new service account:

```console
gcloud iam service-accounts create test-hmac-key
```

Grant the service account permissions to manage HMAC keys:

```console
gcloud projects add-iam-policy-binding ${PROJECT_ID} \
  --member serviceAccount:test-hmac-key@${PROJECT_ID}.iam.gserviceaccount.com \
  --role roles/iam.hmacKeys
```

## Setting up a Service Account for Signing Blobs

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
