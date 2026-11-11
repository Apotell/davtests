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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.2--ansi-explicit-named.sv
//
// THE SOURCE
//
//   20: module mymod(output .P1(r[3:0]),
//   21:              output .P2(r[7:4]),
//   22:              ref .Y(x),
//   23:              input R);
//   24:   logic [7:0] r;
//   25:   int x;
//   26: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// This is the mymod example of 23.2.2.2 with its "..." removed. "ANSI style
// port declarations can be explicitly named ... Like explicitly named ports
// in a module port declaration, port identifiers exist in their own name
// space for each port list." "The self-determined type of the port
// expression becomes the type for the port." (23.2.2.2). So:
//   - P1, P2 and Y name ports only; they declare no net or variable.
//   - P1 is r[3:0] and P2 is r[7:4], 4 bits each; Y is x, an int (32 bits);
//     R is an ordinary ANSI input port of the default type logic (1 bit,
//     23.2.2.3).
//   - Directions are as written: output, output, ref, input. vpiRef is the
//     vpiDirection value for ref (sv_vpi_user.h, Annex M).
// From the object model (37.14): explicit names are reported through
// vpiExplicitName; "The first port has a port index of zero" (detail 9);
// the port expression is the low connection (detail 4); and as mymod is
// never instantiated, no port has a high connection (detail 10).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error diagnostic
//   FourPortsInDeclarationOrder    P1, P2, Y, R with vpiPortIndex 0..3
//   ExplicitNamesAreReported       vpiExplicitName TRUE for P1, P2, Y only
//   DirectionsAreAsDeclared        output, output, ref, input
//   PortSizesFollowTheirExpressions   vpiSize 4, 4, 32, 1
//   PortIdentifiersDeclareNoObjects   no net or variable named P1, P2 or Y
//   PortExpressionsAreLowConns     each port has a vpiLowConn and no
//                                  vpiHighConn
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The shape of each low connection (part-select, variable reference).
//     That is the expression model, outside 23.2.2.2.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.2--ansi-explicit-named.sv";

struct ExpectedPort {
  std::string_view m_name;
  bool m_explicitName;
  int64_t m_direction;
  int64_t m_size;
};

const std::vector<ExpectedPort> kExpectedPorts = {
    {"P1", true, vpiOutput, 4},
    {"P2", true, vpiOutput, 4},
    {"Y", true, vpiRef, 32},
    {"R", false, vpiInput, 1},
};

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
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

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

size_t relationSize(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  const std::vector<const hldb::Any *> *const objects = std::get<2>(result);
  if (objects != nullptr) return objects->size();
  return (std::get<1>(result) != nullptr) ? 1 : 0;
}

const hldb::Port *findPortByName(const hldb::PortCollection *ports, std::string_view name) {
  if (ports == nullptr) return nullptr;
  for (const hldb::Port *port : *ports) {
    if (port->getName() == name) return port;
  }
  return nullptr;
}

template <typename T>
bool hasObjectNamed(const std::vector<T *> *collection, std::string_view name) {
  if (collection == nullptr) return false;
  for (const T *object : *collection) {
    if (object->getName() == name) return true;
  }
  return false;
}

uint32_t lineOf(const Event &error) { return error.getArguments().empty() ? 0 : error.getArguments().front().m_line; }

bool isErrorClass(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

std::string describeError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != nullptr) description += ": " + std::string(errorInfo->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class AnsiExplicitNamedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ansi_explicit_named.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      if (isErrorClass(error)) description += describeError(error);
    }
    return description;
  }

  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  static const hldb::Port *findPort(std::string_view name) {
    const hldb::Module *const module = findModule();
    return (module == nullptr) ? nullptr : findPortByName(module->getPorts(), name);
  }
};

TEST_F(AnsiExplicitNamedTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(AnsiExplicitNamedTest, FourPortsInDeclarationOrder) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "mymod");
  ASSERT_NE(module->getPorts(), nullptr);
  EXPECT_EQ(module->getPorts()->size(), kExpectedPorts.size());
  for (size_t index = 0; index < kExpectedPorts.size(); ++index) {
    const hldb::Port *const port = findPort(kExpectedPorts[index].m_name);
    ASSERT_NE(port, nullptr) << kExpectedPorts[index].m_name;
    EXPECT_EQ(integerProperty(port, vpiPortIndex), static_cast<int64_t>(index)) << kExpectedPorts[index].m_name;
  }
}

TEST_F(AnsiExplicitNamedTest, ExplicitNamesAreReported) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = findPort(expected.m_name);
    ASSERT_NE(port, nullptr) << expected.m_name;
    EXPECT_EQ(integerProperty(port, vpiExplicitName), expected.m_explicitName ? 1 : 0) << expected.m_name;
  }
}

TEST_F(AnsiExplicitNamedTest, DirectionsAreAsDeclared) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = findPort(expected.m_name);
    ASSERT_NE(port, nullptr) << expected.m_name;
    EXPECT_EQ(integerProperty(port, vpiDirection), expected.m_direction) << expected.m_name;
  }
}

TEST_F(AnsiExplicitNamedTest, PortSizesFollowTheirExpressions) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = findPort(expected.m_name);
    ASSERT_NE(port, nullptr) << expected.m_name;
    EXPECT_EQ(integerProperty(port, vpiSize), expected.m_size) << expected.m_name;
  }
}

TEST_F(AnsiExplicitNamedTest, PortIdentifiersDeclareNoObjects) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  for (std::string_view name : {"P1", "P2", "Y"}) {
    EXPECT_FALSE(hasObjectNamed(module->getNets(), name)) << "net " << name;
    EXPECT_FALSE(hasObjectNamed(module->getVariables(), name)) << "variable " << name;
  }
}

TEST_F(AnsiExplicitNamedTest, PortExpressionsAreLowConns) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = findPort(expected.m_name);
    ASSERT_NE(port, nullptr) << expected.m_name;
    EXPECT_EQ(relationSize(port, vpiLowConn), 1u) << expected.m_name;
    EXPECT_EQ(relationSize(port, vpiHighConn), 0u) << expected.m_name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
