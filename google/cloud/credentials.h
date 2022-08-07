// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_CREDENTIALS_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
class CredentialsVisitor;
}  // namespace internal

/**
 * The public interface for Google's Unified Auth Client (GUAC) library.
 *
 * The Unified Auth Client library allows C++ applications to configure
 * authentication for both REST-based and gRPC-based client libraries. The
 * library public interface is (intentionally) very narrow. Applications
 * describe the type of authentication they want, the libraries used this
 * description to initialize the internal components used in the authentication
 * flows.
 *
 * A complete overview of authentication and authorization for Google Cloud is
 * outside the scope of this reference guide. We recommend the [IAM overview]
 * instead. The following brief introduction may help as you read the reference
 * documentation for components related to authentication:
 *
 * - The `Credentials` class and the factory functions that create
 *   `std::shared_ptr<Credentials>` objects are related to *authentication*.
 *   That is they allow you to define what *principal* is making RPCs to GCP.
 * - The problem of *authorization*, that is, what principals can perform what
 *   operations, is resolved by the [IAM Service] in GCP. If you need to use
 *   this service, the [C++ IAM client library] may be useful. Some services
 *   embed IAM operations in their APIs, in that case, the C++ client library
 *   for the service may be easier to use.
 * - There are two types of "principals" in GCP.
 *   - **Service Accounts**: is an account for an application or compute
 *     workload instead of an individual end user.
 *   - **Google Accounts**: represents a developer, an administrator, or any
 *     other person who interacts with Google Cloud.
 * - Most applications should use `GoogleDefaultCredentials()`. This allows your
 *   administrator to configure the service account used in your workload by
 *   setting the default service account in the GCP environment where your
 *   application is deployed (e.g. GCE, GKE, or Cloud Run).
 * - During development, `GoogleDefaultCredentials()` uses the
 *   `GOOGLE_APPLICATION_CREDENTIALS` environment variable to load an
 *   alternative service account key. The value of this environment variable is
 *   the full path of a file which contains the service account key.
 * - During development, if `GOOGLE_APPLICATION_CREDENTIALS` is not set then
 *   `GoogleDefaultCredentials()` will use the account configured via
 *   `gcloud auth application-default login`. This can be either a service
 *   account or a Google Account, such as the developer's account.
 * - Neither `google auth application-default` nor
 *   `GOOGLE_APPLICATION_CREDENTIALS` are recommended for production workloads.
 *
 * @par Limitations
 * The C++ GUAC library does not allow applications to create their own
 * credential types. It is not possible to extend the GUAC library without
 * changing internal components. If you need additional functionality please
 * file a [feature request] on GitHub. Likewise, creating the components that
 * implement (as opposed to *describing*) authentication flows are also
 * considered implementation details. If you would like to use them in your
 * own libraries please file a [feature request].
 *
 * @see https://cloud.google.com/docs/authentication for more information on
 *     authentication in GCP.
 *
 * @see https://cloud.google.com/iam for more information on the IAM Service.
 *
 * [feature request]: https://github.com/googleapis/google-cloud-cpp/issues
 * [IAM overview]: https://cloud.google.com/iam/docs/overview
 * [IAM Service]: https://cloud.google.com/iam/docs
 * [C++ IAM client library]: https://googleapis.dev/cpp/google-cloud-iam/latest/
 */
class Credentials {
 public:
  virtual ~Credentials() = 0;

 private:
  friend class internal::CredentialsVisitor;
  virtual void dispatch(internal::CredentialsVisitor& visitor) = 0;
};

/**
 * Create insecure (aka anonymous, aka unauthenticated) credentials.
 *
 * These credentials are mostly intended for testing. Integration tests running
 * against an emulator do not need to authenticate. In fact, it may be
 * impossible to connect to an emulator using SSL/TLS because the emulators
 * typically run without secure communication.
 *
 * In addition, unit tests may benefit from using these credentials: loading the
 * default credentials unnecessarily slows down the unit tests, and in some
 * CI environments the credentials may fail to load, creating confusing warnings
 * and sometimes even errors.
 */
std::shared_ptr<Credentials> MakeInsecureCredentials();

/**
 * Creates the default credentials.
 *
 * These are the most commonly used credentials, and are expected to meet the
 * needs of most applications. The Google Default Credentials conform to
 * [aip/4110]. Consider using these credentials when:
 *
 * - Your application is deployed to a GCP environment such as GCE, GKE, or
 *   Cloud Run. Each of these deployment environments provides a default service
 *   account to the application, and offers mechanisms to change the default
 *   credentials without any code changes to your application.
 * - You are testing or developing the application on a workstation (physical or
 *   virtual). These credentials will use your preferences as set with
 *   [gcloud auth application-default]. These preferences can be your own GCP
 *   user credentials, or some service account.
 * - Regardless of where your application is running, you can use the
 *   `GOOGLE_APPLICATION_CREDENTIALS` environment variable to override the
 *   defaults. This environment variable should point to a file containing a
 *   service account key file, or a JSON object describing your user
 *   credentials.
 *
 * @see https://cloud.google.com/docs/authentication for more information on
 *     authentication in GCP.
 *
 * [aip/4110]: https://google.aip.dev/auth/4110
 * [gcloud auth application-default]:
 * https://cloud.google.com/sdk/gcloud/reference/auth/application-default
 */
std::shared_ptr<Credentials> MakeGoogleDefaultCredentials();

/**
 * Creates credentials with a fixed access token.
 *
 * These credentials are useful when using an out-of-band mechanism to fetch
 * access tokens. Note that access tokens are time limited, you will need to
 * manually refresh the tokens created by the
 *
 * @see https://cloud.google.com/docs/authentication for more information on
 *     authentication in GCP.
 */
std::shared_ptr<Credentials> MakeAccessTokenCredentials(
    std::string const& access_token,
    std::chrono::system_clock::time_point expiration);

/**
 * Creates credentials for service account impersonation.
 *
 * Service account impersonation allows one account (user or service account)
 * to *act as* a second account. This can be useful in multi-tenant services,
 * where the service may perform some actions with an specific account
 * associated with a tenant. The tenant can grant or restrict permissions to
 * this tenant account.
 *
 * When using service account impersonation is important to distinguish between
 * the credentials used to *obtain* the target account credentials (the
 * @p base_credentials) parameter, and the credentials representing the
 * @p target_service_account.
 *
 * Use `AccessTokenLifetimeOption` to configure the maximum lifetime of the
 * obtained credentials.  The default is 1h (3600s), see [IAM quotas] for the
 * limits set by the platform and how to override them.
 *
 * Use `DelegatesOption` to configure a sequence of intermediate service
 * account, each of which has permissions to impersonate the next and the
 * last one has permissions to impersonate @p target_service_account.
 *
 * Use `ScopesOption` to restrict the authentication scope for the obtained
 * credentials. See below for possible values.
 *
 * [IAM quotas]: https://cloud.google.com/iam/quotas
 * @see https://cloud.google.com/docs/authentication for more information on
 *     authentication in GCP.
 * @see https://cloud.google.com/iam/docs/impersonating-service-accounts for
 *     information on managing service account impersonation.
 * @see https://developers.google.com/identity/protocols/oauth2/scopes for
 *     authentication scopes in Google Cloud Platform.
 */
std::shared_ptr<Credentials> MakeImpersonateServiceAccountCredentials(
    std::shared_ptr<Credentials> base_credentials,
    std::string target_service_account, Options opts = {});

/**
 * Creates service account credentials from a service account key.
 *
 * A [service account] is an account for an application or compute workload
 * instead of an individual end user. The recommended practice is to use
 * Google Default Credentials, which relies on the configuration of the Google
 * Cloud system hosting your application (GCE, GKE, Cloud Run) to authenticate
 * your workload or application.  But sometimes you may need to create and
 * download a [service account key], for example, to use a service account
 * when running your application on a system that is not part of Google Cloud.
 *
 * Service account credentials are used in this latter case.
 *
 * You can create multiple service account keys for a single service account.
 * When you create a service account key, the key is returned as string, in the
 * format described by [aip/4112]. This string contains an id for the service
 * account, as well as the cryptographical materials (a RSA private key)
 * required to authenticate the caller.
 *
 * Therefore, services account keys should be treated as any other secret
 * with security implications. Think of them as unencrypted passwords. Do not
 * store them where unauthorized persons or programs may read them.
 *
 * As stated above, most applications should probably use default credentials,
 * maybe pointing them to a file with these contents. Using this function may be
 * useful when the service account key is obtained from Cloud Secret Manager or
 * a similar service.
 *
 * [aip/4112]: https://google.aip.dev/auth/4112
 * [service account]: https://cloud.google.com/iam/docs/overview#service_account
 * [service account key]:
 * https://cloud.google.com/iam/docs/creating-managing-service-account-keys#iam-service-account-keys-create-cpp
 */
std::shared_ptr<Credentials> MakeServiceAccountCredentials(
    std::string json_object);

/// Configure the delegates for `MakeImpersonateServiceAccountCredentials()`
struct DelegatesOption {
  using Type = std::vector<std::string>;
};

/// Configure the scopes for `MakeImpersonateServiceAccountCredentials()`
struct ScopesOption {
  using Type = std::vector<std::string>;
};

/// Configure the access token lifetime
struct AccessTokenLifetimeOption {
  using Type = std::chrono::seconds;
};

/// A wrapper to store credentials into an options
struct UnifiedCredentialsOption {
  using Type = std::shared_ptr<Credentials>;
};

/**
 * Configures a custom CA (Certificates Authority) certificates file.
 *
 * Most applications should use the system's root certificates and should avoid
 * setting this option unnecessarily. A common exception to this recommendation
 * are containerized applications. These often deploy without system's root
 * certificates and need to explicitly configure a root of trust.
 *
 * The value of this option should be the name of a file in [PEM format].
 * Consult your security team and/or system administrator for the contents of
 * this file. Be aware of the security implications of adding new CA
 * certificates to this file. Only use trustworthy sources for the CA
 * certificates.
 *
 * For REST-based libraries this configures the [CAINFO option] in libcurl.
 * These are used for all credentials that require authentication, including the
 * default credentials.
 *
 * For gRPC-based libraries this configures the `pem_roots_cert` parameter in
 * [grpc::SslCredentialsOptions].
 *
 * @warning gRPC does not have a programmatic mechanism to set the CA
 *     certificates for the default credentials. This option only has no effect
 *     with `MakeGoogleDefaultCredentials()`, or
 *     `MakeServiceAccountCredentials()`.
 *     Consider using the `GRPC_DEFAULT_SSL_ROOTS_FILE_PATH` environment
 *     variable in these cases.
 *
 * @note CA certificates can be revoked or expire, plan for updates in your
 *     deployment.
 *
 * @see https://en.wikipedia.org/wiki/Certificate_authority for a general
 * introduction to SSL certificate authorities.
 *
 * [CAINFO Option]: https://curl.se/libcurl/c/CURLOPT_CAINFO.html
 * [PEM format]: https://en.wikipedia.org/wiki/Privacy-Enhanced_Mail
 * [grpc::SslCredentialsOptions]:
 * https://grpc.github.io/grpc/cpp/structgrpc_1_1_ssl_credentials_options.html
 */
struct CARootsFilePathOption {
  using Type = std::string;
};

/// A list of GUAC options.
using UnifiedCredentialsOptionList =
    OptionList<AccessTokenLifetimeOption, CARootsFilePathOption,
               DelegatesOption, ScopesOption, UnifiedCredentialsOption>;

namespace internal {

/**
 * Use an insecure channel for AccessToken authentication.
 *
 * This is useful when testing against emulators, where it is impossible to
 * create a secure channel.
 */
struct UseInsecureChannelOption {
  using Type = bool;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_CREDENTIALS_H
