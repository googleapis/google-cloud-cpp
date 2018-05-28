// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_LOG_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_LOG_H_
/**
 * @file log.h
 *
 * Google Cloud Platform C++ Libraries logging framework.
 *
 * Some of the libraries need to log information to simplify troubleshooting.
 * The functions and macros used for logging are defined in this file. In
 * general, we abide by the following principles:
 *
 * - Logging should controlled by the application developer, unless explicitly
 *   instructed, the libraries produce no output to the console.
 * - Logging should have very low cost:
 *   - It should be possible to disable logs at compile time, they should
 *     disappear as-if there were `#ifdef/#endif` directives around them.
 *   - A log line at a disabled log level should be about as expensive as an
 *     extra if() statement. At the very least it should not incur additional
 *     memory allocations or locks.
 * - It should be easy to log complex objects: the logging library should play
 *   well with the C++ iostream classes.
 * - The application should be able to intercept log records and re-direct them
 *   to their own logging framework.
 *
 * @par Example: Logging From Library
 * Use the `GCP_LOG()` macro to log from a Google Cloud Platform C++ library:
 *
 * @code
 * void LibraryCode(ComplexThing const& thing) {
 *   GCP_LOG(INFO) << "I am here";
 *   if (thing.is_bad()) {
 *     GCP_LOG(ERROR) << "Poor thing is bad: " << thing;
 *   }
 * }
 * @endcode
 *
 * @par Example: Enable Logs to `std::clog`
 * To enable logs to `std::clog` the application can call:
 *
 * @code
 * void AppCode() {
 *   google::cloud::Log::EnableStdClog();
 * }
 * @endcode
 *
 * Note that while `std::clog` is buffered, the framework will flush any log
 * message at severity `WARNING` or higher.
 *
 * @par Example: Capture Logs
 * The application can capture logs by providing a functor:
 *
 * @code
 * void AppCode() {
 *   auto id = google::cloud::Log::CaptureLogs(
 *       [](google::cloud::LogRecord const& record) {
 *           if (record.severity >= google::cloud::Severity::CRITICAL) {
 *             std::cerr << record << std::endl;
 *           }
 *       });
 *   // Use "id" to remove the capture.
 * }
 * @endcode
 */

#include "google/cloud/version.h"
#include <iostream>
#include <memory>
#include <sstream>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/// Concatenate two pre-processor tokens.
#define GOOGLE_CLOUD_CPP_PP_CONCAT(a, b) a##b

/**
 * Create a unique, or mostly-likely unique identifier.
 *
 * In GCP_LOG() we need an identifier for the logger, we can easily create a C++
 * scope to make it work with any name, say "foo".  However the user may have a
 * variable called "foo" that they want to use in the scope (for example, to log
 * the value of "foo".  We try to make it unlikely that there will be collision
 * by using an identifier that has a long prefix and depends on the line number.
 */
#define GOOGLE_CLOUD_CPP_LOGGER_IDENTIFIER \
  GOOGLE_CLOUD_CPP_PP_CONCAT(google_cloud_cpp_log_, __LINE__)

/**
 * The main entry point for the Google Cloud Platform C++ Library loggers.
 *
 * Typically this used only in tests, applications should use GCP_LOG(). This
 * macro introduces a new scope (via the for-loop) with a single new identifier:
 *   GOOGLE_CLOUD_CPP_LOG_RECORD_IDENTIFIER
 * We use a for-loop (as opposed to an if-statement, or a do-while), because
 * then we can say:
 *
 * @code
 * GCP_LOG(WARNING) << a << b << c;
 * @endcode
 *
 * and the variables are scoped correctly. The for-loop also affords us an
 * opportunity to check that the log level is enabled before entering the body
 * of the loop, and if disabled we can minimize the cost of the log. For
 * example, disable logs do not create an iostream at all.  Finally, the type
 * of the GOOGLE_CLOUD_CPP_LOG_RECORD_IDENTIFIER changes if the log level is
 * disabled at compile-time. For compile-time disabled log levels the compiler
 * should be able to determine that the loop will not execute at all, and
 * completely eliminate it.
 */
#define GOOGLE_CLOUD_CPP_LOG_I(level, sink)                                    \
  for (auto GOOGLE_CLOUD_CPP_LOGGER_IDENTIFIER =                           \
           ::google::cloud::LogRecord<                                         \
               ::google::cloud::Log::CompileTimeEnabled(                       \
                   ::google::cloud::Severity::level)>(                         \
               ::google::cloud::Severity::level, __func__, __FILE__, __LINE__, \
               sink);                                                          \
       GOOGLE_CLOUD_CPP_LOGGER_IDENTIFIER.Enabled();                       \
       GOOGLE_CLOUD_CPP_LOGGER_IDENTIFIER.Dump(sink))                      \
  GOOGLE_CLOUD_CPP_LOGGER_IDENTIFIER.Stream()

/**
 * Log a message with the Google Cloud Platform C++ Libraries logging framework.
 *
 *
 */
#define GCP_LOG(level) NullStream()

#ifndef GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED
#define GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED NOTICE
#endif  // GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED

/**
 * Define the severity levels for Gee-H logging.
 *
 * These are modelled after the severity level in syslog(1) and many derived
 * tools.
 */
enum class Severity {
  /// Use this level for messages that indicate the code is entering and leaving
  /// functions.
  TRACE,
  /// Use this level for debug messages that should not be present in
  /// production.
  DEBUG,
  /// Informational messages, such as normal progress.
  INFO,
  /// Informational messages, such as unusual, but expected conditions.
  NOTICE,
  /// An indication of problems, users may need to take action.
  WARNING,
  /// An error has been detected.  Do not use for normal conditions, such as
  /// remote servers disconnecting.
  ERROR,
  /// The system is in a critical state, such as running out of local resources.
  CRITICAL,
  /// The system is at risk of immediate failure.
  ALERT,
  /// The system is about to crash or terminate.
  FATAL,
  /// The highest possible severity level.
  HIGHEST = int(FATAL),
  /// The lowest possible severity level.
  LOWEST = int(TRACE),
  /// The lowest level that is enabled at compile-time.
  LOWEST_ENABLED = int(GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED),
};

/// Streaming operator, writes a human readable representation.
std::ostream& operator<<(std::ostream& os, Severity x);

class Log {
 public:
  Log() : minimum_severity_(Severity::LOWEST_ENABLED) {}

  bool has_sinks() { return false; }
  void set_minimum_severity(Severity minimum) { minimum_severity_ = minimum; }
  Severity minimum_severity() const { return minimum_severity_; }

  constexpr static bool CompileTimeEnabled(Severity level) {
    return level >= Severity::LOWEST_ENABLED;
  }

 private:
  Severity minimum_severity_;
};

/**
 * Implements operator<< for all types, without any effect.
 *
 * It is desirable to disable at compile-time tracing, debugging, and other low
 * severity messages.  The Google Cloud Platform C++ Libraries logging adaptors
 * return an object of this class when the particular log-line is disabled at
 * compile-time.
 */
struct NullStream {
  /// Generic do-nothing streaming operator
  template <typename T>
  NullStream& operator<<(T) {
    return *this;
  }
};

/**
 * Define the class to capture a log message.
 */
template <bool enabled>
class Logger {
 public:
  Logger(Severity severity, char const* function, char const* filename, int lineno, Log& sink)
  : severity_(severity), function_(function), filename_(filename), lineno_(lineno) {
    enabled_ = true;
  }

  bool Enabled() const { return enabled_; }
  void Dump(Log&) {}
  std::ostream& Stream() {
    if (not stream_) {
      stream_.reset(new std::ostringstream);
    }
    return *stream_;
  }

 private:
  Severity severity_;
  char const* function_;
  char const* filename_;
  int lineno_;
  bool enabled_;
  std::unique_ptr<std::ostringstream> stream_;
};

/**
 * Define the logger for a disabled log level.
 */
template <>
class Logger<false> {
 public:
  Logger<false>(Severity, char const*, char const*, int, Log&) {}

  constexpr bool Enabled() const { return false; }
  void Dump(Log&) {}
  NullStream Stream() { return NullStream(); }
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_LOG_H_
