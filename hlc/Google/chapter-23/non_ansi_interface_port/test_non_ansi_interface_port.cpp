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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-interface-port.sv
//
// THE SOURCE
//
//   20: interface bus;
//   21: endinterface
//   22:
//   23: module m(b);
//   24:   bus b;
//   25: endmodule
//   26:
//   27: module top;
//   28:   bus i();
//   29:   m u(i);
//   30: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// A non-ANSI port can be declared "as an interface port (see Clause 25)"
// (23.2.2.1), with "interface_port_declaration ::= interface_identifier
// list_of_interface_identifiers" (Syntax 23-3). Line 24 declares b as a
// port of the named interface type bus. "An interface port cannot be left
// unconnected" (23.3.3.4), so top connects it to the interface instance i;
// top is then the only module that is not instantiated, hence the only
// top-level module (23.3.1).
// From the object model (37.14): "vpiPortType shall be one of the following
// three types: vpiPort, vpiInterfacePort, or vpiModportPort" (detail 1),
// and "vpiLowConn of a vpiInterfacePort shall always be vpiRefObj"
// (detail 5).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors           no fatal, syntax or error diagnostic
//   ModuleMHasOneInterfacePortB     one port b, vpiPortType vpiInterfacePort
//   InterfacePortLowConnIsARefObj   its vpiLowConn is a vpiRefObj
//   TopIsTheOnlyTopLevelInstance    elaborated runs only: one top-level
//                                   instance, top
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The connection made by u. That is 23.3.2.1 (connection by order).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-interface-port.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

template <typename T>
size_t countFromSourceFile(const std::vector<T *> *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) ++count;
    }
  }
  return count;
}

template <typename T>
const T *findFromSourceFile(const std::vector<T *> *collection, std::string_view name) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName) && (object->getName() == name)) return object;
    }
  }
  return nullptr;
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

class NonAnsiInterfacePortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_interface_port.hlc"}); }
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

  static const hldb::Port *findPortB() {
    if (m_design == nullptr) return nullptr;
    const hldb::Module *const module = findFromSourceFile(m_design->getAllModules(), "m");
    if ((module == nullptr) || (module->getPorts() == nullptr) || (module->getPorts()->size() != 1)) return nullptr;
    return module->getPorts()->front();
  }
};

TEST_F(NonAnsiInterfacePortTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(NonAnsiInterfacePortTest, ModuleMHasOneInterfacePortB) {
  const hldb::Port *const port = findPortB();
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "b");
  EXPECT_EQ(integerProperty(port, vpiPortType), static_cast<int64_t>(vpiInterfacePort));
}

TEST_F(NonAnsiInterfacePortTest, InterfacePortLowConnIsARefObj) {
  const hldb::Port *const port = findPortB();
  ASSERT_NE(port, nullptr);
  const hldb::Any *const lowConn = relatedObject(port, vpiLowConn);
  ASSERT_NE(lowConn, nullptr);
  EXPECT_EQ(lowConn->getVpiType(), static_cast<uint32_t>(vpiRefObj));
}

TEST_F(NonAnsiInterfacePortTest, TopIsTheOnlyTopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    ASSERT_EQ(countFromSourceFile(m_design->getTopModules()), 1u);
    EXPECT_NE(findFromSourceFile(m_design->getTopModules(), "top"), nullptr);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
