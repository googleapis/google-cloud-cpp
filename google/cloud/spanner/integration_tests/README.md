# Cloud Spanner C++ Client integration tests.

This directory contains integration tests for the Cloud Spanner C++ client
library. These tests are written using the Google Test framework, but can only
execute when the environment is properly configured.

## Google Cloud Platform Project

The Google Cloud Platform project for the tests must be configured using the
`GOOGLE_CLOUD_PROJECT` environment variable.

## Credentials

The environment must be configured such that `grpc::GoogleDefaultCredentials`
returns a valid credential with `roles/spanner.admin` permissions on the project
configured via the `GOOGLE_CLOUD_PROJECT` environment variable.

## Default Cloud Spanner Instance

The `GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID` environment variable must be the id of a
Cloud Spanner instance in the project. This instance is used by most tests, as
it saves them from creating a new instance for just a few operations.

## Test Service Account

The `GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT` environment variable must be the id
of a Google Cloud Platform service account in the project. This service account
is used to test IAM operations, the account is added to and removed from
standard spanner roles.
