# Google Cloud Platform C++ Client Libraries: Optimistic Concurrency Control (OCC)

## Introduction to OCC

Optimistic Concurrency Control (OCC) is a strategy used to manage shared
resources and prevent "lost updates" or race conditions.

In Google Cloud C++ libraries, IAM Policy objects contain an `etag` field. When
calling `SetIamPolicy`, the client library serializes this `etag`. If the server
detects that the `etag` provided does not match the current version on the
server, it returns a `kAborted` (or sometimes `kFailedPrecondition`) status.

## Implementing the OCC Loop in C++

The core of the implementation is a `while` loop that checks the `Status`
returned by the API call.

### Steps of the Loop

| Step          | Action                                                 | C++ Implementation                                     |
| ------------- | ------------------------------------------------------ | ------------------------------------------------------ |
| **1. Read**   | Fetch the current IAM Policy.                          | `client.GetIamPolicy(name)`                            |
| **2. Modify** | Apply changes to the `google::iam::v1::Policy` object. | Modify repeated fields (bindings).                     |
| **3. Write**  | Attempt to set the policy.                             | `client.SetIamPolicy(name, policy)`                    |
| **4. Retry**  | Check `Status.code()`.                                 | `if (status.code() == StatusCode::kAborted) continue;` |

## C++ Code Example

The following example demonstrates how to implement the OCC loop using the
`google-cloud-cpp` Resource Manager library.

```c
#include "google/cloud/resourcemanager/v3/projects_client.h"
#include "google/cloud/common_options.h"
#include "google/cloud/project.h"
#include <iostream>
#include <thread>

namespace resourcemanager = ::google::cloud::resourcemanager::v3;
namespace iam = ::google::iam::v1;

/**
 * Executes an Optimistic Concurrency Control (OCC) loop to safely update an IAM policy.
 *
 * @param project_id The Google Cloud Project ID.
 * @param role The IAM role to grant.
 * @param member The member to add.
 * @return StatusOr<Policy> The updated policy or the final error.
 */
google::cloud::StatusOr<iam::Policy> UpdateIamPolicyWithOCC(
    std::string const& project_id,
    std::string const& role,
    std::string const& member) {

  auto client = resourcemanager::ProjectsClient(
      resourcemanager::MakeProjectsConnection());

  std::string const resource_name = "projects/" + project_id;
  int max_retries = 5;
  int retries = 0;

  // --- START OCC LOOP ---
  while (retries < max_retries) {
    // 1. READ: Get the current policy (includes etag)
    auto policy = client.GetIamPolicy(resource_name);
    if (!policy) {
      return policy; // Return error immediately if Read fails (e.g. Permission Denied)
    }

    // 2. MODIFY: Apply changes to the local Policy object
    // Note: Protobuf manipulation in C++ is explicit
    bool role_found = false;
    for (auto& binding : *policy->mutable_bindings()) {
      if (binding.role() == role) {
        binding.add_members(member);
        role_found = true;
        break;
      }
    }
    if (!role_found) {
      auto& new_binding = *policy->add_bindings();
      new_binding.set_role(role);
      new_binding.add_members(member);
    }

    // 3. WRITE: Attempt to set the modified policy
    // The 'policy' object contains the original 'etag' from Step 1.
    auto updated_policy = client.SetIamPolicy(resource_name, *policy);

    // 4. CHECK STATUS
    if (updated_policy) {
      std::cout << "Successfully updated IAM policy.\n";
      return updated_policy;
    }

    // 5. RETRY LOGIC
    auto status = updated_policy.status();
    if (status.code() == google::cloud::StatusCode::kAborted ||
        status.code() == google::cloud::StatusCode::kFailedPrecondition) {

      retries++;
      std::cout << "Concurrency conflict (etag mismatch). Retrying... ("
                << retries << "/" << max_retries << ")\n";

      // Simple exponential backoff
      std::this_thread::sleep_for(std::chrono::milliseconds(100 * retries));
      continue; // Restart loop
    }

    // If it was a different error (e.g., PermissionDenied), return it.
    return updated_policy;
  }
  // --- END OCC LOOP ---

  return google::cloud::Status(
      google::cloud::StatusCode::kAborted,
      "Failed to update IAM policy after max retries due to contention.");
}
```
