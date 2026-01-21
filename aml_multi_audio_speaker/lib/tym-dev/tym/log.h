/*
 * @file log.h.in
 * @brief TYM Logging Library Header
 *
 * This header provides a flexible and configurable logging interface for TYM projects.
 * It supports multiple logging backends (stdout, stderr, syslog, debug) and standard Linux log levels.
 * Logging can be enabled or disabled at compile time, and the backend can be selected via macros.
 */
#ifndef INCLUDE_TYM_LOG_H_
#define INCLUDE_TYM_LOG_H_


// These must be defined before anything else
#define TYM_LOG_BACKEND_STDOUT    1 /**< @brief Log to standard out                 */
#define TYM_LOG_BACKEND_STDERR    2 /**< @brief Log to standard error               */
#define TYM_LOG_BACKEND_SYSLOG    3 /**< @brief Log to syslog                       */
#define TYM_LOG_BACKEND_DEBUG     4 /**< @brief Log to both standard out and syslog */

// Select a default backend if none is provided
/** @def TYM_LOG_BACKEND
 * @brief Define this to select a backend
 *
 * Backend options are: @ref TYM_LOG_BACKEND_STDOUT, @ref TYM_LOG_BACKEND_STDERR
 * @ref TYM_LOG_BACKEND_SYSLOG, @ref TYM_LOG_BACKEND_DEBUG
 */
#if !defined(TYM_LOG_BACKEND)
#define TYM_LOG_BACKEND TYM_LOG_BACKEND_STDOUT
#endif  // no backend check

// Include the header file for the selected backend
#if TYM_LOG_BACKEND == TYM_LOG_BACKEND_STDOUT
#include <stdio.h>
#endif  // TYM_LOG_BACKEND_STDOUT

#if TYM_LOG_BACKEND == TYM_LOG_BACKEND_STDERR
#include <stdio.h>
#endif  // TYM_LOG_BACKEND_STDERR

#if TYM_LOG_BACKEND == TYM_LOG_BACKEND_SYSLOG
#include <sys/syslog.h>
#endif  // TYM_LOG_BACKEND_SYSLOG

#if TYM_LOG_BACKEND == TYM_LOG_BACKEND_DEBUG
#include <stdio.h>
#include <sys/syslog.h>
#endif  // TYM_LOG_BACKEND_DEBUG


/**
 * @brief TYM log levels
 *
 * The TYM log library uses the standard Linux 8 levels of logging. See @ref
 * tym_log_level
 *
 */
enum tym_log_level {
  TYM_EMERG,   /**< System is unusable. A panic condition */
  TYM_ALERT,   /**< Action must be taken immediately.  A condition that should be
                  corrected immediately, such as a corrupted system database */
  TYM_CRIT,    /**< Critical conditions, such as hard device errors. */
  TYM_ERROR,   /**< Error conditions. */
  TYM_WARNING, /**< Warning conditions. */
  TYM_NOTICE,  /**< Normal but significant conditions. Conditions that are not
                  error conditions, but that may require special handling. */
  TYM_INFO,    /**< Informational messages. */
  TYM_DEBUG    /**< Debug-level messages. Messages that contain information
              normally of use only when debugging a program. */
};

/** @brief TYM Log level */
#ifndef TYM_LOG_LEVEL
#define TYM_LOG_LEVEL TYM_DEBUG
#endif  // TYM_LOG_LEVEL

#ifndef TYM_LOG_DISABLE  // Disable logging by defining TYM_LOGGING_DISABLE
#if TYM_LOG_BACKEND == TYM_LOG_BACKEND_STDOUT
#define TYM_LOG(message, _priority, _fmt, ...)                             \
  do {                                                                     \
    if (_priority <= TYM_LOG_LEVEL) {                                      \
      fprintf(stdout, "%s " _fmt "\n", message,  ##__VA_ARGS__);           \
      fflush(stdout);                                                      \
    }                                                                      \
  } while (0)
#elif TYM_LOG_BACKEND == TYM_LOG_BACKEND_STDERR
#define TYM_LOG(message, _priority, _fmt, ...)                             \
  do {                                                                     \
    if (_priority <= TYM_LOG_LEVEL) {                                      \
      fprintf(stderr, "%s " _fmt "\n", message,  ##__VA_ARGS__);           \
    }                                                                      \
  } while (0)
#elif TYM_LOG_BACKEND == TYM_LOG_BACKEND_SYSLOG
#define TYM_LOG(message, _priority, _fmt, ...)                             \
  do {                                                                     \
    if (_priority <= TYM_LOG_LEVEL) {                                      \
      syslog(_priority, "%s " _fmt "\n", message,  ##__VA_ARGS__);         \
    }                                                                      \
  } while (0)
#elif TYM_LOG_BACKEND == TYM_LOG_BACKEND_DEBUG
#define TYM_LOG(message, _priority, _fmt, ...)                             \
  do {                                                                     \
    if (_priority <= TYM_LOG_LEVEL) {                                      \
      fprintf(stdout, "%s " _fmt "\n", message, ##__VA_ARGS__);            \
      syslog(_priority, "%s " _fmt "\n", message,  ##__VA_ARGS__);         \
      fflush(stdout);                                                      \
    }                                                                      \
  } while (0)
#endif  //  backend check
#else   // !TYM_LOG_DISABLE
#define TYM_LOG(_priority, _fmt, ...) ()
#endif  // TYM_LOG_DISABLE

/** @brief Log an Emergency priority level message         */
#define TYM_LOG_EMERG(_fmt, ...) \
  TYM_LOG("TYM EMERG", TYM_EMERG, _fmt, ##__VA_ARGS__)

/** @brief Log an Alert priority level message             */
#define TYM_LOG_ALERT(_fmt, ...) \
  TYM_LOG("TYM ALERT", TYM_ALERT, _fmt, ##__VA_ARGS__)

/** @brief Log a Critical priority level message           */
#define TYM_LOG_CRIT(_fmt, ...) \
  TYM_LOG("TYM CRIT", TYM_CRIT, _fmt, ##__VA_ARGS__)

/** @brief Log an Error priority level message             */
#define TYM_LOG_ERROR(_fmt, ...) \
  TYM_LOG("TYM ERROR", TYM_ERROR, _fmt, ##__VA_ARGS__)

/** @brief Log a Warning priority level message            */
#define TYM_LOG_WARNING(_fmt, ...) \
  TYM_LOG("TYM WARNING", TYM_WARNING, _fmt, ##__VA_ARGS__)

/** @brief Log a Notice priority level message             */
#define TYM_LOG_NOTICE(_fmt, ...) \
  TYM_LOG("TYM NOTICE", TYM_NOTICE, _fmt, ##__VA_ARGS__)

/** @brief Log an Information priority level message       */
#define TYM_LOG_INFO(_fmt, ...) \
  TYM_LOG("TYM INFO", TYM_INFO, _fmt, ##__VA_ARGS__)

/** @brief Log a Debug priority level message              */
#define TYM_LOG_DEBUG(_fmt, ...) \
  TYM_LOG("TYM DEBUG", TYM_DEBUG, _fmt, ##__VA_ARGS__)

/** @} */
#endif  // INCLUDE_TYM_LOG_H_
