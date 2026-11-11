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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.2--ansi-shared-attributes.sv
//
// THE SOURCE
//
//   20: module test(input [7:0] a,
//   21:             input signed [7:0] b, c, d,
//   22:             output [7:0] e,
//   23:             output var signed [7:0] f, g,
//   24:             output signed [7:0] h);
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// 23.2.2.2 gives this as the ANSI form of 23.2.2.1 Example 1: "Multiple
// ports that share all attributes can be declared together." "Every
// attribute of the declaration shall be in the one declaration."
//   - The ports are a to h in that order, port indices 0 to 7 (37.14
//     detail 9).
//   - a to d are inputs, e to h outputs.
//   - The keyword var makes f and g variables; the others have no port kind
//     and an implicit data type, so they default to nets (23.2.2.3).
//   - b, c, d share "signed", as do f and g; h is signed; a and e are not.
//     Nets and variables report this through vpiSigned (vpi_user.h).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   EightPortsInOrder          a to h, vpiPortIndex 0 to 7
//   DirectionsAreShared        a-d vpiInput, e-h vpiOutput
//   FAndGAreVariablesOthersNets  vpiLowConn resolves to a variable for f
//                              and g, to a net for the others
//   SignednessIsShared         vpiSigned TRUE except for a and e
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - vpiSize. Every port is [7:0]; port size is covered by
//     23.2.2.2--ansi-explicit-named.sv.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.2--ansi-shared-attributes.sv";

struct ExpectedPort {
  std::string_view m_name;
  int64_t m_direction;
  bool m_variable;
  bool m_signed;
};

const std::vector<ExpectedPort> kExpectedPorts = {
    {"a", vpiInput, false, false}, {"b", vpiInput, false, true},   {"c", vpiInput, false, true},
    {"d", vpiInput, false, true},  {"e", vpiOutput, false, false}, {"f", vpiOutput, true, true},
    {"g", vpiOutput, true, true},  {"h", vpiOutput, false, true},
};

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

const hldb::Any *relatedObject(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  if (const hldb::Any *const single = std::get<1>(result)) return single;
  const std::vector<const hldb::Any *> *const objects = std::get<2>(result);
  return ((objects != nullptr) && (objects->size() == 1)) ? objects->front() : nullptr;
}

// The object a port's low connection refers to, or nullptr.
const hldb::Any *lowConnActual(const hldb::Port *port) {
  const hldb::Any *const connection = relatedObject(port, vpiLowConn);
  if (connection == nullptr) return nullptr;
  const hldb::RefObj *const reference = any_cast<hldb::RefObj>(connection);
  return (reference == nullptr) ? nullptr : reference->getActual();
}

const hldb::Port *findPortByName(const hldb::PortCollection *ports, std::string_view name) {
  if (ports == nullptr) return nullptr;
  for (const hldb::Port *port : *ports) {
    if (port->getName() == name) return port;
  }
  return nullptr;
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

class AnsiSharedAttributesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ansi_shared_attributes.hlc"}); }
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

  static const hldb::PortCollection *findPorts() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName)) return module->getPorts();
    }
    return nullptr;
  }
};

TEST_F(AnsiSharedAttributesTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(AnsiSharedAttributesTest, EightPortsInOrder) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  EXPECT_EQ(ports->size(), kExpectedPorts.size());
  for (size_t index = 0; index < kExpectedPorts.size(); ++index) {
    const hldb::Port *const port = findPortByName(ports, kExpectedPorts[index].m_name);
    ASSERT_NE(port, nullptr) << kExpectedPorts[index].m_name;
    EXPECT_EQ(integerProperty(port, vpiPortIndex), static_cast<int64_t>(index)) << kExpectedPorts[index].m_name;
  }
}

TEST_F(AnsiSharedAttributesTest, DirectionsAreShared) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = findPortByName(findPorts(), expected.m_name);
    ASSERT_NE(port, nullptr) << expected.m_name;
    EXPECT_EQ(integerProperty(port, vpiDirection), expected.m_direction) << expected.m_name;
  }
}

TEST_F(AnsiSharedAttributesTest, FAndGAreVariablesOthersNets) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = findPortByName(findPorts(), expected.m_name);
    ASSERT_NE(port, nullptr) << expected.m_name;
    const hldb::Any *const actual = lowConnActual(port);
    ASSERT_NE(actual, nullptr) << expected.m_name;
    if (expected.m_variable) {
      EXPECT_NE(any_cast<hldb::Variable>(actual), nullptr) << expected.m_name;
    } else {
      EXPECT_NE(any_cast<hldb::Net>(actual), nullptr) << expected.m_name;
    }
  }
}

TEST_F(AnsiSharedAttributesTest, SignednessIsShared) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = findPortByName(findPorts(), expected.m_name);
    ASSERT_NE(port, nullptr) << expected.m_name;
    const hldb::Any *const actual = lowConnActual(port);
    ASSERT_NE(actual, nullptr) << expected.m_name;
    EXPECT_EQ(integerProperty(actual, vpiSigned), expected.m_signed ? 1 : 0) << expected.m_name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
