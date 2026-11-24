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

// Test for tests/Google/chapter-23/23.2.2.4--default-value-scope.sv
//
// THE SOURCE
//
//   21: parameter logic [7:0] My_DataIn = 8'hFF;
//   22:
//   23: module bus_conn(output logic [7:0] dataout,
//   24:                 input [7:0] datain = My_DataIn);
//   25:   assign dataout = datain;
//   26: endmodule
//   27:
//   28: module bus_connect1(output logic [31:0] dataout,
//   29:                     input [7:0] datain);
//   30:   parameter logic [7:0] My_DataIn = 8'h00;
//   31:   bus_conn bconn0(dataout[31:24], 8'h0F);
//   32:   bus_conn bconn1(dataout[23:16]);
//   33:   bus_conn bconn2(dataout[15:8], My_DataIn);
//   34:   bus_conn bconn3(dataout[7:0]);
//   35: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// This is the LRM's own example for 23.2.2.4: "A module declaration may
// specify a default value for each singular input port. These default values
// shall be constant expressions evaluated in the scope of the module where
// they are defined, not in the scope of the instantiating module." "When the
// module is instantiated, input ports with default values can be omitted from
// the instantiation, and the compiler shall insert the corresponding default
// values." Its annotations:
//
//   bconn0: "Constant literal overrides default in bus_conn definition"
//   bconn1: "Omitted port for datain, default parameter value 8'hFF in
//            bus_conn used"
//   bconn2: "The parameter value 8'h00 from the instantiating scope is used"
//   bconn3: like bconn1
//
// Every line is legal: the default My_DataIn is a parameter, hence a constant
// expression (11.2.1), on an ANSI input port of the singular type logic
// [7:0]. bus_conn has two ports, dataout (output, index 0) and datain
// (input, index 1, 37.14 detail 9). datain has no port kind and an implicit
// data type, so it is a net of the default net type (23.2.2.3).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   BusConnPortsAreDeclared    dataout (getPortIndex() 0) and datain
//                              (getPortIndex() 1)
//   DatainIsANet               the low connection of datain
//                              (Port::getLowConn()) resolves to a net
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which value each instance's datain is connected to (8'h0F, the default
//     8'hFF, the instantiating scope's 8'h00, the default 8'hFF). Default
//     insertion happens when the module is instantiated, so it needs an
//     elaborated design, which this test flow does not produce; it is
//     deferred to the 23.3 batches.
//   - Simulation values of dataout; values are a simulator's business.
//   - The directions output of dataout and input of datain. HLDB gives them
//     only as the integer codes returned by Port::getDirection() and has no
//     named values of its own for directions, so they can be checked once
//     HLDB provides such names.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.4--default-value-scope.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
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

class DefaultValueScopeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "default_value_scope.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no event listener";
    std::string description;
    for (const Event &event : m_session->getEventListener()->getEvents()) {
      if (isErrorClass(event)) description += describeEvent(event);
    }
    return description;
  }

  // The ports of the definition bus_conn, or nullptr.
  static const hldb::PortCollection *findBusConnPorts() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == "bus_conn")) return module->getPorts();
    }
    return nullptr;
  }
};

TEST_F(DefaultValueScopeTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(DefaultValueScopeTest, BusConnPortsAreDeclared) {
  const hldb::PortCollection *const ports = findBusConnPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  EXPECT_EQ((*ports)[0]->getName(), "dataout");
  EXPECT_EQ((*ports)[0]->getPortIndex(), 0);
  EXPECT_EQ((*ports)[1]->getName(), "datain");
  EXPECT_EQ((*ports)[1]->getPortIndex(), 1);
}

TEST_F(DefaultValueScopeTest, DatainIsANet) {
  const hldb::PortCollection *const ports = findBusConnPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  ASSERT_NE((*ports)[1]->getLowConn(), nullptr);
  const hldb::RefObj *const lowConn = any_cast<hldb::RefObj>((*ports)[1]->getLowConn());
  ASSERT_NE(lowConn, nullptr);
  EXPECT_NE(lowConn->getActual<hldb::Net>(), nullptr);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
