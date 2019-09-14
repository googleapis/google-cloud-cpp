# Running the read_object_stall_test.

This is a regression test for a problem


```bash
PROJECT_ID=.... # Your project

ZONE=...
REGION=...

gcloud beta container clusters create --project=${PROJECT_ID} \
      --zone=${ZONE} --machine-type=n1-highmem-2 --num-nodes=35 \
      --enable-stackdriver-kubernetes regression-tests
gcloud container clusters get-credentials --project ${PROJECT_ID} \
          --zone "${ZONE}" regression-tests

SA_NAME=storage-stall-regression-sa
gcloud iam service-accounts create --project=${PROJECT_ID} ${SA_NAME} \
    --display-name="Service account to run GCS C++ Stall Regression"
gcloud projects add-iam-policy-binding ${PROJECT_ID} \
      --member serviceAccount:${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com \
      --role roles/storage.admin


gcloud iam service-accounts keys create \
      --iam-account ${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com \
      /dev/shm/key.json

kubectl create secret generic service-account-key \
        --from-file=key.json=/dev/shm/key.json
```

Create source and target buckets:

```
export SRC_BUCKET_NAME=...
export DST_BUCKET_NAME=...
gsutil mb -s regional -l us-west1 gs://${SRC_BUCKET_NAME}
gsutil mb -s regional -l us-west1 gs://${DST_BUCKET_NAME}
```

Create a docker image to run the job:

```sh
gcloud builds submit \
     --substitutions=SHORT_SHA=$(git rev-parse --short HEAD) \
     --config=google/cloud/storage/tests/read_object_stall_regression/cloudbuild.yaml
```

You may need to fetch the k8s credentials:

```sh
gcloud container clusters get-credentials regression-tests \
    --zone us-west1-a
```

Start the k8s job:

```sh
./google/cloud/storage/tests/read_object_stall_regression/create-job.sh | \
    kubectl apply -f -
```
