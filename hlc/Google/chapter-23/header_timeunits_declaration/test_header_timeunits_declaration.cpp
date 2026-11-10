/*
 Copyright 2020 Apotell

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

// Test for
// tests/Google/chapter-23/chapter-23_testsuit-1/23.2.1--header-timeunits-declaration.sv
//
// THE SOURCE
//
//   19: module m;
//   20:   timeunit 1ns;
//   21:   timeprecision 1ps;
//   22: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Syntax 23-1 places an optional [timeunits_declaration] right after the
// module header: "module_nonansi_header [timeunits_declaration]
// {module_item} endmodule". "timeunit 1ns; timeprecision 1ps;" sets this
// module's own time unit and precision (22.12 covers lines; the `timeunit`
// and `timeprecision` keywords themselves are 3.14). The object model
// reports both as vpi_get() properties vpiTimeUnit and vpiTimePrecision
// (38.6), each encoded as the power-of-ten exponent relative to one second,
// so 1ns is -9 and 1ps is -12.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleIsAccepted           no such diagnostic on lines 19-22
//   ModuleMIsDeclared          exactly one module from this file, named m
//   ModuleIsOnLines19To22      it starts on line 19 and ends on line 22
//   TimeUnitIsOneNanosecond    vpiTimeUnit is -9
//   TimePrecisionIsOnePicosecond   vpiTimePrecision is -12
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The "No timescale set" diagnostic that a module without a
//     timeunits_declaration gets (seen on the other fixtures in this
//     suite). It is a warning, not an error-class diagnostic, and this
//     file's whole point is that m does carry one.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.1--header-timeunits-declaration.sv";
constexpr uint32_t kModuleFirstLine = 19;
constexpr uint32_t kModuleLastLine = 22;

// vpi_get(vpiTimeUnit, ...) / vpi_get(vpiTimePrecision, ...) return the time
// unit/precision as the power-of-ten exponent relative to one second (38.6).
constexpr int32_t kOneNanosecond = -9;
constexpr int32_t kOnePicosecond = -12;

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

template <typename T>
size_t countFromSourceFile(const std::vector<T *> *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) ++count;
    }
  }
  return count;
}

template <typename T>
const T *findFromSourceFile(const std::vector<T *> *collection) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) return object;
    }
  }
  return nullptr;
}

uint32_t lineOf(const Event &error) { return error.getArguments().empty() ? 0 : error.getArguments().front().m_line; }

bool isErrorClass(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

std::string describeError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != nullptr) description += ": " + std::string(errorInfo->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class HeaderTimeunitsDeclarationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "header_timeunits_declaration.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string errorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      const uint32_t line = lineOf(error);
      if (isErrorClass(error) && (line >= firstLine) && (line <= lastLine)) description += describeError(error);
    }
    return description;
  }

  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      if (isErrorClass(error)) description += describeError(error);
    }
    return description;
  }

  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }
};

TEST_F(HeaderTimeunitsDeclarationTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(HeaderTimeunitsDeclarationTest, ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

TEST_F(HeaderTimeunitsDeclarationTest, ModuleMIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "m");
}

TEST_F(HeaderTimeunitsDeclarationTest, ModuleIsOnLines19To22) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

TEST_F(HeaderTimeunitsDeclarationTest, TimeUnitIsOneNanosecond) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getTimeUnit(), kOneNanosecond);
}

TEST_F(HeaderTimeunitsDeclarationTest, TimePrecisionIsOnePicosecond) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getTimePrecision(), kOnePicosecond);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
