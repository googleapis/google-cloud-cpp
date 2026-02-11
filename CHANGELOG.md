# Changelog

**NOTE**: [Future Breaking Changes](/doc/deprecated.md) lists anticipated
breaking changes in the upcoming 4.x release. This release is scheduled for
2027-12 or 2028-01.

## v3.0.0 - 2026-02

**BREAKING CHANGES**

As previously announced, `google-cloud-cpp` now requires C++ >= 17. This is
motivated by similar changes in our dependencies, and because C++ 17 has been
the default C++ version in all the compilers we support for several years.

We think this change is large enough that deserves a major version bump to
signal the new requirements. Additionally, we have decommissioned previously
deprecated API features and have also made some previously optional dependencies
required.

**NOTE**: Please refer to the [V3 Migration Guide](/doc/v3-migration-guide.md) 
for details on updating existing applications using v1.x.y or v2.x.y.
