**Title**: use only public types in public APIs, and aliases are no exceptions

**Status**: proposed <!-- TODO(coryan) - change to accepted before merge -->

**Context**: We want our public APIs to be fully documented, easy to discover,
and *stable*. We need the flexibility to change the implementation details of
our public APIs. We label symbols that are subject to change by placing them in
an `::internal::` or `::${library}_internal::` namespace. This is all well and
good, but sometimes we have used aliases to these internal symbols in our public
APIs. We should not do this, it leaves important parts of the public API
undocumented, and makes it unclear if the API is stable.

**Decision**: Never require (take as an argument) or return an internal type in
the public APIs. If a type is part of the public API, then the type needs to be
moved out of the internal namespace. If possible, just expose an interface in
the public API, and keep the implementation details in the internal namespace.

**Consequences**: On the positive side, our public APIs become better
documented, more discoverable, and easier to reason about. On the negative side,
we need to ensure any type we consume or return from a public API is stable.
That will constrain the class of changes we can make to these APIs.

We will adopt the ADR first, and change any APIs that do not conform as needed.
Some cases will be difficult, all the APIs using retry and backoff policies
appear somewhat complicated.
