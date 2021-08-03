# Kokoro Configuration and Build scripts.

This directory contains the build scripts and configuration files
for Kokoro, Google's internal CI system for open source projects.

We use Kokoro because:

- We can run Windows and macOS builds. Kokoro can also run Linux builds, but we
  use Google Cloud Build for our Linux builds.
- We can store secrets, such as key files or access tokens, and run integration
  builds against production.

The documentation for Kokoro is available to Googlers only (sorry) at go/kokoro,
there are extensive codelabs and detailed descriptions of the system there.

In brief, the Kokoro configuration is split in two:

1. A series of configuration files inside Google define what builds exist, and
   what resources these builds have access to.
   * These files are in a hierarchy that mirrors the ci/kokoro directory in this
     repo.
   * The `common.cfg` files are parsed first.
   * The `common.cfg` files are applied according to the directory hierarchy.
   * Finally any settings in `foobar.cfg` are applied.
1. A series of configuration files in the `ci/kokoro` directory further define
   the build configuration:
   * They define the build script for each build, though they are often common.
   * They define which of the resources *allowed* by the internal configuration
     are actually *used* by that build.

Somewhat unique to Kokoro one must define a separate *INTEGRATION* vs.
*PRESUBMIT* thus the duplication of configuration files for essentially the
same settings.
