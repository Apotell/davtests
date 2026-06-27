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
 * File:   LoggingConfig.h
 *
 * Typed logging configuration loaded from the JSON file "hlc.conf" (shipped
 * next to the executable). Replaces the former -log_* command-line arguments.
 * Each sink carries its own log level; rotation applies only to the file sink.
 *
 * Public header: CommandLineParser holds a LoggingConfig by value, so this is
 * installed alongside it. It depends only on Logger.h (LogLevel) and the
 * standard library -- no spdlog/json -- so it stays a clean facade. (Logger.h
 * itself only forward-declares LoggingConfig.)
 *
 * To add a new sink: add a <Name>SinkConfig struct + a field on LoggingConfig,
 * a parse block in loadLoggingConfig(), and a logger wired to it in
 * Logger::initialize().
 */

#ifndef SURELOG_LOGGINGCONFIG_H
#define SURELOG_LOGGINGCONFIG_H
#pragma once

#include <Surelog/Common/Logger.h>  // LogLevel

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace SURELOG {

// Common to every sink.
struct SinkConfig {
  LogLevel m_level = LogLevel::Info;
};

// The rotating log file. Rotation knobs live here only.
struct FileSinkConfig : SinkConfig {
  std::size_t m_maxSizeBytes = 50 * 1024 * 1024;  // "log-max-size": 50MB
  std::size_t m_maxFiles = 100;                   // "log-max-files"
  bool m_rotateOnOpen = false;                    // forced true in Debug builds (see isDebugBuild)
};

// stdout (the CoutStream / Logger::console output).
struct ConsoleSinkConfig : SinkConfig {};

struct LoggingConfig {
  FileSinkConfig m_fileSink;
  ConsoleSinkConfig m_consoleSink;
};

// True for Debug builds, false for Release. Single abstraction point for
// debug-build detection (keyed on NDEBUG); used to force rotate-on-open.
constexpr bool isDebugBuild() {
#ifdef NDEBUG
  return false;
#else
  return true;
#endif
}

// Loads logging settings from an already-opened input stream (the caller opens
// hlc.conf through the FileSystem VFS). A not-good stream (missing file),
// missing fields, or malformed JSON all fall back to the documented defaults
// (a default-constructed LoggingConfig). Never throws. Human-readable problems
// are appended to `warnings` (the logger is not up yet, so the caller emits
// them after initialization). The Debug rotate-on-open rule is applied here, so
// it holds regardless of the stream contents.
void loadLoggingConfig(std::istream &in, LoggingConfig &config);

}  // namespace SURELOG

#endif  // SURELOG_LOGGINGCONFIG_H
