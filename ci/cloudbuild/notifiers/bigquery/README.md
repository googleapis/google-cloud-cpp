This directory contains the config and `deploy.sh` script for use of the Cloud
Build BigQuery notifier. The high-level description of how this works (as I
understand it), is the following:

* We use Google Cloud Build (GCB) to run many of our builds. See
  https://pantheon.corp.google.com/cloud-build/dashboard?project=cloud-cpp-testing-resources
* GCB sends build status notifications to the Pub/Sub
  `projects/cloud-cpp-testing-resources/topics/cloud-builds` topic. See
  https://pantheon.corp.google.com/cloudpubsub/topic/detail/cloud-builds?project=cloud-cpp-testing-resources 
* We run the GCB BigQuery Notifier as a Cloud Run service, which subscribes to
  the build notifications and writes them to a BigQuery table.

The `bigquery.yaml` file is the config that we give to the GCB BigQuery
Notifier service. It is read from a GCS bucket, so we first copy the file there
before deploying the service. The steps to deploy are captured in the
`deploy.sh` script.

Links:
* https://cloud.google.com/build/docs/configuring-notifications/configure-bigquery
* https://github.com/GoogleCloudPlatform/cloud-build-notifiers/tree/master/bigquery
