## The GCS Library uses optional fields only for sub-objects.

**Status**: accepted

**Context**: because the underlying protocol for GCS is JSON, any field can be
absent from the responses received from the server. Furthermore, the application
can use the `fields` query parameter to select a subset of the fields in a
response. A natural question is whether the fields should be represented as
`optional<T>`. That is, whether the class to represent object attributes should
look like this:

```C++
class ObjectMetadata { public:
  std::string const& name() const;
  std::chrono::system_clock::time_point time_created() const;
  CustomerEncryption const& customer_encryption() const;
};
```

or it should look like this:

```C++
using google::cloud::optional;
class ObjectMetadata { public:
  optional<std::string> const& name() const;
  optional<std::chrono::system_clock::time_point> time_created() const;
  optional<CustomerEncryption> const& customer_encryption() const;
};
```

**Decision**:

* For string fields where there is no semantic difference between an empty
  string and the field not present we just use `std::string<>`.
* For array fields where there is no semantic difference between field not
  present and an empty array we just use `std::vector<>`.
* For integer and boolean fields we default to `0` (and `false`) if the field
  is not present.
* For object fields we default to wrapping the field in `optional<>`.

For fields wrapped in `optional<>` we offer convenience functions to make it
easier to operate on these fields. For a field called `foo` these are:

* `has_foo()` returns true if the field is set.
* `foo()` returns the field value if set, the behavior is undefined if the value
  is not set.
* `foo_as_optional()` returns the optional field.
* `reset_foo()` resets the field (for writable fields).
* `set_foo()` sets the field (for writable fields).

**Consequences**: The advantage of this approach is that most fields are easy
to use most of the time. The disadvantage of this approach include:

* The ambiguity when the application filters the returned fields, the value
  may be the default value because the client did not get the field.
* As applications change over time and they start filtering different fields
  the code may assume that the value of a field is valid, but it has a default
  value. With optionals the application should crash during testing, or may
  be programmed defensively since the start.
* It also requires more thought designing the classes a field has different
  semantics for "not there" vs. "the default value".

**Reference**

This was originally discussed in
[#934](https://github.com/googleapis/google-cloud-cpp/issues/934).

The original [PR](https://github.com/googleapis/google-cloud-cpp/pull/1358)
also has some interesting discussions.
