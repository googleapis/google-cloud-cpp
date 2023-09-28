# OAuth2 Access Token Generation Library

This library contains helpers to create OAuth2 access tokens. This may be useful
when trying to access customer services deployed to Cloud Run, or when using
Google APIs that lack a client library in `google-cloud-cpp`.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/oauth2/access_token_generator.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << "\n";
    return 1;
  }

  // Use Application Default Credentials to initialize an AccessTokenGenerator
  // retrieve the access token.
  auto credentials = google::cloud::MakeGoogleDefaultCredentials();
  auto generator =
      google::cloud::oauth2::MakeAccessTokenGenerator(*credentials);
  auto token = generator->GetToken();
  if (!token) throw std::move(token).status();
  // Only print the first few characters to avoid leaking tokens in test logs.
  std::cout << "The access token starts with " << token->token.substr(0, 16)
            << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- [Using OAuth 2.0 to Access Google APIs]: for general information around OAuth2
  and their use in Google APIs.
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[doxygen-link]: https://cloud.google.com/cpp/docs/reference/oauth2/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/oauth2
[using oauth 2.0 to access google apis]: https://developers.google.com/identity/protocols/oauth2
