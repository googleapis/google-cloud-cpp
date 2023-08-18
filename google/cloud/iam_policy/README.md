# IAM Policy Proto Libraries

Several Google Cloud Platform services use the IAM Policy APIs and protocol
buffer definitions. This directory contains CMake targets to generate the C++
libraries corresponding to this code. Customers are not expected to use these
libraries directly (though they might), instead we recommend using the idiomatic
C++ libraries for the GCP services.

Please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).
