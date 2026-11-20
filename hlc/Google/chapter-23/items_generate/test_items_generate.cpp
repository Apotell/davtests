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

// Test for tests/Google/chapter-23/23.2.4--items-generate.sv
//
// THE SOURCE
//
//   20: module m;
//   21:   generate
//   22:     for (genvar i = 0; i < 2; i++) begin : g_loop
//   23:       wire w;
//   24:     end
//   25:   endgenerate
//   26:   if (1) begin : g_if
//   27:     wire x;
//   28:   end
//   29:   case (1)
//   30:     1: begin : g_case
//   31:       wire y;
//   32:     end
//   33:   endcase
//   34: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module definition can contain zero or more module items." (23.2.4).
// Syntax 23-5: non_port_module_item ::= generate_region | ..., and
// module_common_item ::= ... | loop_generate_construct |
// conditional_generate_construct. The file uses a generate region holding a
// loop generate construct, an if generate construct and a case generate
// construct, all legal module items.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   ModuleMIsDeclared          the module m is defined
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The generate blocks g_loop[0], g_loop[1], g_if and g_case and their
//     nets. Generate schemes are evaluated during elaboration (27.3), which
//     this test flow does not run; they are left to Clause 27 tests.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.4--items-generate.sv";

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

class ItemsGenerateTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "items_generate.hlc"}); }
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

  // The module named 'name' defined in the file under test, or nullptr.
  static const hldb::Module *findModule(std::string_view name) {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == name)) return module;
    }
    return nullptr;
  }
};

TEST_F(ItemsGenerateTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ItemsGenerateTest, ModuleMIsDeclared) { EXPECT_NE(findModule("m"), nullptr); }
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
