# How-to Guide: Find missing service proto files

Some of the generated libraries include multiple gRPC services. Some directories
in `googleapis/googleapis` include multiple services in a single `.proto` file.
Some directories add new `.proto` files, and the expectation is that all these
proto files are included in the same "package" or "library".

Most of the time we get a feature request to add new libraries. We rarely get
such a bug for new `.proto` files, because for other languages the automation
detects these and adds them as needed.

Fixing our processes and automation to detect these new `.proto` files would be
ideal. Until that happens, we need to periodically detect if new protos must be
included in a library.

We can find these with a few commands.

First use Bazel to compile `//:grpc_utils`. That will force Bazel to download
the version of `googleapis/googleapis` defined in our repo:

```shell
bazel build //:grpc_utils
```

Next find out where bazel downloaded the code:

```shell
googleapis="$(bazel info output_base)/external/com_google_googleapis/"
```

At this point we can run a "one liner" to find the mismatched repos:

```shell
time comm -23 \
    <(git ls-files -- 'external/googleapis/protolists/*.list' | \
        xargs sed -e 's;@com_google_googleapis//;;' -e 's;:;/;' | \
        xargs env -C ${googleapis} grep -l '^service ' | sort) \
    <(sed -n  '/service_proto_path:/ {s/.*_path: "//; s/"//p}' generator/generator_config.textproto | sort)
```

Once you have identified missing `*.proto` files, follow the
[Adding generated libraries] guide to add these protos to an existing library.

## Explanation

We make the following assumptions, based on how we maintain the repository:

1. Recall that all the libraries we generate are based on `*_cc_grpc` targets in
   `googleapis/googleapis`.
1. Recall that we do only generate libraries for a subset of the `*_cc_grpc`
   targets, e.g., we only include Cloud services.
1. We are interested in any `*.proto` file that is needed by a `*_cc_grpc`
   target we *already have*, but that is not referenced from the
   `generator_config.textproto` file.
1. Recall that the list of `*.proto` file dependencies are in our `*.list`
   files. These files are automatically generated (see [`update_libraries.sh`]).
1. By convention / coding standards, all service definitions in the Google
   `*.proto` files start at column 0 with `service `.

With these assumptions, we can explain how the script works. First gather all
the `*.list` files:

```shell
git ls-files -- 'external/googleapis/protolists/*.list'
```

Each file contains proto *targets*, we convert this list to just the relative
path of each `*.proto` file:

```shell
git ls-files -- 'external/googleapis/protolists/*.list' | \
        xargs sed -e 's;@com_google_googleapis//;;' -e 's;:;/;'
```

Now we can search for any files that have a gRPC service definition:

```shell
git ls-files -- 'external/googleapis/protolists/*.list' | \
        xargs sed -e 's;@com_google_googleapis//;;' -e 's;:;/;' | \
        xargs env -C ${googleapis} grep -l '^service '
```

And we sort this list to make the comparison with a different list easy:

```shell
git ls-files -- 'external/googleapis/protolists/*.list' | \
        xargs sed -e 's;@com_google_googleapis//;;' -e 's;:;/;' | \
        xargs env -C ${googleapis} grep -l '^service ' | sort
```

Separately we build the list of protofiles that are listed in
`generator_config.textproto`, and sort it for the same reasons:

```shell
sed -n  '/service_proto_path:/ {s/.*_path: "//; s/"//p}' generator/generator_config.textproto | sort
```

Then just use `comm -23` to find the differences between these two lists.

```shell
time comm -23 \
    <(git ls-files -- 'external/googleapis/protolists/*.list' | \
        xargs sed -e 's;@com_google_googleapis//;;' -e 's;:;/;' | \
        xargs env -C ${googleapis} grep -l '^service ' | sort) \
    <(sed -n  '/service_proto_path:/ {s/.*_path: "//; s/"//p}' generator/generator_config.textproto | sort)
```

[adding generated libraries]: /doc/contributor/howto-guide-adding-generated-libraries.md
[`update_libraries.sh`]: /external/googleapis/update_libraries.sh
