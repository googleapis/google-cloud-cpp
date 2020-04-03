# Continuous Integration Configuration Files

This directory contains files to configure the continuous integration scripts.
Notably, it defines the GCP resources used in integration tests. This document
describes how to create these resources.

## Bigtable

You need to create a Cloud Bigtable instance, typically you would use something
like:

```bash
cbt -project "${GOOGLE_CLOUD_PROJECT}" createinstance \
  test-instance "Instance for Integration Tests" test-instance-c1 \
  us-central1-a 1 HDD
```

You need to create a table for the quick start program:

```bash
cbt -project "${GOOGLE_CLOUD_PROJECT}" -instance test-instance \
    createtable quickstart families=cf1
cbt -project "${GOOGLE_CLOUD_PROJECT}" -instance test-instance \
    set quickstart "r1" "cf1:greeting=Hello"
```

## Storage

You need to create at least one bucket:

```bash
gsutil mb -p "${GOOGLE_CLOUD_PROJECT}" \
    "gs://${GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
```

<!-- TODO(#...) - document all the steps to create resources -->
