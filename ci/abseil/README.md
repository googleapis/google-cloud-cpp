# Abseil Installation Test Files

This directory contains **unsupported** files to test `google-cloud-cpp` against
an installed version of Abseil.  The files in this directory are incomplete,
they only install the libraries needed by `google-cloud-cpp`.  The files in this
directory also install "fake" static libraries that are created by header-only
libraries. The files generated in this directory have multiple defects, for
example the `pkg-config` files do not account for dependencies
between the different libraries in Abseil.

In other words, they are intended to exercise the code in `google-cloud-cpp`,
and should not be used for any other purpose.
