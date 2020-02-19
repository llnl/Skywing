#ifndef SKYNET_ENABLE_LOGGING_HPP
#define SKYNET_ENABLE_LOGGING_HPP

#include "spdlog/spdlog.h"

// Macros to enable logging; if the logging level isn't high enough than these
// will do nothing
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  #define SKYNET_ENABLE_TRACE_LOG() ::spdlog::set_level(::spdlog::level::trace)
#else
  #define SKYNET_ENABLE_TRACE_LOG() (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
  #define SKYNET_ENABLE_DEBUG_LOG() ::spdlog::set_level(::spdlog::level::debug)
#else
  #define SKYNET_ENABLE_DEBUG_LOG() (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
  #define SKYNET_ENABLE_WARN_LOG() ::spdlog::set_level(::spdlog::level::warn)
#else
  #define SKYNET_ENABLE_WARN_LOG() (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
  #define SKYNET_ENABLE_ERROR_LOG() ::spdlog::set_level(::spdlog::level::error)
#else
  #define SKYNET_ENABLE_ERROR_LOG() (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
  #define SKYNET_ENABLE_CRITICAL_LOG() ::spdlog::set_level(::spdlog::level::critical)
#else
  #define SKYNET_ENABLE_CRITICAL_LOG() (void)0
#endif

// Automatically enable the set logging level; this is something that's too easy
// to forget to do
namespace skynet::internal::detail
{
  #if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
    static inline const auto dummy = []{ SKYNET_ENABLE_TRACE_LOG(); return 0; }();
  #elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
    static inline const auto dummy = []{ SKYNET_ENABLE_DEBUG_LOG(); return 0; }();
  #elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_WARN
    static inline const auto dummy = []{ SKYNET_ENABLE_WARN_LOG(); return 0; }();
  #elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_ERROR
    static inline const auto dummy = []{ SKYNET_ENABLE_ERROR_LOG(); return 0; }();
  #elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_CRITICAL
    static inline const auto dummy = []{ SKYNET_ENABLE_CRITICAL_LOG(); return 0; }();
  #endif
}

#endif // SKYNET_ENABLE_LOGGING_HPP
