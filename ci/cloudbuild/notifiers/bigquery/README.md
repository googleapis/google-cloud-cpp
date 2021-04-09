# Configuration files for Cloud Build BigQuery notifier

Sometimes we need to analyze our build results, looking for patterns of build
latency, flakiness, etc. These analyses often require looking at the results
over long periods of time, so the usual dashboards and squinting may not work.
Putting the results in some sort of database works better for that purpose.
The Cloud Build BigQuery notifier allows us to store build results in BigQuery.

This directory contains files needed to use the Cloud Build BigQuery notifier.
The high-level description of how this works (as I understand it) is the
following:

* We use Google Cloud Build (GCB) to run many of our builds. See
  https://console.cloud.google.com/cloud-build/dashboard?project=cloud-cpp-testing-resources
* GCB sends build status notifications to the Cloud Pub/Sub
  `projects/cloud-cpp-testing-resources/topics/cloud-builds` topic. See
  https://console.cloud.google.com/cloudpubsub/topic/detail/cloud-builds?project=cloud-cpp-testing-resources
* We run the GCB BigQuery Notifier as a Cloud Run service, which subscribes to
  the build notifications and writes them to a BigQuery table.

The `bigquery.yaml` file is the config that we give to the GCB BigQuery
Notifier service. It is read from a GCS bucket, so we first copy the file there
before deploying the service. The steps to deploy are captured in the
`deploy.sh` script.

Links:
* https://cloud.google.com/build/docs/configuring-notifications/configure-bigquery
* https://github.com/GoogleCloudPlatform/cloud-build-notifiers/tree/master/bigquery
