# HOWTO: Deploy the GCS metadata indexer

## Prerequisites

Install the command-line tools:

```bash
gcloud components install beta
gcloud components install docker-credential-gcr
gcloud auth configure-docker
```

## Bootstrap the Indexer

Pick the project to run the service and the spanner instance.

```bash
export GOOGLE_CLOUD_PROJECT=...
```

Run the program to bootstrap the indexer, this will create the Cloud Spanner
instance and databases to store the data, the Cloud Run deployments to run the
indexer, two service accounts to run the Cloud Run deployments, and a Cloud
Pub/Sub topic to receive GCS metadata updates. It will also setup the IAM
permissions so Cloud Pub/Sub can push these updates to Cloud Run.

```bash
./bootstrap-gcs-indexer.sh
```

## Index an existing Bucket

Then connect the program to one of the buckets in this project. This will
configure the bucket to publish any metadata updates to the topic created above,
and start a program to index the existing metadata in the bucket.

```bash
./index-bucket my-bucket-name
```

Note that this program is not ready to scale to very large numbers of metadata
entries.
