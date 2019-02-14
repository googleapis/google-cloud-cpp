**Title**: Preserve all functionality offered by the raw API.

**Status**: proposed

**Context**: When we design APIs for the client libraries one may need to
tradeoff ease of use for functionality. All things being equal it is easier to
design simpler, easier to understand client libraries if one is willing to
remove functionality that is rarely used. While we want to encourage users to
adopt the libraries, we do not want to abandon advanced users to their own
devices. We do not want them to be tempted or forced to discard the client
libraries in favor of raw REST or gRPC because that is the only way they can
access a feature.

**Decision**: the libraries must preserve all functionality. If it is possible
to perform a task using the raw gRPC or REST API, it should be possible to
perform this task with the library. It is acceptable for the library to make
it easy to use the library for the most common cases, but to require additional
configuration to access rarely used features.

This applies to some non-functional requirements too, the library should not
consume too many additional resources (CPU, memory, network) over the raw gRPC
or REST API.

**Consequences**: the library may need to offer policies, configuration options,
builders, or optional parameters to access the more esoteric behaviors.
The library may chose default values for these options, such that in most cases
the application does not need to specify the options at all. That increases the
burden for the library developers, particularly the documentation must detail
all optional parameters, configuration settings, and policies in enough detail
for application developers to discover them.

