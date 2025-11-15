This repository mostly follows the Google C++ style guide found at
https://google.github.io/styleguide/cppguide.html.

This repository differs from the Google C++ style guide in the following ways:

- supports C++14 as its minimum C++ standard version.
- lists local includes first, then system includes.
- uses std::mutex instead of absl::mutex.
- uses google::cloud::future and google::cloud::promise instead of std::future
  and std::promise.
- uses google::cloud::Status and google::cloud::StatusOr instead of absl::Status
  and absl::StatusOr.
- prefers to factor out duplicated code if it appears 3 or more times in
  non-test files.
- encourages duplication of salient setup and expectations in test cases to
  increase readability.
