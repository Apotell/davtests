/*
 Copyright 2019 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   Logger.h
 *
 * spdlog-free facade over the logging backend, so spdlog stays a PRIVATE
 * dependency and never leaks into the installed Surelog package. The fmt-style
 * macro API lives in the .cpp-only header LoggerMacros.h.
 */

#ifndef SURELOG_LOGGER_H
#define SURELOG_LOGGER_H
#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>

#ifndef EOF
    #define EOF (-1)
#endif

namespace SURELOG {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal, Off };

// Full definition in the internal header Surelog/Common/LoggingConfig.h; only a
// reference is needed in this public facade, so spdlog/json stay private.
struct LoggingConfig;

// Owns the logging backend. Optionally, one per Session; initialize() registers it as
// spdlog's default so the HLC_* macros reach it without a Session handle.
class Logger {
 public:
  Logger();
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  // Opens the single log file and registers the backend as spdlog's default,
  // applying the per-sink configuration from hlc.conf. Returns false if the
  // path is empty or the file cannot be opened.
  //
  // Level mapping (set on the loggers, not on the shared file sink, so the
  // verbatim report/dumps always reach the file):
  //   config.m_fileSink.level -> HLC_* macro stream/Logger::log (stdout + stderr + file).
  //   config.m_consoleSink.level -> Logger::log()/LogStream (stdout + stderr).
  //   The diagnostic report itself is filtered by the per-severity switches in
  //   CommandLineParser (driven by config.m_fileSink.m_level), not here.
  //
  // Rotation applies only to the file sink, from config.fileSink:
  //   maxSizeBytes > 0 -> size-based roll when the file exceeds that size.
  //   rotateOnOpen     -> per-run: roll the previous log aside at startup.
  //   maxFiles         -> backups to keep (0 = none). With neither trigger set
  //                       the file is appended, unbounded.
  bool initialize(const LoggingConfig &config, const std::filesystem::path &filePath);
  bool initialized() const;

  void log(LogLevel level, std::string_view msg);
  void trace(std::string_view msg) { log(LogLevel::Trace, msg); }
  void debug(std::string_view msg) { log(LogLevel::Debug, msg); }
  void info(std::string_view msg) { log(LogLevel::Info, msg); }
  void warn(std::string_view msg) { log(LogLevel::Warn, msg); }
  void error(std::string_view msg) { log(LogLevel::Error, msg); }
  void critical(std::string_view msg) { log(LogLevel::Fatal, msg); }

  // Sets the threshold for the console logger.
  void setLevel(LogLevel level);

  // Flush all loggers
  void flush();

 protected:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

// std::ostream front-end for the Logger: write to it with operator<< and each
// complete line (terminated by '\n' or std::flush) is forwarded to Logger::log
// immediately, keeping the intermediate buffer small. Any trailing content
// without a final newline is flushed at destruction.
class LogStream : public std::ostream {
 public:
  explicit LogStream(Logger *logger, LogLevel level) : std::ostream(&m_buf), m_buf(logger, level) {}
  ~LogStream() override = default;
  LogStream(const LogStream &) = delete;
  LogStream &operator=(const LogStream &) = delete;

 private:
  // Streambuf that forwards one Logger::log() call per line.
  class LineBuf : public std::streambuf {
   public:
    LineBuf(Logger *logger, LogLevel level) : m_logger(logger), m_level(level) {}
    ~LineBuf() override { emit(); }

   protected:
    // Single-character path (fallback when xsputn is not called).
    int overflow(int c) override {
      if (c == EOF) return EOF;
      if (c == '\n') {
        emit();
      } else {
        m_line += static_cast<char>(c);
      }
      return c;
    }

    // Bulk-write path: scan the incoming chunk for newlines.
    std::streamsize xsputn(const char *s, std::streamsize n) override {
      const char *p = s;
      const char *const end = s + n;
      while (p < end) {
        const char *nl = std::char_traits<char>::find(p, static_cast<std::size_t>(end - p), '\n');
        if (nl) {
          m_line.append(p, nl);
          emit();
          p = nl + 1;
        } else {
          m_line.append(p, end);
          break;
        }
      }
      return n;
    }

    // std::flush / std::endl triggers sync().
    int sync() override {
      emit();
      return 0;
    }

   private:
    void emit() {
      if (!m_line.empty()) {
        m_logger->log(m_level, m_line);
        m_line.clear();
      }
    }

    std::string m_line;
    Logger *const m_logger = nullptr;
    const LogLevel m_level = LogLevel::Off;
  };

  LineBuf m_buf;
};
}  // namespace SURELOG

#endif  // SURELOG_LOGGER_H
