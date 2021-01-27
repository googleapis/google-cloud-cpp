# Verify CMake and Bazel targets are usable

We were relying on the quickstart examples to verify our CMake and Bazel
targets are usable, but this does not work when the targets change, as
the quickstart examples usually build using the *previous* release.
