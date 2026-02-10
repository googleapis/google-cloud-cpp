# Future Breaking Changes

The next major version of `google-cloud-cpp` (the 4.x series) is expected to
include the following **breaking changes**.

<details>
<summary>Pubsub: remove legacy admin APIs</summary>
<br>

We will remove the hand-written versions of the Pub/Sub admin APIs. These have
been superseded by versions generated automatically from the service
definitions. The new APIs can be found in the
[`google/cloud/pubsub/admin`](https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/pubsub/admin)
tree and within the `google::cloud::pubsub_admin` namespace. Starting with the
v2.23.0 release, and depending on your compiler settings, using the old
classes/functions may elicit a deprecation warning. See [#12987] for more
details.

</details>

[#12987]: https://github.com/googleapis/google-cloud-cpp/issues/12987
