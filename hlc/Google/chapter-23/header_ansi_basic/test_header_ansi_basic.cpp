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

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
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

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

// True for an error-class event: fatal, syntax or error severity.
bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}
}  // namespace

class HeaderAnsiBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "header_ansi_basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string eventsOnLines(uint32_t firstLine, uint32_t lastLine) {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isErrorClass(event) && (line >= firstLine) && (line <= lastLine)) oss << describeEvent(event);
    }
    return oss.str();
  }

  static std::string allEvents() { return eventsOnLines(0, std::numeric_limits<uint32_t>::max()); }

  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }
};

TEST_F(HeaderAnsiBasicTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(HeaderAnsiBasicTest, ModuleIsAccepted) { EXPECT_EQ(eventsOnLines(kModuleFirstLine, kModuleLastLine), ""); }

TEST_F(HeaderAnsiBasicTest, ModuleMIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("m"));
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
  const hldb::Port *const portA = hldb::findByName<hldb::Port>("a", module->getPorts());
  ASSERT_NE(portA, nullptr);
  EXPECT_EQ(portA->getAnyType(), hldb::AnyType::Port);
  EXPECT_EQ(portA->getDirection(), vpiInput);
}

TEST_F(HeaderAnsiBasicTest, PortBIsOutput) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::Port *const portB = hldb::findByName<hldb::Port>("b", module->getPorts());
  ASSERT_NE(portB, nullptr);
  EXPECT_EQ(portB->getDirection(), vpiOutput);
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
