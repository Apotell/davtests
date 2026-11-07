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
// tests/Google/chapter-23/chapter-23_testsuit-1/23.2.1--header-ansi-basic.sv
//
// THE SOURCE
//
//   20: module m(input a, output b);
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The ANSI header style makes the declarations of the port characteristics
// part of the module header." (23.2.1). "Each port declaration provides the
// complete information about the port. The port's direction, width, net or
// variable type, and signedness are completely described." (23.2.2.2). a
// and b get the same vpiPort/vpiDirection representation that the
// non-ANSI-declared ports of 23.2.1--header-non-ansi-basic.sv get, since
// the VPI object model does not distinguish the two header styles.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleIsAccepted           no such diagnostic on lines 20-21
//   ModuleMIsDeclared          exactly one module from this file, named m
//   ModuleIsOnLines20To21      it starts on line 20 and ends on line 21
//   PortAIsInput               a is a vpiPort with vpiDirection vpiInput
//   PortBIsOutput              b is a vpiPort with vpiDirection vpiOutput
//   ExactlyTwoPorts            no port besides a and b
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The vpiLowConn relation from each port to its net: see
//     23.2.1--header-non-ansi-basic.sv.

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
constexpr std::string_view kSourceFileName = "23.2.1--header-ansi-basic.sv";
constexpr uint32_t kModuleFirstLine = 20;
constexpr uint32_t kModuleLastLine = 21;

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

class HeaderAnsiBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "header_ansi_basic.hlc"}); }
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

TEST_F(HeaderAnsiBasicTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(HeaderAnsiBasicTest, ModuleIsAccepted) { EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), ""); }

TEST_F(HeaderAnsiBasicTest, ModuleMIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "m");
}

TEST_F(HeaderAnsiBasicTest, ModuleIsOnLines20To21) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

TEST_F(HeaderAnsiBasicTest, PortAIsInput) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::Port *const portA = findPortByName(module->getPorts(), "a");
  ASSERT_NE(portA, nullptr);
  EXPECT_EQ(portA->getVpiType(), static_cast<uint32_t>(vpiPort));
  EXPECT_EQ(integerProperty(portA, vpiDirection), static_cast<int64_t>(vpiInput));
}

TEST_F(HeaderAnsiBasicTest, PortBIsOutput) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::Port *const portB = findPortByName(module->getPorts(), "b");
  ASSERT_NE(portB, nullptr);
  EXPECT_EQ(integerProperty(portB, vpiDirection), static_cast<int64_t>(vpiOutput));
}

TEST_F(HeaderAnsiBasicTest, ExactlyTwoPorts) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::PortCollection *const ports = module->getPorts();
  ASSERT_NE(ports, nullptr);
  EXPECT_EQ(ports->size(), 2u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
