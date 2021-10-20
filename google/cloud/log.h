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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_LOG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_LOG_H

/**
 * @file log.h
 *
 * Google Cloud Platform C++ Libraries logging framework.
 *
 * Some of the libraries need to log information to simplify troubleshooting.
 * The functions and macros used for logging are defined in this file. In
 * general, we abide by the following principles:
 *
 * - Logging should controlled by the application developer. Unless explicitly
 *   instructed, the libraries produce no output to the console, except
 *   to emit a message to `std::clog` immediately before a GCP_LOG(FATAL)
 *   terminates the process.
 * - Logging should have very low cost:
 *   - It should be possible to disable logs at compile time, they should
 *     disappear as-if there were `#%ifdef`/`#%endif` directives around them.
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
 *   google::cloud::LogSink::EnableStdClog();
 * }
 * @endcode
 *
 * Alternatively, the application can enable logging to `std::clog` without any
 * code changes or recompiling by setting the "GOOGLE_CLOUD_CPP_ENABLE_CLOG"
 * environment variable before the program starts. The existence of this
 * variable is all that matters; the value is ignored.
 *
 * Note that while `std::clog` is buffered, the framework will flush any log
 * message at severity `WARNING` or higher.
 *
 * @par Example: Capture Logs
 * The application can implement simple backends by wrapping a functor:
 *
 * @code
 * void AppCode() {
 *   auto id = google::cloud::LogSink::AttachFunctor(
 *       [](google::cloud::LogRecord record) {
 *           if (record.severity >= google::cloud::Severity::CRITICAL) {
 *             std::cerr << record << "\n";
 *           }
 *       });
 *   // Use "id" to remove the capture.
 * }
 * @endcode
 */

#include "google/cloud/version.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/// Concatenate two pre-processor tokens.
#define GOOGLE_CLOUD_CPP_PP_CONCAT(a, b) a##b

/**
 * Create a unique, or most likely unique identifier.
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
 * example, disabled logs do not create an iostream at all.
 *
 * Finally, the type of the GOOGLE_CLOUD_CPP_LOG_RECORD_IDENTIFIER changes if
 * the log level is disabled at compile-time. For compile-time disabled log
 * levels the compiler should be able to determine that the loop will not
 * execute at all, and completely eliminate the code (we verified this using
 * godbolt.org with modern GCC and Clang versions).
 *
 * Note the use of fully-qualified class names, including the initial `::`, this
 * is to prevent problems if anybody uses this macro in a context where `Logger`
 * is defined by the enclosing namespaces.
 */
#define GOOGLE_CLOUD_CPP_LOG_I(level, sink)                                    \
  for (::google::cloud::Logger<::google::cloud::LogSink::CompileTimeEnabled(   \
           ::google::cloud::Severity::level)>                                  \
           GOOGLE_CLOUD_CPP_LOGGER_IDENTIFIER(                                 \
               ::google::cloud::Severity::level, __func__, __FILE__, __LINE__, \
               sink);                                                          \
       GOOGLE_CLOUD_CPP_LOGGER_IDENTIFIER.enabled();                           \
       GOOGLE_CLOUD_CPP_LOGGER_IDENTIFIER.LogTo(sink))                         \
  GOOGLE_CLOUD_CPP_LOGGER_IDENTIFIER.Stream()

// Note that we do not use `GOOGLE_CLOUD_CPP_PP_CONCAT` here: we actually want
// to concatenate `GCP_LS` with the literal `level`. On some platforms, the
// level names are used as macros (e.g., on macOS, `DEBUG` is often a macro with
// value 1 for debug builds). If we were to use `GOOGLE_CLOUD_CPP_PP_CONCAT` and
// `level` is a a macro, then we would get `GCP_LS_<macro_value>`, i.e.,
// `GCP_LS_1` (incorrect) instead of `GCP_LS_DEBUG` (correct).
/**
 * Log a message with the Google Cloud Platform C++ Libraries logging framework.
 */
#define GCP_LOG(level) \
  GOOGLE_CLOUD_CPP_LOG_I(GCP_LS_##level, ::google::cloud::LogSink::Instance())

#ifndef GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED
#define GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED GCP_LS_DEBUG
#endif  // GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED

/**
 * Define the severity levels for Google Cloud Platform C++ Libraries logging.
 *
 * These are modelled after the severity level in syslog(1) and many derived
 * tools.
 *
 * We force the enum to be represented as an `int` because we will store the
 * values in an `std::atomic<>` and the implementations usually optimize
 * `std::atomic<int>` but not `std::atomic<Foo>`
 */
enum class Severity : int {
  /// Use this level for messages that indicate the code is entering and leaving
  /// functions.
  GCP_LS_TRACE,  // NOLINT(readability-identifier-naming)
  /// Use this level for debug messages that should not be present in
  /// production.
  GCP_LS_DEBUG,
  /// Informational messages, such as normal progress.
  GCP_LS_INFO,  // NOLINT(readability-identifier-naming)
  /// Informational messages, such as unusual, but expected conditions.
  GCP_LS_NOTICE,  // NOLINT(readability-identifier-naming)
  /// An indication of problems, users may need to take action.
  GCP_LS_WARNING,  // NOLINT(readability-identifier-naming)
  /// An error has been detected.  Do not use for normal conditions, such as
  /// remote servers disconnecting.
  GCP_LS_ERROR,  // NOLINT(readability-identifier-naming)
  /// The system is in a critical state, such as running out of local resources.
  GCP_LS_CRITICAL,  // NOLINT(readability-identifier-naming)
  /// The system is at risk of immediate failure.
  GCP_LS_ALERT,  // NOLINT(readability-identifier-naming)
  /// The system is unusable.  GCP_LOG(FATAL) will call std::abort().
  GCP_LS_FATAL,  // NOLINT(readability-identifier-naming)
  /// The highest possible severity level.
  GCP_LS_HIGHEST = int(GCP_LS_FATAL),  // NOLINT(readability-identifier-naming)
  /// The lowest possible severity level.
  GCP_LS_LOWEST = int(GCP_LS_TRACE),  // NOLINT(readability-identifier-naming)
  /// The lowest level that is enabled at compile-time.
  // NOLINTNEXTLINE(readability-identifier-naming)
  GCP_LS_LOWEST_ENABLED = int(GOOGLE_CLOUD_CPP_LOGGING_MIN_SEVERITY_ENABLED),
};

/// Streaming operator, writes a human readable representation.
std::ostream& operator<<(std::ostream& os, Severity x);

/**
 * Represents a single log message.
 */
struct LogRecord {
  Severity severity;
  std::string function;
  std::string filename;
  int lineno;
  std::thread::id thread_id;
  std::chrono::system_clock::time_point timestamp;
  std::string message;
};

/// Default formatting of a LogRecord.
std::ostream& operator<<(std::ostream& os, LogRecord const& rhs);

/**
 * The logging backend interface.
 */
class LogBackend {
 public:
  virtual ~LogBackend() = default;

  virtual void Process(LogRecord const& log_record) = 0;
  virtual void ProcessWithOwnership(LogRecord log_record) = 0;
  virtual void Flush() {}
};

/**
 * A sink to receive log records.
 */
class LogSink {
 public:
  LogSink();

  /// Return true if the severity is enabled at compile time.
  static bool constexpr CompileTimeEnabled(Severity level) {
    return level >= Severity::GCP_LS_LOWEST_ENABLED;
  }

  /**
   * Return the singleton instance for this application.
   */
  static LogSink& Instance();

  /**
   * Return true if this object has no backends.
   *
   * We want to avoid synchronization overhead when checking if a log message is
   * enabled. Most of the time, most messages will be disabled, so incurring the
   * locking overhead on each message would be too expensive and would
   * discourage developers from creating logs. Furthermore, missing a few
   * messages while the change of state "propagates" to other threads does not
   * affect the correctness of the program.
   *
   * Note that `memory_order_relaxed` does not provide a compiler barrier
   * either, so in theory stores into the atomic could be reordered by the
   * optimizer. We have no reason to worry about that because all the writes
   * are done inside a critical section protected by a mutex. The compiler
   * cannot (or should not) reorder operations around those.
   */
  bool empty() const { return empty_.load(std::memory_order_relaxed); }

  /**
   * Return true if @p severity is enabled.
   *
   * We want to avoid synchronization overhead when checking if a log message is
   * enabled. Most of the time, most messages will be disabled, so incurring the
   * locking overhead on each message would be too expensive and would
   * discourage developers from creating logs. Furthermore, missing a few
   * messages while the change of state "propagates" to other threads does not
   * affect the correctness of the program.
   *
   * Note that `memory_order_relaxed` does not provide a compiler barrier
   * either, so in theory stores into the atomic could be reordered by the
   * optimizer. We have no reason to worry about that because all the writes
   * are done inside a critical section protected by a mutex. The compiler
   * cannot (or should not) reorder operations around those.
   */
  bool is_enabled(Severity severity) const {
    auto minimum = minimum_severity_.load(std::memory_order_relaxed);
    return static_cast<int>(severity) >= minimum;
  }

  void set_minimum_severity(Severity minimum) {
    minimum_severity_.store(static_cast<int>(minimum));
  }
  Severity minimum_severity() const {
    return static_cast<Severity>(minimum_severity_.load());
  }

  // A `long` is perfectly fine here, it is guaranteed to be at least 32-bits
  // wide, it is unlikely that an application would need more than 2 billion
  // logging backends during its lifetime. The style guide recommends using
  // `std::int32_t` for such a use-case, unfortunately I (coryan) picked `long`
  // before we had tooling to enforce this style guide rule.
  using BackendId = long;  // NOLINT(google-runtime-int)

  BackendId AddBackend(std::shared_ptr<LogBackend> backend);
  void RemoveBackend(BackendId id);
  void ClearBackends();
  std::size_t BackendCount() const;

  void Log(LogRecord log_record);

  /// Flush all the current backends
  void Flush();

  /**
   * Enable `std::clog` on `LogSink::Instance()`.
   *
   * This is also enabled if the "GOOGLE_CLOUD_CPP_ENABLE_CLOG" environment
   * variable is set.
   */
  static void EnableStdClog(
      Severity min_severity = Severity::GCP_LS_LOWEST_ENABLED) {
    Instance().EnableStdClogImpl(min_severity);
  }

  /**
   * Disable `std::clog` on `LogSink::Instance()`.
   *
   * Note that this will remove the default logging backend.
   */
  static void DisableStdClog() { Instance().DisableStdClogImpl(); }

 private:
  void EnableStdClogImpl(Severity min_severity);
  void DisableStdClogImpl();
  void SetDefaultBackend(std::shared_ptr<LogBackend> backend);
  BackendId AddBackendImpl(std::shared_ptr<LogBackend> backend);
  void RemoveBackendImpl(BackendId id);

  std::map<BackendId, std::shared_ptr<LogBackend>> CopyBackends();

  std::atomic<bool> empty_;
  std::atomic<int> minimum_severity_;
  std::mutex mutable mu_;
  BackendId next_id_ = 0;
  BackendId default_backend_id_ = 0;
  std::map<BackendId, std::shared_ptr<LogBackend>> backends_;
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
 *
 * @tparam CompileTimeEnabled this represents whether the severity has been
 *   disabled at compile-time. The class is specialized for `false` to minimize
 *   the run-time impact of (and, in practice, completely elide) disabled logs.
 */
template <bool CompileTimeEnabled>
class Logger {
 public:
  Logger(Severity severity, char const* function, char const* filename,
         int lineno, LogSink& sink)
      : enabled_(!sink.empty() && sink.is_enabled(severity)),
        severity_(severity),
        function_(function),
        filename_(filename),
        lineno_(lineno) {}

  ~Logger() {
    if (severity_ >= Severity::GCP_LS_FATAL) std::abort();
  }

  bool enabled() const { return enabled_; }

  /// Send the log record captured by this object to @p sink.
  void LogTo(LogSink& sink) {
    if (!stream_ || !enabled_) {
      return;
    }
    enabled_ = false;
    LogRecord record;
    record.severity = severity_;
    record.function = function_;
    record.filename = filename_;
    record.lineno = lineno_;
    record.thread_id = std::this_thread::get_id();
    record.timestamp = std::chrono::system_clock::now();
    record.message = stream_->str();
    sink.Log(std::move(record));
  }

  /// Return the iostream that captures the log message.
  std::ostream& Stream() {
    if (!stream_) {
      stream_.reset(new std::ostringstream);
    }
    return *stream_;
  }

 private:
  bool enabled_;
  Severity severity_;
  char const* function_;
  char const* filename_;
  int lineno_;
  std::unique_ptr<std::ostringstream> stream_;
};

/**
 * Define the logger for a disabled log level.
 */
template <>
class Logger<false> {
 public:
  Logger(Severity severity, char const*, char const*, int, LogSink&)
      : severity_(severity) {}

  ~Logger() {
    if (severity_ >= Severity::GCP_LS_FATAL) std::abort();
  }

  //@{
  /**
   * @name Provide trivial implementations that meet the generic `Logger<bool>`
   * interface.
   */
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  bool enabled() const { return false; }
  void LogTo(LogSink&) {}
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  NullStream Stream() { return NullStream(); }
  //@}

 private:
  Severity severity_;
};

namespace internal {
std::shared_ptr<LogBackend> DefaultLogBackend();
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_LOG_H
