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
// THE MODEL
//
// HLC's graph is a definition model: it must hold everything the source
// says, so that an instance model can be built from it. The checks are about
// binding, not connection:
//   - datain's default (Net::getValue()) is bound to the compilation-unit
//     parameter My_DataIn of line 21, the scope where bus_conn is defined,
//     not to bus_connect1's My_DataIn of line 30.
//   - Each instance's arguments are ordered (NamedArgument::getConnByName()
//     false), and each is bound to the port of bus_conn in its position.
//   - bconn2's actual My_DataIn is bound to bus_connect1's parameter, the
//     instantiating scope.
//   - bconn1 and bconn3 have no argument for datain.
// Which value each instance's datain finally receives is an instance-model
// question; that check is its own test, skipped until HLC builds an
// elaborated model (m_design->getElaborated()).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error diagnostic
//   BusConnPortsAreDeclared        dataout (getPortIndex() 0) and datain
//                                  (getPortIndex() 1)
//   BusConnPortDirections          dataout vpiOutput, datain vpiInput
//   DatainIsANet                   the low connection of datain
//                                  (Port::getLowConn()) resolves to a net
//   DatainNetType                  Net::getNetType() of datain is vpiWire
//   BusConnPortSignedness          getSigned() of the data type of dataout
//                                  and of datain is false
//   DatainDefaultIsTheUnitParameter  Net::getValue() of datain refers to the
//                                  parameter My_DataIn of the compilation unit
//   InstancesBindOrderedArguments  bconn0..bconn3: ordered arguments, each
//                                  bound to the port in its position; bconn0
//                                  passes 8'h0F, bconn2 bus_connect1's
//                                  My_DataIn; bconn1 and bconn3 omit datain.
//   InstancesReceiveDatainValues   elaborated model only: datain of bconn0..
//                                  bconn3 is connected to 8'h0F, 8'hFF,
//                                  8'h00, 8'hFF
//
// SKIPPED (GTEST_SKIP; the real assertion stays below the skip)
//
//   InstancesReceiveDatainValues: HLC builds a definition model only, so
//   there is no instance of bus_conn whose datain could hold a value.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Simulation values of dataout; values are a simulator's business.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec.h>
#include <hldb/unit.h>
#include <hldb/vpi_user.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {
namespace {
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

// One instance of bus_conn in bus_connect1, as the source writes it.
struct ExpectedInstance {
  std::string_view m_name;
  size_t m_argumentCount;               // ordered arguments written
  std::string_view m_datainActual;      // decompiled constant, "My_DataIn", or "" when omitted
  std::string_view m_elaboratedDatain;  // value datain receives in the instance model
};

const ExpectedInstance kExpectedInstances[] = {
    {"bconn0", 2, "8'h0F", "8'h0F"},
    {"bconn1", 1, "", "8'hFF"},
    {"bconn2", 2, "My_DataIn", "8'h00"},
    {"bconn3", 1, "", "8'hFF"},
};
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

  // The definition named 'name', or nullptr.
  static const hldb::Module *findDefinition(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  // The ports of the definition bus_conn, or nullptr.
  static const hldb::PortCollection *findBusConnPorts() {
    const hldb::Module *const module = findDefinition("bus_conn");
    return (module == nullptr) ? nullptr : module->getPorts();
  }

  // The net port datain of bus_conn declares, or nullptr.
  static const hldb::Net *findDatainNet() {
    const hldb::Port *const port = hldb::findByName<hldb::Port>("datain", findBusConnPorts());
    return (port == nullptr) ? nullptr : any_cast<hldb::Net>(hldb::getActual(port->getLowConn()));
  }
};

TEST_F(DefaultValueScopeTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(DefaultValueScopeTest, BusConnPortsAreDeclared) {
  const hldb::PortCollection *const ports = findBusConnPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  EXPECT_EQ((*ports)[0]->getName(), std::string_view("dataout"));
  EXPECT_EQ((*ports)[0]->getPortIndex(), 0);
  EXPECT_EQ((*ports)[1]->getName(), std::string_view("datain"));
  EXPECT_EQ((*ports)[1]->getPortIndex(), 1);
}

TEST_F(DefaultValueScopeTest, BusConnPortDirections) {
  const hldb::PortCollection *const ports = findBusConnPorts();
  const hldb::Port *const dataout = hldb::findByName<hldb::Port>("dataout", ports);
  const hldb::Port *const datain = hldb::findByName<hldb::Port>("datain", ports);
  ASSERT_NE(dataout, nullptr);
  ASSERT_NE(datain, nullptr);
  EXPECT_EQ(dataout->getDirection(), vpiOutput);
  EXPECT_EQ(datain->getDirection(), vpiInput);
}

TEST_F(DefaultValueScopeTest, DatainIsANet) {
  const hldb::Port *const datain = hldb::findByName<hldb::Port>("datain", findBusConnPorts());
  ASSERT_NE(datain, nullptr);
  ASSERT_NE(datain->getLowConn(), nullptr);
  const hldb::RefObj *const lowConn = any_cast<hldb::RefObj>(datain->getLowConn());
  ASSERT_NE(lowConn, nullptr);
  EXPECT_NE(lowConn->getActual<hldb::Net>(), nullptr);
}

// datain has no port kind and an implicit data type, so it is a net of the default net type, wire (23.2.2.3).
TEST_F(DefaultValueScopeTest, DatainNetType) {
  const hldb::Net *const net = findDatainNet();
  ASSERT_NE(net, nullptr) << "datain is not a net";
  EXPECT_EQ(net->getNetType(), vpiWire);
}

// logic [7:0] and the implicit [7:0] are both unsigned (6.11.3).
TEST_F(DefaultValueScopeTest, BusConnPortSignedness) {
  const hldb::PortCollection *const ports = findBusConnPorts();
  for (const std::string_view name : {"dataout", "datain"}) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, ports);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const declared = hldb::getActual(port->getLowConn());
    ASSERT_NE(declared, nullptr) << name;
    const hldb::Typespec *const typespec = hldb::getTypespec(declared);
    ASSERT_NE(typespec, nullptr) << name;
    EXPECT_FALSE(hldb::getSigned(typespec)) << name;
  }
}

// "These default values shall be constant expressions evaluated in the scope of the module where they are
// defined, not in the scope of the instantiating module." (23.2.2.4)
TEST_F(DefaultValueScopeTest, DatainDefaultIsTheUnitParameter) {
  const hldb::Net *const net = findDatainNet();
  ASSERT_NE(net, nullptr);
  const hldb::RefObj *const value = any_cast<hldb::RefObj>(net->getValue());
  ASSERT_NE(value, nullptr) << "the default My_DataIn of datain is missing";
  EXPECT_EQ(value->getName(), std::string_view("My_DataIn"));
  const hldb::Parameter *const parameter = value->getActual<hldb::Parameter>();
  ASSERT_NE(parameter, nullptr) << "the default My_DataIn is not bound to a parameter";
  ASSERT_NE(parameter->getParent(), nullptr);
  EXPECT_EQ(parameter->getParent()->getAnyType(), hldb::AnyType::Unit)
      << "the default must bind to the compilation-unit My_DataIn (line 21), not bus_connect1's (line 30)";
}

TEST_F(DefaultValueScopeTest, InstancesBindOrderedArguments) {
  const hldb::Module *const busConnect1 = findDefinition("bus_connect1");
  ASSERT_NE(busConnect1, nullptr);
  for (const ExpectedInstance &expected : kExpectedInstances) {
    const hldb::RefInstance *const instance =
        hldb::findByName<hldb::RefInstance>(expected.m_name, busConnect1->getRefInstances());
    ASSERT_NE(instance, nullptr) << expected.m_name;
    const hldb::NamedArgumentCollection *const arguments = instance->getArguments();
    ASSERT_NE(arguments, nullptr) << expected.m_name;
    ASSERT_EQ(arguments->size(), expected.m_argumentCount) << expected.m_name;

    // Ordered arguments: the n-th argument binds to the n-th port of bus_conn (23.3.2.1).
    const std::string_view portNames[] = {"dataout", "datain"};
    for (size_t index = 0; index < arguments->size(); ++index) {
      const hldb::NamedArgument *const argument = (*arguments)[index];
      EXPECT_FALSE(argument->getConnByName()) << expected.m_name << " argument " << index << " is ordered";
      EXPECT_FALSE(argument->getWildcardConn()) << expected.m_name << " argument " << index;
      const hldb::Port *const port = any_cast<hldb::Port>(hldb::getActual(argument->getLowConn()));
      ASSERT_NE(port, nullptr) << expected.m_name << " argument " << index << " is not bound to a port";
      EXPECT_EQ(port->getName(), portNames[index]) << expected.m_name << " argument " << index;
    }
    EXPECT_NE(any_cast<hldb::PartSelect>((*arguments)[0]->getHighConn()), nullptr)
        << expected.m_name << " passes a part select of dataout";

    if (expected.m_datainActual == "My_DataIn") {
      // The actual is evaluated in the instantiating scope: bus_connect1's My_DataIn.
      const hldb::RefObj *const actual = any_cast<hldb::RefObj>((*arguments)[1]->getHighConn());
      ASSERT_NE(actual, nullptr) << expected.m_name;
      const hldb::Parameter *const parameter = actual->getActual<hldb::Parameter>();
      ASSERT_NE(parameter, nullptr) << expected.m_name;
      EXPECT_EQ(parameter->getParent(), busConnect1) << expected.m_name << " uses bus_connect1's My_DataIn";
    } else if (!expected.m_datainActual.empty()) {
      const hldb::Constant *const actual = any_cast<hldb::Constant>((*arguments)[1]->getHighConn());
      ASSERT_NE(actual, nullptr) << expected.m_name;
      EXPECT_EQ(actual->getDecompile(), expected.m_datainActual) << expected.m_name;
    }
  }
}

// "When the module is instantiated, input ports with default values can be omitted from the instantiation, and
// the compiler shall insert the corresponding default values." (23.2.2.4) In the instance model, datain of each
// instance is connected to the actual, or to the default evaluated in bus_conn's scope.
TEST_F(DefaultValueScopeTest, InstancesReceiveDatainValues) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "HLC builds a definition model only, with no instance of bus_conn whose datain could hold "
                    "a value (IEEE 1800-2023 23.2.2.4: the compiler shall insert the default value of an "
                    "omitted input port)";
  }
  const hldb::Module *const busConnect1 = findDefinition("bus_connect1");
  ASSERT_NE(busConnect1, nullptr);
  for (const ExpectedInstance &expected : kExpectedInstances) {
    const hldb::Module *const module = hldb::findByName<hldb::Module>(expected.m_name, busConnect1->getModules());
    ASSERT_NE(module, nullptr) << expected.m_name;
    const hldb::Port *const datain = hldb::findByName<hldb::Port>("datain", module->getPorts());
    ASSERT_NE(datain, nullptr) << expected.m_name;
    const hldb::Constant *const highConn = any_cast<hldb::Constant>(datain->getHighConn());
    ASSERT_NE(highConn, nullptr) << expected.m_name << ".datain is not connected to a constant";
    EXPECT_EQ(highConn->getDecompile(), expected.m_elaboratedDatain) << expected.m_name;
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
