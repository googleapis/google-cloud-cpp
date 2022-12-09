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

#include "google/cloud/common_options.h"
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
 * An opaque representation of the authentication configuration.
 *
 * Applications use factory functions to provide the authentication parameters
 * (for example, a raw access token). The factory function encapsulates the
 * parameters in an instance of this class. The interface in this class is
 * (intentionally) very narrow. Only the internal components in the client
 * libraries should need to access the details of this class.
 *
 * @see https://cloud.google.com/docs/authentication for more information on
 *     authentication in GCP.
 *
 * @see https://cloud.google.com/iam for more information on the IAM Service.
 *
 * [IAM overview]: https://cloud.google.com/iam/docs/overview
 * [IAM Service]: https://cloud.google.com/iam/docs
 * [C++ IAM client library]: https://googleapis.dev/cpp/google-cloud-iam/latest/
 *
 * @ingroup guac
 */
class Credentials {
 public:
  virtual ~Credentials() = 0;

 private:
  friend class internal::CredentialsVisitor;
  virtual void dispatch(internal::CredentialsVisitor& visitor) = 0;
};

/**
 * A wrapper to store credentials into an options
 *
 * @ingroup options
 * @ingroup guac
 */
struct UnifiedCredentialsOption {
  using Type = std::shared_ptr<Credentials>;
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
 *
 * @ingroup guac
 *
 * @param opts optional configuration values.  Note that the effect of these
 *     parameters depends on the underlying transport.  For example
 *     `TracingComponentsOption` is ignored by gRPC-based services.
 */
std::shared_ptr<Credentials> MakeInsecureCredentials(Options opts = {});

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
 *
 * @ingroup guac
 *
 * @param opts optional configuration values.  Note that the effect of these
 *     parameters depends on the underlying transport.  For example
 *     `TracingComponentsOption` is ignored by gRPC-based services.
 */
std::shared_ptr<Credentials> MakeGoogleDefaultCredentials(Options opts = {});

/**
 * Creates credentials with a fixed access token.
 *
 * These credentials are useful when using an out-of-band mechanism to fetch
 * access tokens. Note that access tokens are time limited, you will need to
 * manually refresh the tokens created by the
 *
 * @see https://cloud.google.com/docs/authentication for more information on
 *     authentication in GCP.
 *
 * @ingroup guac
 *
 * @param access_token the access token to be used by the client library.
 * @param expiration the expiration time for the token.
 * @param opts optional configuration values.  Note that the effect of these
 *     parameters depends on the underlying transport.  For example
 *     `TracingComponentsOption` is ignored by gRPC-based services.
 */
std::shared_ptr<Credentials> MakeAccessTokenCredentials(
    std::string const& access_token,
    std::chrono::system_clock::time_point expiration, Options opts = {});

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
 *
 * @ingroup guac
 *
 * @param base_credentials the credentials used to contact the IAM Credentials
 * services.
 * @param target_service_account the email address of the service account to
 * impersonate.
 * @param opts optional configuration values.  Note that the effect of these
 *     parameters depends on the underlying transport.  For example
 *     `TracingComponentsOption` is ignored by gRPC-based services.
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
 *
 * @ingroup guac
 *
 * @param json_object the service account configuration as a JSON string.
 * Typically applications read this from a file, or download the contents from
 * something like Google's secret manager service.
 * @param opts optional configuration values.  Note that the effect of these
 *     parameters depends on the underlying transport.  For example
 *     `TracingComponentsOption` is ignored by gRPC-based services.
 */
std::shared_ptr<Credentials> MakeServiceAccountCredentials(
    std::string json_object, Options opts = {});

/**
 * Configure the delegates for `MakeImpersonateServiceAccountCredentials()`
 *
 * @ingroup options
 * @ingroup guac
 */
struct DelegatesOption {
  using Type = std::vector<std::string>;
};

/**
 * Configure the scopes for `MakeImpersonateServiceAccountCredentials()`
 *
 * @ingroup options
 * @ingroup guac
 */
struct ScopesOption {
  using Type = std::vector<std::string>;
};

/**
 * Configure the access token lifetime
 *
 * @ingroup options
 * @ingroup guac
 */
struct AccessTokenLifetimeOption {
  using Type = std::chrono::seconds;
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
 *
 * @ingroup options
 * @ingroup guac
 */
struct CARootsFilePathOption {
  using Type = std::string;
};

/// A list of  options related to authentication.
using UnifiedCredentialsOptionList =
    OptionList<AccessTokenLifetimeOption, CARootsFilePathOption,
               DelegatesOption, ScopesOption, TracingComponentsOption,
               UnifiedCredentialsOption>;

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
