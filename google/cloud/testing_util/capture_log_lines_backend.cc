//
// Created by coryan on 10/23/18.
//

#include "google/cloud/testing_util/capture_log_lines_backend.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

void CaptureLogLinesBackend::Process(LogRecord const& lr) {
  // Break the records in lines, it is easier to analyze them as such.
  std::istringstream is(lr.message);
  std::string line;
  while (std::getline(is, line)) {
    log_lines.emplace_back(line);
  }
}

void CaptureLogLinesBackend::ProcessWithOwnership(LogRecord lr) { Process(lr); }

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
