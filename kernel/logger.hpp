/**
 * @file loffer.hpp
 *
 * @brief Kernel Logger implementation.
 */

#pragma once

enum LogLevel
{
    kError = 3,
    kWarn = 4,
    kInfo = 6,
    kDebug = 7,
};

/**
 * @brief Set the global log level for the kernel logger.
 * The logger will only output messages with a level greater than or equal to this value.
 */
void SetLogLevel(LogLevel level);

/**
 * @brief Log a message with the specified log level.
 *
 * @param level The log level of the message.
 * @param format The format string for the message, compat to printk.
 * @param ... Additional arguments for the format string.
 */
int Log(LogLevel level, const char *format, ...);