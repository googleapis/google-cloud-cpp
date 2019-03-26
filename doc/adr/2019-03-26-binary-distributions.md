**Title**: We will not distribute/own/maintain binary packages, but we will
support others doing so within reason.

**Status**: accepted

**Context**: Compiling google-cloud-cpp from source is not always possible nor
desired by all users. Some users may want to install a binary package that
contains our public headers and pre-compiled libraries so that they can link
against our library. There is no single standard binary package manager in C++,
instead there are a variety of others that users might want to use (e.g., dpkg,
rpm, vcpkg).

**Decision**: We will not directly support nor provide any binary packages. We
will not test binary distributions of our code. And we will not host configs
for binary distributions, since that would involve hosting files which we do
not test. However, we will do our best to make our code easily packageable by
others in a wide variety of formats. For more context, see
[#333](https://github.com/googleapis/google-cloud-cpp/issues/333).

**Consequences**: This decision will shield from endorsing any particular
binary package management system. Other individuals who want to build and
maintain a binary distribution of our code may do so without our permission or
knowledge. If they need small and reasonable tweaks from us to support their
binary distributions, they may file an issue and we'll do our best to
accommodate them within reason.
