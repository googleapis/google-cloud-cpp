# Google Cloud Storage Code Samples

This directory contains code snippets showing how to use the Google Cloud
Storage APIs.  These snippets are referenced from the Doxygen documentation.

## Compiling the Samples

These examples are compiled as part of the build for the Google Cloud C++
Libraries.  The instructions on how to compile the code are in the
[top-level README](../../../../README.md) file.

## Running the Samples

Follow the
[instructions](https://cloud.google.com/storage/docs/quickstart-gsutil)
to create a project, enable billing, and create a bucket using the `gsutil`
command-line tool.

Then you can get the metadata for a bucket using:

```bash
./storage_bucket_samples get-metadata gcp-public-data-landsat
```
