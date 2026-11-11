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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.2--ansi-generic-interface.sv
//
// THE SOURCE
//
//   21: interface bus;
//   22: endinterface
//   23:
//   24: module cpuMod(interface d, interface j);
//   25: endmodule
//   26:
//   27: module top;
//   28:   bus b();
//   29:   cpuMod u(.d(b), .j(b));
//   30: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Generic interface ports (see 25.3.3) cannot be declared using the
// non-ANSI style list_of_ports syntax ... Generic interface ports can only
// be declared using the ANSI style list_of_port_declarations syntax."
// (23.2.2.2, with the cpuMod example). "An interface port cannot be left
// unconnected" (23.3.3.4), so top connects both ports to the interface
// instance b; that makes top the only module that is not instantiated, so
// it is the design's only top-level module (23.3.1).
// From the object model (37.14): "vpiPortType shall be one of the following
// three types: vpiPort, vpiInterfacePort, or vpiModportPort" (detail 1),
// "vpiLowConn of a vpiInterfacePort shall always be vpiRefObj" (detail 5),
// and "The first port has a port index of zero" (detail 9).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors           no fatal, syntax or error diagnostic
//   CpuModHasPortsDAndJ             two ports, d and j, indices 0 and 1
//   BothAreInterfacePorts           vpiPortType vpiInterfacePort
//   InterfacePortLowConnsAreRefObjs each vpiLowConn is a vpiRefObj
//   TopIsTheOnlyTopLevelInstance    elaborated runs only: one top-level
//                                   instance, top
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - A direction for d and j. The LRM gives an interface port none; HLC's
//     warnings about a missing direction are warnings, not errors, and are
//     not asserted either way.
//   - The connections made by u. They are 23.3.2.2 (connection by name).

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
constexpr std::string_view kSourceFileName = "23.2.2.2--ansi-generic-interface.sv";

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

class AnsiGenericInterfaceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ansi_generic_interface.hlc"}); }
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

  static const hldb::Port *findCpuModPort(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    const hldb::Module *const module = findFromSourceFile(m_design->getAllModules(), "cpuMod");
    return (module == nullptr) ? nullptr : findPortByName(module->getPorts(), name);
  }
};

TEST_F(AnsiGenericInterfaceTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(AnsiGenericInterfaceTest, CpuModHasPortsDAndJ) {
  ASSERT_NE(m_design, nullptr);
  const hldb::Module *const module = findFromSourceFile(m_design->getAllModules(), "cpuMod");
  ASSERT_NE(module, nullptr);
  ASSERT_NE(module->getPorts(), nullptr);
  EXPECT_EQ(module->getPorts()->size(), 2u);

  const std::vector<std::string_view> names = {"d", "j"};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::Port *const port = findCpuModPort(names[index]);
    ASSERT_NE(port, nullptr) << names[index];
    EXPECT_EQ(integerProperty(port, vpiPortIndex), static_cast<int64_t>(index)) << names[index];
  }
}

TEST_F(AnsiGenericInterfaceTest, BothAreInterfacePorts) {
  for (std::string_view name : {"d", "j"}) {
    const hldb::Port *const port = findCpuModPort(name);
    ASSERT_NE(port, nullptr) << name;
    EXPECT_EQ(integerProperty(port, vpiPortType), static_cast<int64_t>(vpiInterfacePort)) << name;
  }
}

TEST_F(AnsiGenericInterfaceTest, InterfacePortLowConnsAreRefObjs) {
  for (std::string_view name : {"d", "j"}) {
    const hldb::Port *const port = findCpuModPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const lowConn = relatedObject(port, vpiLowConn);
    ASSERT_NE(lowConn, nullptr) << name;
    EXPECT_EQ(lowConn->getVpiType(), static_cast<uint32_t>(vpiRefObj)) << name;
  }
}

TEST_F(AnsiGenericInterfaceTest, TopIsTheOnlyTopLevelInstance) {
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
