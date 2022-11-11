**Title**: clients for multiple versions of a GCP service exist in one library.

**Status**: accepted

**Context**:
The current directory and namespace structure does not support the addition of
new versions of a service. We need to change the libraries to fix this.

Note that we are talking about different versions of the GCP service. That is,
the version that would appear in that service's API reference docs. We are not
talking about versions of the google-cloud-cpp client library.

For example, at the moment of writing this, the `google::cloud::bigquery`
library contains several services. These services have independent versions
(some are v1, some are v2). If a new version of one of these services were
released, we would not be able to place the types and functions for it in the
`google::cloud::bigquery` namespace, as the new symbols might clash with the
previous version's symbols.

vN and vN+1 of a GCP service might exist at the same time. They might share
names (although not protobuf package names), but have different feature sets.
Customers must be able to use these different versions of the same GCP service
in a single C++ program.

Historically, we have grouped related services into a single library. We have
placed the code for such a library, including all the `*Client` classes for
these services in a flat hierarchy under `google/cloud/<library>`. We have only
generated clients for the "latest" version of these services. These clients all
exist in the `google::cloud::library` namespace.

**Decision**:
We will continue to pick some directory in the `googleapis` repo, and map it to
a library in `google-cloud-cpp` (e.g. `googleapis//google/privacy/dlp` ->
`google-cloud-cpp//google/cloud/dlp`). This process involves human judgment, but
there is a lot of precedence to fall back on.

The library in our repo will match the subdirectory structure of the source
directory in the `googleapis` repo. We will **not** continue to place the code
in a flat hierarchy under `google/cloud/<library>`.

Namespaces of services will match the subdirectory structure of the
`google-cloud-cpp` library, but with `_` instead of `/`s. We cannot just nest
the namespace, because that may conflict with the gRPC types. For example,
`google/cloud/bigquery/v2/` -> `google::cloud::bigquery_v2` and
`google/cloud/bigquery/storage/v1` -> `google::cloud::bigquery_storage_v1`.

Libraries will thus contain **all** versions of the nested GCP services
(provided that GCP still supports those versions).

We will not initially break any existing code. This ADR has nothing to say about
if or when non-versioned library namespaces are removed.

**Consequences**:
Customers will be able to use multiple versions of an active GCP service from a
single version of `google-cloud-cpp`.

Service namespaces will become more verbose, as they will necessarily
include some version information.

It will become easier for `google-cloud-cpp` library maintainers to update
existing GCP services (because we now have a protocol).

Customers must build all versions of all services in a library, even if they
only wish to use one version.
