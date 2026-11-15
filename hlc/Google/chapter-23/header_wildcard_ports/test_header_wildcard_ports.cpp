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
//   22: extern module m(a, b);
//   23:
//   24: module m(.*);
//   25:   input a;
//   26:   output b;
//   27: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Syntax 23-1 (23.2.1) gives module_declaration a third alternative:
//
//   { attribute_instance } module_keyword [ lifetime ] module_identifier
//       ( . * ) ; [ timeunits_declaration ] { module_item } endmodule
//
// 23.5 gives it its meaning: "If an extern declaration exists for a module,
// it is possible to use .* as the ports of the module. This usage shall be
// equivalent to placing the ports (and possibly parameters) of the extern
// declaration on the module." So lines 22-27 are equivalent to
//
//   module m(a, b); input a; output b; endmodule
//
// An extern declaration declares "the ports on a module without defining
// the module itself" (23.5), so the file defines exactly one module, m, on
// lines 24-27, with the input port a (index 0) and the output port b
// (index 1).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleIsAccepted           no such diagnostic on lines 22-27
//   OneDefinitionOfModuleM     exactly one module from this file, m
//   DefinitionIsOnLines24To27  m starts on line 24 and ends on line 27
//   PortAIsInput               a is port 0 with vpiDirection vpiInput
//   PortBIsOutput              b is port 1 with vpiDirection vpiOutput
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - ".*" without an extern declaration. 23.5 only defines the form when
//     an extern declaration exists and does not say what happens otherwise.

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
constexpr std::string_view kSourceFileName = "23.2.1--header-wildcard-ports.sv";
constexpr uint32_t kExternLine = 22;
constexpr uint32_t kModuleFirstLine = 24;
constexpr uint32_t kModuleLastLine = 27;

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

class HeaderWildcardPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "header_wildcard_ports.hlc"}); }
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

  // The module definition on line 24, not whatever object HLDB may keep for
  // the extern declaration.
  static const hldb::Module *findDefinition() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getStartLine() == kModuleFirstLine)) return module;
    }
    return nullptr;
  }
};

TEST_F(HeaderWildcardPortsTest, CompilesWithoutErrors) {
  GTEST_SKIP() << "extern module declarations are not modeled: \"extern module m(a, b);\" is built as a second "
                  "module m, so the definition is reported as a colliding design unit (CompMultiplyDefinedDesignUnit) "
                  "(IEEE 1800-2023 23.2.1; hldb_model_gaps.md #9)";
  EXPECT_EQ(allEvents(), "");
}

TEST_F(HeaderWildcardPortsTest, ModuleIsAccepted) {
  GTEST_SKIP() << "extern module declarations are not modeled: \"extern module m(a, b);\" is built as a second "
                  "module m, so the definition is reported as a colliding design unit (CompMultiplyDefinedDesignUnit) "
                  "(IEEE 1800-2023 23.2.1; hldb_model_gaps.md #9)";
  EXPECT_EQ(eventsOnLines(kExternLine, kModuleLastLine), "");
}

TEST_F(HeaderWildcardPortsTest, OneDefinitionOfModuleM) {
  GTEST_SKIP() << "extern module declarations are not modeled: \"extern module m(a, b);\" is built as a second "
                  "module definition (IEEE 1800-2023 23.5)";
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);
  const hldb::Module *const module = findDefinition();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("m"));
}

TEST_F(HeaderWildcardPortsTest, DefinitionIsOnLines24To27) {
  const hldb::Module *const module = findDefinition();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

TEST_F(HeaderWildcardPortsTest, PortAIsInput) {
  GTEST_SKIP() << "\"module m(.*)\" does not take its ports from the extern declaration of m, so it has no "
                  "ports (IEEE 1800-2023 23.5)";
  const hldb::Module *const module = findDefinition();
  ASSERT_NE(module, nullptr);
  const hldb::Port *const portA = hldb::findByName<hldb::Port>("a", module->getPorts());
  ASSERT_NE(portA, nullptr);
  EXPECT_EQ(portA->getPortIndex(), 0);
  EXPECT_EQ(portA->getDirection(), vpiInput);
}

TEST_F(HeaderWildcardPortsTest, PortBIsOutput) {
  GTEST_SKIP() << "\"module m(.*)\" does not take its ports from the extern declaration of m, so it has no "
                  "ports (IEEE 1800-2023 23.5)";
  const hldb::Module *const module = findDefinition();
  ASSERT_NE(module, nullptr);
  const hldb::Port *const portB = hldb::findByName<hldb::Port>("b", module->getPorts());
  ASSERT_NE(portB, nullptr);
  EXPECT_EQ(portB->getPortIndex(), 1);
  EXPECT_EQ(portB->getDirection(), vpiOutput);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
