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


gsutil mb -s regional -l us-west1 gs://${SRC_BUCKET_NAME}
gsutil mb -s regional -l us-west1 gs://${DST_BUCKET_NAME}

sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
    -e "s/@ID@/$(date +%s)-${RANDOM}/" \
    -e "s/@SRC_BUCKET_NAME@/${SRC_BUCKET_NAME}/" \
    -e "s/@DST_BUCKET_NAME@/${DST_BUCKET_NAME}/" \
    google/cloud/storage/tests/read_object_stall_regression/k8s.yaml | kubectl apply -f -

```
