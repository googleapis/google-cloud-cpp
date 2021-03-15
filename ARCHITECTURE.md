# Architecture

This document describes the high-level architecture of the Google Cloud C++
Client libraries. Its main audience are developers and contributors making
changes and additions to these libraries, but users of the libraries may also
find this useful. If you want to familiarize yourself with the code in the
`google-cloud-cpp` project, you are at the right place.

## What these libraries do

The goal of the libraries is to provide idiomatic C++ libraries to access
services in [Google Cloud Platform](https://cloud.google.com). All services are
in scope, though only a handful of services have been implemented to this point.

What do we mean by idiomatic? We mean that C++ developers will find the
APIs familiar, or "natural", that these APIs will fit well with the rest of the
C++ ecosystem, and that very few new "concepts" are needed to understand how to
use these libraries.

More specifically, some of the functionality offered by these libraries include:

- The libraries automatically *retry* RPCs that fail with a retriable error,
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
- The libraries convert long running operations into a `future`, an object
  that will get a value asynchronously. The library takes care of polling the
  long running operation until it is completed (or fails).
- The libraries convert pagination APIs into C++ iterators.
- The libraries can be configured to log RPCs and responses, to help application
  developers troubleshoot their code.

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

- Bigtable has a [bigtable::Table](/google/cloud/bigtable/table.h) client, as
  well as [TableAdmin](/google/cloud/bigtable/table_admin.h) and
  [InstanceAdmin](/google/cloud/bigtable/instance_admin.h) clients.
- Storage has a single [storage::Client](/google/cloud/storage/client.h) client
  class.
- Pub/Sub has two non-admin clients, the
  [pubsub::Publisher](/google/cloud/pubsub/publisher.h)
  and the [pubsub::Subscriber](/google/cloud/pubsub/subscriber.h). It also has
  two admin clients:
  [pubsub::TopicAdminClient](/google/cloud/pubsub/topic_admin_client.h),
  and [pubsub::SubscriptionAdminClient](/google/cloud/pubsub/subscription_admin_client.h).
- Spanner has one non-admin client, the
  [spanner::Client](/google/cloud/spanner/client.h). It also has two admin
  clients:
  [spanner::InstanceAdminClient](/google/cloud/spanner/instance_admin_client.h),
  and
  [spanner::DatabaseAdminClient](/google/cloud/spanner/database_admin_client.h).

Generally these classes are very "thin"; they take function arguments from the
application, package them in lightweight structure, and then forward the request
to the `${library}::*Connection` class.

It is important to know that *almost* always there is one RPC generated by each
`*Client` member functions and RPCs. But that "almost" is very important, the
devil is, as usual, in the details.

## The `*Connection` classes

Connections serve two functions:

* They are interfaces, which applications can mock to implement tests against
  the `google-cloud-cpp` library.
* The default implementation injects the "retry loop" for each RPC.

Most of the time there is a 1-1 mapping between a `FooClient` and the
corresponding `FooConnection`.

Because `*Connection` classes are intended for customers to use (at least as
mocks), they are part of the public API.

## The `*Stub` classes

The `*Stub` classes wrap the `*Stub` generated by `gRPC`. They provide several
functions:

* They are pure interfaces, which allow `google-cloud-cpp` **developers** to
  mock them in our tests.
* They are not part of the public interface, they live in `${library}_internal`,
  and therefore give us some flexibility to change things.
* They return `StatusOr<T>`, simplifying some of the calls and generic
  programming.
* They are the classes most amenable for automatic code generation.
* When using asynchronous APIs, they return
  `google::cloud::future<StatusOr<T>>`. This is preferred over using a `void*`
  to match requests and responses.

The `*Stub` classes are typically organized as a (small) stack of
[Decorators](https://en.wikipedia.org/wiki/Decorator_pattern), which simplifies
their testing.

| Layer | Description |
| ----- | ----------- |
| `*Logging` | Optional `*Stub` decorator that logs each request and response |
| `*Metadata` | Injects resource metadata headers for routing |
| `*RoundRobin` | Round-robins over several `*Stub`s, not all libraries have them |

## The `Options` class(es)

Many functions need a way for a user to specify optional settings. This was
traditionally done with distinct classes, like
[`spanner::ConnectionOptions`][spanner-connection-options-link],
[`spanner::QueryOptions`][query-options-link], or
[`storage::ClientOptions`][client-options-link]. These classes often had very
different interfaces and semantics (e.g., some included a meaningful default
value, others didn't). The new, **recommended** way to represent options of all
varieties is using the [`google::cloud::Options`][options-link] class.

Any function that needs to accept optional settings should do so by accepting
an instance of `google::cloud::Options`, and by **documenting** which option
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
It's OK to introduce additional options files, but keep discoverability in
mind.

Instances of `Options` do not contain any default values. Defaults should be
computed by a service-specific function, such as
[`spanner_internal::DefaultOptions()`][spanner-defaults-link]. This function
(or a related one) is used _by our implementations_ to augment the (optionally)
user-provided `Options` instance with appropriate defaults for the given
service. Defaults should be computed in the user-facing function that accepted
the `Options` argument so that all the internal implementation functions lower
in the stack can simply accept the `Options` by `const&` and can assume it's
properly populated. The user-facing function that documented to the user which
options it accepts should also call
[`google::cloud::internal::CheckExpectedOptions<...>(...)`][check-expected-example-link]
in order to help users diagnose option-related issues in their code.

[spanner-connection-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/master/google/cloud/spanner/connection_options.h
[query-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/master/google/cloud/spanner/query_options.h
[client-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/master/google/cloud/storage/client_options.h
[common-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/master/google/cloud/common_options.h
[grpc-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/master/google/cloud/grpc_options.h
[spanner-options-link]: https://github.com/googleapis/google-cloud-cpp/blob/master/google/cloud/spanner/options.h
[options-link]: https://github.com/googleapis/google-cloud-cpp/blob/master/google/cloud/options.h
[spanner-defaults-link]: https://github.com/googleapis/google-cloud-cpp/blob/master/google/cloud/spanner/internal/defaults.h
[check-expected-example-link]: https://github.com/googleapis/google-cloud-cpp/blob/6bd0fae69af98939a1ba4fedea7bb20366ad15d9/google/cloud/spanner/client.cc#L358-L360

## Deviations from the "normal" Architecture

### Bigtable

Bigtable does not have `*Stub` and `*Connection` classes, they are sadly merged
into a single layer.

### Pub/Sub

Pub/Sub generally follow these patterns, but there is substantial code outside
the main classes to implement a few features:

- The `Publisher` needs to buffer messages until there is enough of them to
  justify a `AsyncPublish()` call. Moreover, when using ordering keys the
  library maintains separate buffers per ordering keys, and must respect
  message ordering. There is a whole hierarchy of `Batching*` classes to deal
  with these requirements.
- Likewise, the `Subscriber` must deliver messages with ordering keys one at a
  time, and must implement flow control for messages delivered to the
  application. There is a series of classes dedicate to keeping a
  "session" working correctly.

### Spanner

Spanner implements some key features in the
[spanner_internal::SessionPool](/google/cloud/spanner/internal/session_pool.h).

### Storage

The Storage `*Connection` classes are called `storage::internal::RawClient`,
which sadly forces our users to reach into the `internal` namespace to mock
things.  Furthermore, some of the `RawClient` decorators should be called
`*Stub`.
