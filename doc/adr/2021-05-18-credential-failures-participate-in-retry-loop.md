**Title**: Credential refresh failures participate in retry loop.

**Status**: accepted

**Context**: many credential types require fetching tokens from a remote service. For example, service account
impersonation credentials must obtain an access token (which are time limited) by contacting the
[IAM service account credentials] service. Likewise, for services without
[self-signed JWT] support (e.g. GCS) the access tokens must be obtained via a separate service. The question arises:
should we use a retry loop to refresh the credentials? Or should rely on the retry loop of the RPC that triggers a token
refresh?

**Decision**: no separate loop to refresh credentials, the failures participate in the overall retry loop.

**Consequences**: for customers this means one less retry loop to think about. A separate refresh loop would be
surprising as some random RPCs could take longer (or shorter) to fail than configured by the main retry loop.

The downside is failures on a token refresh that is triggered by a non-idempotent operation. Obviously token
refreshes **are** retryable, even if the operation that triggered the refresh is not retryable. We can minimize these
problems by using background threads (if they exist) to refresh the tokens. As tokens can be cached for several minutes,
we can start a refresh before they expire, and these background refreshes should be retryable.

**Alternatives Considered:**

_Retry the token refresh but only use remaining time:_ in this approach we would run the retry loop for the token
refresh, using the existing retry strategy (and consuming its "time" or "error count"). This is feasible, but requires
changing *all* stubs and decorators to consume a retry policy.

[IAM service account credentials]: https://cloud.google.com/iam/docs/reference/credentials/rest

[self-signed JWT]: https://google.aip.dev/auth/4111
