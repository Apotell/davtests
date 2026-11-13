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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2--module-keyword-basic.sv
//
// THE SOURCE
//
// Lines 1-15 are the license header and a one-line comment citing 23.2. The
// SystemVerilog text is:
//
//   19: module top;
//   20: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "A module definition shall be enclosed between the keywords module and
// endmodule. The identifier following the keyword module shall be the name
// of the module being defined." (23.2). top has no ports, no parameters and
// no items, and is never instantiated, so it is a top-level module: "A
// top-level module is implicitly instantiated once, and its instance name
// is the same as the module name." (23.3.1).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleIsAccepted           no such diagnostic on lines 19-20
//   ReportsNoMisreadingDiagnostics   no syntax/keyword/identifier-length/
//                              non-ASCII diagnostic at any severity
//   ModuleTopIsDeclared        exactly one module from this file, named top
//   ModuleIsOnLines19To20      it starts on line 19 and ends on line 20
//   ModuleTopIsEmpty           no port, net or parameter in it
//   TopIsTheOnlyTopLevelInstance   elaborated runs only: one top-level
//                              instance, top, of the definition at line 19
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Any distinction between "module" and the interchangeable "macromodule"
//     keyword (23.2): that is covered by 23.2--macromodule-keyword-basic.sv.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2--module-keyword-basic.sv";
constexpr uint32_t kModuleFirstLine = 19;
constexpr uint32_t kModuleLastLine = 20;

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

bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

std::string describeEvent(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(event.getType()));
  description += " line " + std::to_string(lineOf(event));
  if (eventDefinition != nullptr) description += ": " + std::string(eventDefinition->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class ModuleKeywordBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "module_keyword_basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string eventsOnLines(uint32_t firstLine, uint32_t lastLine) {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isErrorClass(event) && (line >= firstLine) && (line <= lastLine)) {
        std::vector<std::string> msgs = hldb::renderEvent(event);
        std::copy(msgs.cbegin(), msgs.cend(), std::ostream_iterator<std::string>(oss));
      }
    }
    return oss.str();
  }

  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const hldb::Event &event : listener->getEvents()) {
      if (isErrorClass(event)) {
        std::vector<std::string> msgs = hldb::renderEvent(event);
        std::copy(msgs.cbegin(), msgs.cend(), std::ostream_iterator<std::string>(oss));
      }
    }
    return oss.str();
  }

  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }
};

TEST_F(ModuleKeywordBasicTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ModuleKeywordBasicTest, ModuleIsAccepted) { EXPECT_EQ(eventsOnLines(kModuleFirstLine, kModuleLastLine), ""); }

TEST_F(ModuleKeywordBasicTest, ReportsNoMisreadingDiagnostics) {
  EXPECT_EQ(findEvent(EventType::PpSyntaxError), nullptr);
  EXPECT_EQ(findEvent(EventType::PaSyntaxError), nullptr);
  EXPECT_EQ(findEvent(EventType::PaReservedKeyword), nullptr);
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

TEST_F(ModuleKeywordBasicTest, ModuleTopIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getAnyType(), hldb::AnyType::Module);
  EXPECT_EQ(module->getDefName(), std::string_view("top"));
  EXPECT_EQ(module->getName(), std::string_view("top"));
}

TEST_F(ModuleKeywordBasicTest, ModuleIsOnLines19To20) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

TEST_F(ModuleKeywordBasicTest, ModuleTopIsEmpty) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE((module->getPorts() == nullptr) || module->getPorts()->empty());
  EXPECT_TRUE((module->getNets() == nullptr) || module->getNets()->empty());
  EXPECT_TRUE((module->getParameters() == nullptr) || module->getParameters()->empty());
}

// test is never instantiated, so it is the design's top-level module (23.3.1).
TEST_F(ModuleKeywordBasicTest, TopIsTheOnlyTopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    ASSERT_EQ(countFromSourceFile(m_design->getTopModules()), 1u);
    const hldb::Module *const instance = findFromSourceFile(m_design->getTopModules());
    ASSERT_NE(instance, nullptr);

    EXPECT_EQ(instance->getName(), std::string_view("top"));
    EXPECT_EQ(instance->getDefName(), std::string_view("top"));
    EXPECT_TRUE(endsWith(instance->getDefFile(), kSourceFileName)) << instance->getDefFile();
    EXPECT_EQ(instance->getDefLineNo(), static_cast<int32_t>(kModuleFirstLine));
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
