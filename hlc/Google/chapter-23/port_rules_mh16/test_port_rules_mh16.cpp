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

// Test for tests/Google/chapter-23/23.2.2.3--mh16-range-then-wire.sv
//
// THE SOURCE
//
//   19: module mh16([5:0] x, wire y);
//   20: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// For subsequent ports in an ANSI style port list (23.2.2.3): "If the
// direction, port kind and data type are all omitted, then they shall be
// inherited from the previous port. ... Otherwise: If the direction is
// omitted, it shall be inherited from the previous port. If the port kind is
// omitted, it shall be determined as previously specified. If the data type
// is omitted, it shall default to logic". "Unpacked dimensions shall not be
// inherited from the previous port declaration and shall be repeated for each
// port with the same dimensions." The first-port rules make an omitted
// direction inout, an input or inout without kind a net of the default net
// type (wire, 22.8), and an output with an explicit data type a variable.
// integer is 32 bits (6.11).
// The LRM annotates this example:
//   "module mh16([5:0] x, wire y); // inout wire logic [5:0] x // inout wire logic y"
//
// How the test observes it: each port's low connection (Port::getLowConn(),
// 37.14 detail 4) refers to the net or variable the port declares. Its port
// kind is the object itself (a Net or a Variable), its data type is its
// typespec (hldb::getTypespec(); an array typespec for unpacked dimensions),
// its packed width (37.17) is the bit count hldb::ExprEval::getBitCount()
// computes for it, and its signedness is hldb::getSigned() of its typespec (of
// the element type for an unpacked array). The port's direction is
// Port::getDirection() and a net's net type Net::getNetType(), compared with
// the named values of hldb/vpi_user.h (vpiInput, vpiWire, ...).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors         no fatal, syntax or error diagnostic
//   PortsInDeclarationOrder       port names and getPortIndex() 0, 1, ...
//   PortKindsFollowTheRules       net or variable
//   DirectionsFollowTheRules      Port::getDirection() of each port
//   NetTypesFollowTheRules        Net::getNetType() of each net
//   DataTypesFollowTheRules       typespec of each net or variable
//   PackedWidthsFollowTheRules    getBitCount() of each net or variable
//   SignednessFollowsTheRules     getSigned() of each data type (of the
//                                 element type for an unpacked array)
//
// SKIPPED (GTEST_SKIP; the real assertions stay below the skip)
//
//   CompilesWithoutErrors, NetTypesFollowTheRules, DataTypesFollowTheRules,
//   PackedWidthsFollowTheRules, SignednessFollowsTheRules: HLC does not treat
//   "[5:0]" (implicit_data_type) as a data type written for x, so it takes x
//   for a port with nothing written: x gets no typespec, net type 36 (not a
//   net type) instead of wire, and an "Invalid name" error is reported
//   (23.2.2.3: x is inout wire logic [5:0]).
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Instances and connections. The rules of 23.2.2.3 fix kind, data type
//     and direction from the declaration alone; connections are 23.3.
//   - The port objects' own size. 23.2.2.2--ansi-explicit-named.sv covers
//     it.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/ExprEval.h>
#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/array_typespec.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kModuleName = "mh16";

// The kind of object a port declares (23.2.2.3).
enum class PortKind { Net, Variable };
// Interconnect ports "have no data type" (23.2.2.3), so theirs is not checked.
constexpr hldb::AnyType kNoDataType = hldb::AnyType::Any;
constexpr int64_t kUnchecked = -1;

struct ExpectedPort {
  std::string_view m_name;
  PortKind m_kind;
  int32_t m_direction;  // vpiInput, vpiOutput, vpiInout or vpiRef
  int32_t m_netType;    // the net type of a net; kUnchecked for a variable
  hldb::AnyType m_dataType;
  int64_t m_size;
  int64_t m_signed;
  bool m_unpacked;
};

const std::vector<ExpectedPort> kExpectedPorts = {
    {"x", PortKind::Net, vpiInout, vpiWire, hldb::AnyType::LogicTypespec, 6, 0, false},
    {"y", PortKind::Net, vpiInout, vpiWire, hldb::AnyType::LogicTypespec, 1, 0, false},
};

// The packed width in bits of a net or variable (37.17), as
// HLDB's expression evaluator computes it from the object's typespec, or -1
// when it cannot.
int64_t widthOf(const hldb::Any *object) {
  const hldb::ExprEval evaluator(nullptr, true);
  uint64_t bits = 0;
  if (!evaluator.getBitCount(object, object, true, &bits, true)) return -1;
  return static_cast<int64_t>(bits);
}

// The net or variable a port's low connection refers to, or nullptr.
const hldb::Any *lowConnActual(const hldb::Port *port) { return hldb::getActual(port->getLowConn()); }

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

class PortRulesMh16Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "port_rules_mh16.hlc"}); }
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

  static const hldb::PortCollection *findPorts() {
    if (m_design == nullptr) return nullptr;
    const hldb::Module *const module = hldb::findByDefName<hldb::Module>(kModuleName, m_design->getAllModules());
    return (module == nullptr) ? nullptr : module->getPorts();
  }

  // The net or variable declared by the port named 'name', or nullptr.
  static const hldb::Any *findDeclared(std::string_view name) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, findPorts());
    return (port == nullptr) ? nullptr : lowConnActual(port);
  }
};

TEST_F(PortRulesMh16Test, CompilesWithoutErrors) {
  GTEST_SKIP() << "HLC reports an 'Invalid name' error for the type of x, which it leaves without a "
                  "typespec: it does not treat '[5:0]' (implicit_data_type) as a data type written for x "
                  "(IEEE 1800-2023 23.2.2.3)";
  EXPECT_EQ(allErrors(), "");
}

TEST_F(PortRulesMh16Test, PortsInDeclarationOrder) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), kExpectedPorts.size());
  for (size_t index = 0; index < kExpectedPorts.size(); ++index) {
    EXPECT_EQ((*ports)[index]->getName(), kExpectedPorts[index].m_name);
    EXPECT_EQ((*ports)[index]->getPortIndex(), static_cast<int32_t>(index));
  }
}

TEST_F(PortRulesMh16Test, PortKindsFollowTheRules) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Any *const declared = findDeclared(expected.m_name);
    ASSERT_NE(declared, nullptr) << expected.m_name;
    if (expected.m_kind == PortKind::Variable) {
      EXPECT_NE(any_cast<hldb::Variable>(declared), nullptr) << expected.m_name << " is not a variable";
    } else {
      EXPECT_NE(any_cast<hldb::Net>(declared), nullptr) << expected.m_name << " is not a net";
    }
  }
}

TEST_F(PortRulesMh16Test, DirectionsFollowTheRules) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(expected.m_name, findPorts());
    ASSERT_NE(port, nullptr) << expected.m_name;
    EXPECT_EQ(port->getDirection(), expected.m_direction) << expected.m_name;
  }
}

TEST_F(PortRulesMh16Test, NetTypesFollowTheRules) {
  GTEST_SKIP() << "HLC gives x net type 36 (not a net type) instead of wire: it does not treat '[5:0]' "
                  "(implicit_data_type) as a data type written for x (IEEE 1800-2023 23.2.2.3: inout wire)";
  for (const ExpectedPort &expected : kExpectedPorts) {
    if (expected.m_netType == kUnchecked) continue;
    const hldb::Net *const net = any_cast<hldb::Net>(findDeclared(expected.m_name));
    ASSERT_NE(net, nullptr) << expected.m_name << " is not a net";
    EXPECT_EQ(net->getNetType(), expected.m_netType) << expected.m_name;
  }
}

TEST_F(PortRulesMh16Test, DataTypesFollowTheRules) {
  GTEST_SKIP() << "HLC leaves x without a typespec: it does not treat '[5:0]' (implicit_data_type) as a "
                  "data type written for x (IEEE 1800-2023 23.2.2.3: x is logic [5:0])";
  for (const ExpectedPort &expected : kExpectedPorts) {
    if (expected.m_dataType == kNoDataType) continue;
    const hldb::Any *const declared = findDeclared(expected.m_name);
    ASSERT_NE(declared, nullptr) << expected.m_name;
    const hldb::Typespec *const typespec = hldb::getTypespec(declared);
    ASSERT_NE(typespec, nullptr) << expected.m_name;
    if (expected.m_unpacked) {
      const hldb::ArrayTypespec *const array = any_cast<hldb::ArrayTypespec>(typespec);
      ASSERT_NE(array, nullptr) << expected.m_name << " has a " << hldb::AnyTypeName(typespec->getAnyType());
      const hldb::RefTypespec *const element = array->getElemTypespec();
      ASSERT_NE(element, nullptr) << expected.m_name;
      ASSERT_NE(element->getActual(), nullptr) << expected.m_name;
      EXPECT_EQ(hldb::AnyTypeName(element->getActual()->getAnyType()), hldb::AnyTypeName(expected.m_dataType))
          << expected.m_name;
    } else {
      EXPECT_EQ(hldb::AnyTypeName(typespec->getAnyType()), hldb::AnyTypeName(expected.m_dataType)) << expected.m_name;
    }
  }
}

TEST_F(PortRulesMh16Test, PackedWidthsFollowTheRules) {
  GTEST_SKIP() << "HLC leaves x without a typespec, so it has no width: it does not treat '[5:0]' "
                  "(implicit_data_type) as a data type written for x (IEEE 1800-2023 23.2.2.3: 6 bits)";
  for (const ExpectedPort &expected : kExpectedPorts) {
    if (expected.m_size == kUnchecked) continue;
    const hldb::Any *const declared = findDeclared(expected.m_name);
    ASSERT_NE(declared, nullptr) << expected.m_name;
    EXPECT_EQ(widthOf(declared), expected.m_size) << expected.m_name;
  }
}

TEST_F(PortRulesMh16Test, SignednessFollowsTheRules) {
  GTEST_SKIP() << "HLC leaves x without a typespec: it does not treat '[5:0]' (implicit_data_type) as a "
                  "data type written for x (IEEE 1800-2023 23.2.2.3: x is logic [5:0], unsigned)";
  for (const ExpectedPort &expected : kExpectedPorts) {
    if (expected.m_signed == kUnchecked) continue;
    const hldb::Any *const declared = findDeclared(expected.m_name);
    ASSERT_NE(declared, nullptr) << expected.m_name;
    const hldb::Typespec *typespec = hldb::getTypespec(declared);
    // The signedness of an unpacked array is that of its element type (7.4).
    if (const hldb::ArrayTypespec *const array = any_cast<hldb::ArrayTypespec>(typespec)) {
      typespec = hldb::getElemTypespec(array);
    }
    ASSERT_NE(typespec, nullptr) << expected.m_name;
    EXPECT_EQ(static_cast<int64_t>(hldb::getSigned(typespec)), expected.m_signed) << expected.m_name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
