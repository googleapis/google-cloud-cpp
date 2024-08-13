**Title**: Releases should contain googleapis SHA that are recent and mature.

**Status**: proposed

**Context**: We typically update the googleapis commit SHA that determines the
vintage of proto service definitions as needed during the month. This leads to
an irregular cadence of SHA updates that if infrequent could result in a release
pointing at older versions of proto files that do not include updates from the
service authors that they and users expect to see in new releases. If the most
recent SHA update occurs too close to publishing a release, we run the risk of
rollbacks/forward-fixes in the googleapis repo that could require us to
immediately publish a subsequent release(s).

**Decision**: Between the last 5-7 days of the month, we will make a final
update of the googleapis commit SHA and freeze it to that value until after the
next release is published.

**Consequences**: googleapis service proto changes that land during this freeze
period will not appear in the upcoming release, but will have to wait until the
following release.
