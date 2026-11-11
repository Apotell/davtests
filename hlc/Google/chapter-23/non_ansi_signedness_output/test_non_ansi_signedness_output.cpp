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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-signedness-output.sv
//
// THE SOURCE
//
//   20: module test(e, f, g, h);
//   21:   output [7:0] e;
//   22:   output [7:0] f;
//   23:   output signed [7:0] g;
//   24:   output signed [7:0] h;
//   25:   logic signed [7:0] f;
//   26:   logic [7:0] g;
//   27: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// This is the output half of Example 1 of 23.2.2.1, whose annotations are:
//
//   output [7:0] e;        // no explicit net declaration - net is unsigned
//   output signed [7:0] h; // no explicit net declaration - net is signed
//   logic signed [7:0] f;  // port f inherits signed attribute from logic decl.
//   logic [7:0] g;         // logic g inherits signed attribute from port
//
// "If either the port or the net/variable is declared as signed, then the
// other shall also be considered signed." (23.2.2.1). e and h have no
// declaration besides the port declaration, so they are nets; f and g are
// declared with the data type logic, so they are variables. The object model
// gives nets and variables the Boolean vpiSigned (vpi_user.h: "TRUE for
// vpiIODecl and any object in the expression class if the object has the
// signed attribute").
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   FourOutputPorts            e, f, g, h, all vpiOutput
//   EAndHAreNets               the vpiLowConn of e and h resolve to nets
//   FAndGAreVariables          the vpiLowConn of f and g resolve to variables
//   OnlyEIsUnsigned            vpiSigned FALSE for e, TRUE for f, g and h
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The ranges. Every declaration uses [7:0], so they agree, as 23.2.2.1
//     requires.

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
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-signedness-output.sv";

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

class NonAnsiSignednessOutputTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_signedness_output.hlc"}); }
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

  static const hldb::Port *findPort(std::string_view name) {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName)) return findPortByName(module->getPorts(), name);
    }
    return nullptr;
  }
};

TEST_F(NonAnsiSignednessOutputTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(NonAnsiSignednessOutputTest, FourOutputPorts) {
  for (std::string_view name : {"e", "f", "g", "h"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    EXPECT_EQ(integerProperty(port, vpiDirection), static_cast<int64_t>(vpiOutput)) << name;
  }
}

TEST_F(NonAnsiSignednessOutputTest, EAndHAreNets) {
  for (std::string_view name : {"e", "h"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const actual = lowConnActual(port);
    ASSERT_NE(actual, nullptr) << name;
    EXPECT_NE(any_cast<hldb::Net>(actual), nullptr) << name;
  }
}

TEST_F(NonAnsiSignednessOutputTest, FAndGAreVariables) {
  for (std::string_view name : {"f", "g"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const actual = lowConnActual(port);
    ASSERT_NE(actual, nullptr) << name;
    EXPECT_NE(any_cast<hldb::Variable>(actual), nullptr) << name;
  }
}

TEST_F(NonAnsiSignednessOutputTest, OnlyEIsUnsigned) {
  for (std::string_view name : {"e", "f", "g", "h"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const actual = lowConnActual(port);
    ASSERT_NE(actual, nullptr) << name;
    EXPECT_EQ(integerProperty(actual, vpiSigned), (name == "e") ? 0 : 1) << name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
