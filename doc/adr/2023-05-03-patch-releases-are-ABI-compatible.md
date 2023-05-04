**Title**: Patch releases **must** be ABI compatible.

**Status**: accepted

**Context**: We create a few patch releases each year. These are always bug
fixes. We have never introduced API changes in a patch release, nor should we.
Prior to this ADR, our patch releases changed the Application Binary Interface
(ABI) as the patch number was embedded in the inline namespace
`google::cloud::v$Major_$Minor_$Patch`.

**Decision**: We will remove the patch number from our inline namespace. If we
ever have to hotfix a bug in such a way that it breaks the API or ABI we will
**not** use a patch release for such fixes. We will just use a new minor
release.

**Consequences**:

- Applications will not be able to link two patch versions of the same minor
  release. That is, they will not be able to link v4.2.1 and v4.2.2 at the same
  time. This is unlikely to cause problems as recompiling / relinking against
  the newer version is trivial.
- Package maintainers will be able to release packages with nicer compatibility
  guarantees.
