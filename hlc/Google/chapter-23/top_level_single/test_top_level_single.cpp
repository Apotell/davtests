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

// Test for tests/Google/chapter-23/23.3.1--top-level-single.sv
//
// THE SOURCE
//
//   19: module top;
//   20:   wire w;
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Top-level modules are modules that are included in the SystemVerilog
// source text, but do not appear in any module instantiation statement"
// (23.3.1). "A design shall contain at least one top-level module. A
// top-level module is implicitly instantiated once, and its instance name is
// the same as the module name. Such an instance is called a top-level
// instance." (23.3.1). The object model of an instance (37.10) has a name, a
// definition name and a top-module flag, which HLDB gives as getName(),
// getDefName() and getTopModule(); getTopModule() is TRUE for a top-level
// module instance. The elaborated instance holds its own copy of
// the module's nets (37.10, relation net).
//
// Instances exist only after elaboration (23.3). The HLC install of
// 2026-11-23 no longer gives tests an elaborator (hldb::Elaborator was
// removed), so the test checks the design as Compile() leaves it; the
// instance checks fail until HLC builds the instance tree itself.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors         no fatal, syntax or error event
//   DesignIsElaborated            the design is marked elaborated
//   OneTopLevelInstance           exactly one top-level instance
//   TopInstanceIsNamedTop         its getName() is top, the module name
//   TopInstanceIsOfModuleTop      its getDefName() is top
//   TopInstanceIsATopModule       its getTopModule() is TRUE
//   TopInstanceHasNetW            the instance holds net w
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Nothing else: the fixture has a single module and a single net.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.3.1--top-level-single.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

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

class TopLevelSingleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "top_level_single.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }

  // The top-level instances from the file under test.
  static std::vector<const hldb::Module *> topInstances() {
    std::vector<const hldb::Module *> result;
    if ((m_design == nullptr) || (m_design->getTopModules() == nullptr)) return result;
    for (const hldb::Module *module : *m_design->getTopModules()) {
      if (endsWith(module->getFile(), kSourceFileName)) result.emplace_back(module);
    }
    return result;
  }

  // The names of 'instances', comma-separated, for failure messages.
  static std::string namesOf(const std::vector<const hldb::Module *> &instances) {
    std::string result;
    for (const hldb::Module *instance : instances) {
      if (!result.empty()) result += ", ";
      result += std::string(instance->getName());
    }
    return result;
  }
};

TEST_F(TopLevelSingleTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(TopLevelSingleTest, DesignIsElaborated) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getElaborated());
}

TEST_F(TopLevelSingleTest, OneTopLevelInstance) {
  const std::vector<const hldb::Module *> tops = topInstances();
  EXPECT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
}

TEST_F(TopLevelSingleTest, TopInstanceIsNamedTop) {
  const std::vector<const hldb::Module *> tops = topInstances();
  ASSERT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
  EXPECT_EQ(tops.front()->getName(), "top");
}

TEST_F(TopLevelSingleTest, TopInstanceIsOfModuleTop) {
  const std::vector<const hldb::Module *> tops = topInstances();
  ASSERT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
  EXPECT_EQ(tops.front()->getDefName(), "top");
}

TEST_F(TopLevelSingleTest, TopInstanceIsATopModule) {
  const std::vector<const hldb::Module *> tops = topInstances();
  ASSERT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
  EXPECT_TRUE(tops.front()->getTopModule());
}

TEST_F(TopLevelSingleTest, TopInstanceHasNetW) {
  const std::vector<const hldb::Module *> tops = topInstances();
  ASSERT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
  const hldb::NetCollection *const nets = tops.front()->getNets();
  ASSERT_NE(nets, nullptr);
  const bool found =
      std::any_of(nets->cbegin(), nets->cend(), [](const hldb::Net *net) { return net->getName() == "w"; });
  EXPECT_TRUE(found);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
