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
// tests/Google/chapter-23/chapter-23_testsuit-1/23.2.1--header-lifetime-automatic.sv
//
// THE SOURCE
//
//   21: module mh_default;
//   22: endmodule
//
//   24: module automatic mh_auto;
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module header defines ... The default lifetime (static or automatic)
// of subroutines defined within the module" (23.2.1), and Syntax 23-1 places
// an optional [lifetime] right after module_keyword. mh_auto carries the
// "automatic" lifetime keyword; mh_default carries none.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors        no fatal, syntax or error diagnostic
//   BothModulesAreAccepted       no such diagnostic on lines 21-25
//   ModuleMhDefaultIsDeclared    exactly one module named mh_default
//   ModuleMhDefaultIsOnLines21To22
//   ModuleMhDefaultIsNotAutomatic   no lifetime keyword: not automatic
//   ModuleMhAutoIsDeclared        exactly one module named mh_auto
//   ModuleMhAutoIsOnLines24To25
//   ModuleMhAutoIsAutomatic       "automatic" keyword: automatic lifetime
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - What the default lifetime actually resolves to (static, per 6.21) for
//     mh_default's subroutines. mh_default declares no subroutine, so there
//     is nothing in the file for that default to apply to; this test only
//     asserts that the module itself is not flagged automatic.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
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
constexpr std::string_view kSourceFileName = "23.2.1--header-lifetime-automatic.sv";
constexpr uint32_t kFirstLine = 21;
constexpr uint32_t kLastLine = 25;
constexpr uint32_t kDefaultFirstLine = 21;
constexpr uint32_t kDefaultLastLine = 22;
constexpr uint32_t kAutoFirstLine = 24;
constexpr uint32_t kAutoLastLine = 25;

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

const hldb::Module *findModuleByName(const hldb::ModuleCollection *modules, std::string_view name,
                                     std::string_view sourceFileName) {
  if (modules == nullptr) return nullptr;
  for (const hldb::Module *module : *modules) {
    if (endsWith(module->getFile(), sourceFileName) && (module->getName() == name)) return module;
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

class HeaderLifetimeAutomaticTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "header_lifetime_automatic.hlc"}); }
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

  static const hldb::Module *findMhDefault() {
    return (m_design == nullptr) ? nullptr : findModuleByName(m_design->getAllModules(), "mh_default", kSourceFileName);
  }
  static const hldb::Module *findMhAuto() {
    return (m_design == nullptr) ? nullptr : findModuleByName(m_design->getAllModules(), "mh_auto", kSourceFileName);
  }
};

TEST_F(HeaderLifetimeAutomaticTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(HeaderLifetimeAutomaticTest, BothModulesAreAccepted) { EXPECT_EQ(eventsOnLines(kFirstLine, kLastLine), ""); }

TEST_F(HeaderLifetimeAutomaticTest, ModuleMhDefaultIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  const hldb::Module *const module = findMhDefault();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "mh_default");
}

TEST_F(HeaderLifetimeAutomaticTest, ModuleMhDefaultIsOnLines21To22) {
  const hldb::Module *const module = findMhDefault();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getStartLine(), kDefaultFirstLine);
  EXPECT_EQ(module->getEndLine(), kDefaultLastLine);
}

TEST_F(HeaderLifetimeAutomaticTest, ModuleMhDefaultIsNotAutomatic) {
  const hldb::Module *const module = findMhDefault();
  ASSERT_NE(module, nullptr);
  EXPECT_FALSE(module->getAutomatic());
}

TEST_F(HeaderLifetimeAutomaticTest, ModuleMhAutoIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  const hldb::Module *const module = findMhAuto();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "mh_auto");
}

TEST_F(HeaderLifetimeAutomaticTest, ModuleMhAutoIsOnLines24To25) {
  const hldb::Module *const module = findMhAuto();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getStartLine(), kAutoFirstLine);
  EXPECT_EQ(module->getEndLine(), kAutoLastLine);
}

TEST_F(HeaderLifetimeAutomaticTest, ModuleMhAutoIsAutomatic) {
  const hldb::Module *const module = findMhAuto();
  ASSERT_NE(module, nullptr);
  EXPECT_TRUE(module->getAutomatic());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
