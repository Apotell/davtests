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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.2--ansi-output-variable-initialized.sv
//
// THE SOURCE
//
//   20: module m(output var logic x = 1'b1);
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "ansi_port_declaration ::= ... | [ variable_port_header ] port_identifier
// { variable_dimension } [ = constant_expression ]" (Syntax 23-4), and its
// footnote 2: "It shall be illegal to initialize a port that is not a
// variable output port". x is declared with the keyword var, so it is a
// variable output port, and its initializer "= 1'b1" is legal.
// 23.2.2.2--ansi-output-net-initialized.sv is the illegal net counterpart.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   PortXIsAnOutputVariable    one port x, vpiOutput, whose vpiLowConn
//                              resolves to a variable
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The initial value itself. Variable initialization is 6.8, not the
//     port declaration.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.2--ansi-output-variable-initialized.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// True for an error-class event: one whose severity is fatal, syntax or error.
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

class AnsiOutputVariableInitializedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ansi_output_variable_initialized.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every error-class event, described as by describeEvent(); empty when there is none.
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }
};

TEST_F(AnsiOutputVariableInitializedTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(AnsiOutputVariableInitializedTest, PortXIsAnOutputVariable) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_NE(m_design->getAllModules(), nullptr);
  const hldb::Port *port = nullptr;
  for (const hldb::Module *module : *m_design->getAllModules()) {
    if (endsWith(module->getFile(), kSourceFileName) && (module->getPorts() != nullptr) &&
        (module->getPorts()->size() == 1)) {
      port = module->getPorts()->front();
    }
  }
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), std::string_view("x"));
  EXPECT_EQ(port->getDirection(), vpiOutput);

  const hldb::Any *const connection = port->getLowConn();
  ASSERT_NE(connection, nullptr);
  const hldb::RefObj *const lowConn = any_cast<hldb::RefObj>(connection);
  ASSERT_NE(lowConn, nullptr);
  EXPECT_NE(lowConn->getActual<hldb::Variable>(), nullptr);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
