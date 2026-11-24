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
// a module (37.5) reaches gate and UDP instances through the relation
// primitive, which HLDB gives as Module::getPrimitives(): g1 is a gate (an and
// gate), and u1 is a UDP instance whose definition name is udp_buf.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   G1IsAGate                  getPrimitives() holds g1, a Gate
//   UdpU1IsAnInstanceOfUdpBuf  getPrimitives() holds u1, a Udp whose
//                              getDefName() is udp_buf
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The module, interface and program instances c1, i1 and p1. Instances
//     are created during elaboration (23.3), which this test flow does not
//     run.
//   - The warning that discourages programs. It is a warning, not an error.
//   - That g1 is an and gate. HLDB gives the gate kind only as the integer
//     code returned by Primitive::getPrimType() and has no named values of
//     its own for it, so it can be checked once HLDB provides such names.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/any_type.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/primitive.h>

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

// The object named 'name' in 'collection', or nullptr.
template <typename T>
const T *findNamed(const std::vector<T *> *collection, std::string_view name) {
  if (collection == nullptr) return nullptr;
  for (const T *object : *collection) {
    if (object->getName() == name) return object;
  }
  return nullptr;
}

// The number of objects in 'collection'; zero for a null collection.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  return (collection == nullptr) ? 0 : collection->size();
}

// The type name of 'object' (getAnyType()), as hldb::AnyTypeName() gives it.
std::string_view typeName(const hldb::Any *object) { return hldb::AnyTypeName(object->getAnyType()); }

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

TEST_F(ItemsInstancesTest, G1IsAGate) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::Primitive *const gate = findNamed(module->getPrimitives(), "g1");
  ASSERT_NE(gate, nullptr);
  EXPECT_EQ(typeName(gate), hldb::AnyTypeName(hldb::AnyType::Gate));
}

TEST_F(ItemsInstancesTest, UdpU1IsAnInstanceOfUdpBuf) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::Primitive *const udp = findNamed(module->getPrimitives(), "u1");
  ASSERT_NE(udp, nullptr);
  EXPECT_EQ(typeName(udp), hldb::AnyTypeName(hldb::AnyType::Udp));
  EXPECT_EQ(udp->getDefName(), "udp_buf");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
