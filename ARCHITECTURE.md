# Architecture

This document describes the high-level architecture of the Google Cloud C++
Client libraries. Its main audience are developers and contributors making
changes and additions to these libraries. If you want to familiarize yourself
with the code in the `google-cloud-cpp` project, you are at the right place.

While we expect users of the libraries may find this document useful, this
document does not change or define the [public API](/doc/public-api.md). You can
use this document to understand how things work, or maybe to troubleshoot
problems. You should not depend on the implementation details described here to
write your application. Only the public API is stable, the rest is subject to
change without notice.

## What these libraries do

The goal of the libraries is to provide idiomatic C++ libraries to access
services in [Google Cloud Platform](https://cloud.google.com). All services are
in scope. As of 2024-03 we have over 100 libraries, covering most Google Cloud
Platform services.

What do we mean by idiomatic? We mean that C++ developers will find the APIs
familiar, or "natural", that these APIs will fit well with the rest of the C++
ecosystem, and that very few new "concepts" are needed to understand how to use
these libraries.

More specifically, the functionality offered by these libraries include:

- The libraries automatically *retry* RPCs that fail with a retryable error,
  such as network failures.
- The libraries only retry operations when it is safe to do so. For example,
  "delete the first row" is not a safe operation to retry when the previous
  result is unknown. In the documentation we refer to operations that are safe
  to retry as [idempotent](https://en.wikipedia.org/wiki/Idempotence).
- The libraries also *resume* large downloads or database reads from the last
  fully received element, minimizing the amount of duplicate data received by
  your application.
- The application can always change the retry and resume policies. The libraries
  have reasonable defaults for these policies, and implement best practices for
  backing off before retrying an operation. If these defaults do not work in
  your specific use-case, you can change them.
- The libraries convert long-running operations into a `future<T>`, an object
  that will get a value asynchronously. The library takes care of polling the
  long-running operation until it is completed (or fails).
- The libraries convert pagination APIs into C++ iterators.
- The libraries can be configured to log RPCs and responses, to help application
  developers troubleshoot their code.
- The libraries return errors using an "outcome" object: `StatusOr<T>`. Similar
  to the upcoming `std::expected<T, E>` class in C++23.
- Application developers can convert outcome objects to exceptions without much
  effort.

## Where is the code?

Each library is found in a subdirectory of [/google/cloud/](/google/cloud). The
name of the subdirectory is chosen to match the most distinctive part of the
service name. For example, `/google/cloud/storage` contains the code for the
[Google Cloud Storage](https://cloud.google.com/storage) service.

Within each directory you will find (almost always) the same structure:

- `${library}/*.h` are the public headers for the library. The same directory
  contains the `${library}/*.cc` implementation files and `*_test.cc` for unit
  tests. More often than not all the unit tests for `foo.{h,cc}` are found in
  `foo_test.cc`, but there may be exceptions when the tests grow too large to
  fit in a single file (we have lots of tests!).
- `mocks/` also contains public APIs, typically this is a header-only library
  defining mocks so application developers can mock the library in their own
  tests. Note that some libraries do not have mocks.
- `quickstart/` contains a small program showing how to use the library in a
  larger project. This is intended for application developers wanting to, well,
  use the libraries quickly.
- `samples/` (sometimes `examples/`) contain code snippets that are extracted
  both into the Doxygen reference documentation and to the cloud.google.com
  site, as part of the service documentation. Some libraries have a single
  program in this directory, others have smaller programs (or more samples).

The remaining directories are implementation details, or at least only intended
to be interesting for `google-cloud-cpp` developers and contributors.

- `internal/` contains implementation details for the library. The files in
  these directories are *not* intended for public consumption, but contributors
  and developers need to change them, of course. The relationship between `.h`,
  `.cc` and `_test.cc` files mirrors the one found in `${library}/`.
- `benchmarks/` contains "macro benchmarks" for the library, typically programs
  that interact with the production environment to verify the library meets the
  latency, throughput, and CPU overhead (as in "low CPU overhead") requirements.
- `ci/` may contain helper scripts used in the CI builds, for example, to
  startup and shutdown any emulators used by the library.
- `doc/` contains the landing page for any Doxygen documentation, as well as
  additional Doxygen docs that are not extracted from the code comments.
- `integration_tests/` (or simply `tests/` in some libraries) contain any
  integration tests where we test the library using emulators and/or the
  production environment.
- `testing/` contains testing utilities shared by the unit tests and the
  integration tests of the library.

## The `*Client` classes

The main interface for each library is the `${library}::*Client` class. Roughly
speaking each "client" corresponds to a `service` in the `.proto` definitions
for the service. It is common for services to have separate `service`
definitions for "admin" operations, such as creating a new instance of the
service, or changing permissions, as opposed to "normal" operations, such as
inserting a new row, or publishing a new message. When "admin" services are
defined, there are separate `${library}::*Client` objects.

These are some examples:

- Bigtable has a [`bigtable::DataClient`](/google/cloud/bigtable/data_client.h)
  client. It also has two admin clients:
  [`bigtable_admin::BigtableInstanceAdminClient`](/google/cloud/bigtable/admin/bigtable_instance_admin_client.h)
  and
  [`bigtable_admin::BigtableTableAdminClient`](/google/cloud/bigtable/admin/bigtable_table_admin_client.h).
- Storage has a single [`storage::Client`](/google/cloud/storage/client.h)
  client class.
- Pub/Sub has two non-admin clients, the
  [`pubsub::Publisher`](/google/cloud/pubsub/publisher.h) and the
  [`pubsub::Subscriber`](/google/cloud/pubsub/subscriber.h). It also has two
  admin clients:
  [`pubsub::TopicAdminClient`](/google/cloud/pubsub/topic_admin_client.h), and
  [`pubsub::SubscriptionAdminClient`](/google/cloud/pubsub/subscription_admin_client.h).
- Spanner has one non-admin client, the
  [`spanner::Client`](/google/cloud/spanner/client.h). It also has two admin
  clients:
  [`spanner_admin::InstanceAdminClient`](/google/cloud/spanner/admin/instance_admin_client.h),
  and
  [`spanner_admin::DatabaseAdminClient`](/google/cloud/spanner/admin/database_admin_client.h).

Generally these classes are very "thin"; they take function arguments from the
application, package them in lightweight structure, and then forward the request
to the `${library}::*Connection` class.

It is important to know that *almost* always there is one RPC generated by each
`*Client` member functions and RPCs. That "almost" is (as the saying goes) "load
bearing", the devil is, as usual, in the details.

## The `*Connection` classes

Connections serve two functions:

- They are interfaces, which applications can mock to implement tests against
  the `google-cloud-cpp` library.
- The default implementation injects the "retry loop" for each RPC.

Most of the time there is a 1-1 mapping between a `FooClient` and the
corresponding `FooConnection`.

Because `*Connection` classes are intended for customers to use (at least as
mocks), they are part of the public API.

Typically there are three concrete versions of the `*Connection` interface:

| Name              | Description                                                   |
| ----------------- | ------------------------------------------------------------- |
| `*Impl`           | An implementation using the `*Stub` layer                     |
| `*Tracing`        | Instrument each retry loop with an [OpenTelemetry] trace span |
| `Mock*Connection` | An implementation using `googlemock`                          |

Only `Mock*Connection` is part of the public API. It is used by application
developers that want to test their code using a mocked behavior for the
`*Client` class.

In some cases you may find a fourth implementation, used to implement clients
over [HTTP and gRPC Transcoding][aip/127]. This class is also not part of the
public API.

| Name         | Description                                    |
| ------------ | ---------------------------------------------- |
| `*Rest*Impl` | An implementation using the `*Rest*Stub` layer |

## The `*Stub` classes

The `*Stub` classes wrap the `*Stub` generated by `gRPC`. They provide several
functions:

- They are pure interfaces, which allow `google-cloud-cpp` **developers** to
  mock them in our tests.
- They are not part of the public interface, they live in `${library}_internal`,
  and therefore give us some flexibility to change things.
- They return `StatusOr<T>`, simplifying the usage of these functions.
- They are the classes most amenable for automatic code generation.
- When using asynchronous APIs, they return
  `google::cloud::future<StatusOr<T>>`. This is preferred over using a `void*`
  to match requests and responses.

The `*Stub` classes are typically organized as a (small) stack of
[Decorators](https://en.wikipedia.org/wiki/Decorator_pattern), which simplifies
their testing.

| Layer         | Description                                                     |
| ------------- | --------------------------------------------------------------- |
| `*Logging`    | Optional `*Stub` decorator that logs each request and response  |
| `*Metadata`   | Injects resource metadata headers for routing                   |
| `*RoundRobin` | Round-robins over several `*Stub`s, not all libraries have them |
| `*Tracing`    | Instrument each RPC with an [OpenTelemetry] trace span          |

For services where we have enabled REST support, there is a parallel set of
`*Rest*Stub` classes. These implement the same functionality, but make calls
using [HTTP and gRPC Transcoding][aip/127].

## The `Options` class(es)

Many functions need a way for a user to specify optional settings. This was
traditionally done with distinct classes, like
[`spanner::ConnectionOptions`][spanner-connection-options-link],
[`spanner::QueryOptions`][query-options-link], or
[`storage::ClientOptions`][client-options-link]. These classes often had very
different interfaces and semantics (e.g., some included a meaningful default
value, others didn't). The new, **recommended** way to represent options of all
varieties is using the [`google::cloud::Options`][options-link] class.

Any function that needs to accept optional settings should do so by accepting an
instance of `google::cloud::Options`, and by **documenting** which option
classes are expected so that users know how the function can be configured.
Functions that accept the old-style option classes can continue to exist and
should forward to the new `Options`-based overload. These old functions need not
even be deprecated because they should work just fine. However, to avoid
burdening users with unnecessary decisions, **functions should clearly document
that the `Options` overload is to be preferred**.

Each setting for an `Options` instance is a unique type. To improve
discoverability of available option types, we should minimize the places where
users have to look to find them to [common_options.h][common-options-link],
[grpc_options.h][grpc-options-link], and (preferably) a single
`<product>/options.h` file (e.g., [`spanner/options.h`][spanner-options-link]).
It's OK to introduce additional options files, but keep discoverability in mind.

Instances of `Options` do not contain any default values. Defaults should be
computed by a service-specific function, such as
[`spanner_internal::DefaultOptions()`][spanner-defaults-link]. This function (or
a related one) is used _by our implementations_ to augment the (optionally)
user-provided `Options` instance with appropriate defaults for the given
service. Defaults should be computed in the user-facing function that accepted
the `Options` argument so that all the internal implementation functions lower
in the stack can simply accept the `Options` by `const&` and can assume it's
properly populated. The user-facing function that documented to the user which
options it accepts should also call
[`google::cloud::internal::CheckExpectedOptions<...>(...)`][check-expected-example-link]
in order to help users diagnose option-related issues in their code.

## Deviations from the "normal" Architecture

### Pub/Sub

Pub/Sub generally follow these patterns, but there is substantial code outside
the main classes to implement a few features:

- The `Publisher` needs to buffer messages until there is enough of them to
  justify a `AsyncPublish()` call. Moreover, when using ordering keys the
  library maintains separate buffers per ordering keys, and must respect message
  ordering. There is a whole hierarchy of `Batching*` classes to deal with these
  requirements.
- Likewise, the `Subscriber` must deliver messages with ordering keys one at a
  time, and must implement flow control for messages delivered to the
  application. There is a series of classes dedicate to keeping a "session"
  working correctly.

### Spanner

Spanner implements some key features in the
[`spanner_internal::SessionPool`](/google/cloud/spanner/internal/session_pool.h).

### Storage

In Storage the `*Connection` classes are in the `storage::internal` namespace,
which forces our users to reach into the `internal` namespace to mock things.
There is an open bug to fix this. It would involve moving all the `*Request` and
`*Response` classes out of `storage::internal`. Some of the member functions in
these classes should not be part of the public API. In short, the changes are
more involved than a simple `git mv`.

[aip/127]: https://google.aip.dev/127
[check-expected-example-link]: https://github.com/googleapis/google-cloud-cpp/blob/0288f8c00dd21de2fb012c517155b300667edc5c/google/cloud/spanner/client.cc#L335-L338
[client-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/storage/client_options.h
[common-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/common_options.h
[grpc-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/grpc_options.h
[opentelemetry]: https://opentelemetry.io/
[options-link]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/options.h
[query-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/query_options.h
[spanner-connection-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/connection_options.h
[spanner-defaults-link]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/internal/defaults.h
[spanner-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/spanner/options.h
