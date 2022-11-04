**Title**: clients for multiple versions of a GCP service are grouped into one
library.

**Status**: proposed

**Context**:
We need our libraries to be resilient against new versions of existing GCP
services.

Note that we are talking about different versions of a GCP service. That is, the
version that would appear in that service's API reference docs. We are not
talking about versions of the google-cloud-cpp client library.

vN and vN+1 of a GCP service might exist at the same time. They might share
names (although not package names), but have different feature sets. Customers
must be able to use these different versions of the same GCP service, together.

Historically, we have defined a client library in `google/cloud/<library>`,
regardless of the directory structure in `googleapis/googleapis`. We have
included all nested services in that library. We have defined clients for these
services in the `google::cloud::library` namespace. We have only added clients
for the "latest" version of these services.

For example, at the moment of writing this, `google::cloud::bigquery` contains
multiple nested services. These services have independent versions (some are v1,
some are v2). If a new version of one of these services was released, we could
not necessarily put it in the `google::cloud::bigquery` namespace, as its
symbols might clash with the previous version's symbols. That is the problem
addressed by this ADR.

**Decision**:
We recognize that it is necessary to capture the version of individual GCP
services in our types.

We will continue to group nested services in one library, under
`google/cloud/<library>`. The subdirectory structure of a library will now match
that of the subdirectory structure within `googleapis/googleapis`.

Namespaces of services will be (for example) `google::cloud::foo_vN` or
`google::cloud::foo_nested_vN`. We must do this to avoid conflicts with proto
types, which may be defined in `google::cloud::foo::vN`

Libraries will thus contain **all** versions of the nested GCP services
(provided that GCP still supports those versions).

We will not initially break any existing code. This ADR has nothing to say about
if or when non-versioned library namespaces are removed.

**Consequences**:
Customers will be able to use multiple versions of an active GCP service from a
single version of google-cloud-cpp.

It will become easier for google-cloud-cpp library maintainers to update
existing GCP services (because we now have a protocol).

Service namespaces will become more verbose, as they will necessarily
include some version information.

Customers must build all versions of all services in a library, even if they
only wish to use one version.
