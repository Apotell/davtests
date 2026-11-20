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

// Test for tests/Google/chapter-23/23.2.4--items-instances.sv
//
// THE SOURCE
//
//   20: primitive udp_buf(output o, input i);
//   21:   table
//   22:     0 : 0;
//   23:     1 : 1;
//   24:   endtable
//   25: endprimitive
//   26:
//   27: interface ifc;
//   28: endinterface
//   29:
//   30: program prg;
//   31: endprogram
//   32:
//   33: module child;
//   34: endmodule
//   35:
//   36: module m;
//   37:   wire a, b, c, o1, o2;
//   38:   and g1(o1, a, b);
//   39:   udp_buf u1(o2, c);
//   40:   child c1();
//   41:   ifc i1();
//   42:   prg p1();
//   43: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module definition can contain zero or more module items." (23.2.4).
// Syntax 23-5: module_or_generate_item ::= ... | gate_instantiation |
// udp_instantiation | module_instantiation | ..., and module_common_item ::=
// ... | interface_instantiation | program_instantiation. The object model of
// a module (37.5) reaches gate and UDP instances through primitive
// (vpiPrimitive): g1 is a vpiGate whose vpiPrimType is vpiAndPrim, and u1 is a
// vpiUdp whose definition name (vpiDefName) is udp_buf.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   GateG1IsAnAndGate          vpiPrimitive holds g1, a vpiGate of vpiAndPrim
//   UdpU1IsAnInstanceOfUdpBuf  vpiPrimitive holds u1, a vpiUdp of udp_buf
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The module, interface and program instances c1, i1 and p1. Instances
//     are created during elaboration (23.3), which this test flow does not
//     run.
//   - The warning that discourages programs. It is a warning, not an error.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
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
constexpr std::string_view kSourceFileName = "23.2.4--items-instances.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

std::string stringProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const char *const *const text = std::get_if<const char *>(&value)) {
    return (*text == nullptr) ? std::string("<null>") : std::string(*text);
  }
  return "<not a string>";
}

// The objects 'object' has in the VPI relation 'relation', as vpi_iterate()
// or vpi_handle() would return them (37.3).
std::vector<const hldb::Any *> relationObjects(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  if (const std::vector<const hldb::Any *> *const objects = std::get<2>(result)) return *objects;
  if (const hldb::Any *const single = std::get<1>(result)) return {single};
  return {};
}

// The object named 'name' among 'objects', or nullptr.
const hldb::Any *findNamed(const std::vector<const hldb::Any *> &objects, std::string_view name) {
  for (const hldb::Any *object : objects) {
    if (object->getName() == name) return object;
  }
  return nullptr;
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

class ItemsInstancesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "items_instances.hlc"}); }
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

TEST_F(ItemsInstancesTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ItemsInstancesTest, GateG1IsAnAndGate) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::Any *const gate = findNamed(relationObjects(module, vpiPrimitive), "g1");
  ASSERT_NE(gate, nullptr);
  EXPECT_EQ(gate->getVpiType(), static_cast<uint32_t>(vpiGate));
  EXPECT_EQ(integerProperty(gate, vpiPrimType), static_cast<int64_t>(vpiAndPrim));
}

TEST_F(ItemsInstancesTest, UdpU1IsAnInstanceOfUdpBuf) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::Any *const udp = findNamed(relationObjects(module, vpiPrimitive), "u1");
  ASSERT_NE(udp, nullptr);
  EXPECT_EQ(udp->getVpiType(), static_cast<uint32_t>(vpiUdp));
  EXPECT_EQ(stringProperty(udp, vpiDefName), "udp_buf");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
