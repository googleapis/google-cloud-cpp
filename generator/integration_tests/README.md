# Updating the golden files

We keep a set of "golden" files to ensure the generator has not changed its
output in unexpected ways.  If you need to update these files, use this command:

```shell
bazel_output_base="$(bazel info output_base)"
bazel run \
  //generator:google-cloud-cpp-codegen -- \
  --protobuf_proto_path="${bazel_output_base}/external/com_google_protobuf/src" \
  --googleapis_proto_path="${bazel_output_base}/external/com_google_googleapis" \
  --golden_proto_path="${PWD}" \
  --output_path="${PWD}" \
  --update_ci=false \
  --config_file="${PWD}/generator/integration_tests/golden_config.textproto"
```

The `generate-libraries` build also runs this command and highlights any changes
using `git diff`.

## Keep hand-crafted tests separate from golden files

We write unit tests to verify the generated files work as expected. These tests
are in the `tests/` subdirectory.  Do **not** insert hand-crafted code into
the `golden/` subdirectory. That directory does not get the usual creature
comforts, such as automatic code formatting.
