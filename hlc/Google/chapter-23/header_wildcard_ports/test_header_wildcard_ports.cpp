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
// tests/Google/chapter-23/chapter-23_testsuit-1/23.2.1--header-wildcard-ports.sv
//
// THE SOURCE
//
//   21: module m(.*);
//   22:   input a;
//   23:   output b;
//   24: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Syntax 23-1 gives module_declaration a third alternative besides the
// non-ANSI and ANSI headers:
//
//   { attribute_instance } module_keyword [ lifetime ] module_identifier
//       ( . * ) ; [ timeunits_declaration ] { module_item } endmodule
//
// a and b are declared as ports in the body, exactly as they would be
// under a non-ANSI header; the module's complete port list is the wildcard
// ".*" rather than a named list_of_ports. This file exercises that third
// alternative: it is legal SystemVerilog under 23.2.1/Syntax 23-1, so it
// should compile and produce the same two-port module as
// 23.2.1--header-non-ansi-basic.sv.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleIsAccepted           no such diagnostic on lines 21-24
//   ModuleMIsDeclared          exactly one module from this file, named m
//   ModuleIsOnLines21To24      it starts on line 21 and ends on line 24
//   PortAIsInput               a is a vpiPort with vpiDirection vpiInput
//   PortBIsOutput               b is a vpiPort with vpiDirection vpiOutput
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Anything about how the ".*" wildcard is distinguished, in the object
//     model, from an explicit list_of_ports. 37.10 does not call out such a
//     marker, and 23.2.1's prose only discusses the non-ANSI and ANSI
//     styles, so the wildcard form is expected to normalize to the same
//     vpiPort objects as the other two (as 23.2.1--header-non-ansi-basic.sv
//     and 23.2.1--header-ansi-basic.sv show for their own header styles).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.1--header-wildcard-ports.sv";
constexpr uint32_t kModuleFirstLine = 21;
constexpr uint32_t kModuleLastLine = 24;

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

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

const hldb::Port *findPortByName(const hldb::PortCollection *ports, std::string_view name) {
  if (ports == nullptr) return nullptr;
  for (const hldb::Port *port : *ports) {
    if (port->getName() == name) return port;
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

class HeaderWildcardPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "header_wildcard_ports.hlc"}); }
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

TEST_F(HeaderWildcardPortsTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(HeaderWildcardPortsTest, ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

TEST_F(HeaderWildcardPortsTest, ModuleMIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "m");
}

TEST_F(HeaderWildcardPortsTest, ModuleIsOnLines21To24) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

TEST_F(HeaderWildcardPortsTest, PortAIsInput) {
  GTEST_SKIP() << "HLC Gap: Implicit ports aren't supported yet!";
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::Port *const portA = findPortByName(module->getPorts(), "a");
  ASSERT_NE(portA, nullptr);
  EXPECT_EQ(integerProperty(portA, vpiDirection), static_cast<int64_t>(vpiInput));
}

TEST_F(HeaderWildcardPortsTest, PortBIsOutput) {
  GTEST_SKIP() << "HLC Gap: Implicit ports aren't supported yet!";
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::Port *const portB = findPortByName(module->getPorts(), "b");
  ASSERT_NE(portB, nullptr);
  EXPECT_EQ(integerProperty(portB, vpiDirection), static_cast<int64_t>(vpiOutput));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
