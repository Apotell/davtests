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
//   G1IsAGate                  getPrimitives() holds g1, a Gate whose
//                              Primitive::getPrimType() is vpiAndPrim
//   UdpU1IsAnInstanceOfUdpBuf  getPrimitives() holds u1, a Udp whose
//                              getDefName() is udp_buf
//   InstancesAreBound          getRefInstances(): c1 bound to the module
//                              child, i1 to the interface ifc, p1 to the
//                              program prg; elaborated model only: m holds
//                              the instances c1, i1 and p1
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The warning that discourages programs. It is a warning, not an error.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/primitive.h>
#include <hldb/program.h>
#include <hldb/program_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>
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

  // The definition of the module named 'name', or nullptr.
  static const hldb::Module *findModule(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
};

TEST_F(ItemsInstancesTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ItemsInstancesTest, G1IsAGate) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::Primitive *const gate = hldb::findByName<hldb::Primitive>("g1", module->getPrimitives());
  ASSERT_NE(gate, nullptr);
  EXPECT_EQ(typeName(gate), hldb::AnyTypeName(hldb::AnyType::Gate));
  EXPECT_EQ(gate->getPrimType(), vpiAndPrim);
}

TEST_F(ItemsInstancesTest, UdpU1IsAnInstanceOfUdpBuf) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::Primitive *const udp = hldb::findByName<hldb::Primitive>("u1", module->getPrimitives());
  ASSERT_NE(udp, nullptr);
  EXPECT_EQ(typeName(udp), hldb::AnyTypeName(hldb::AnyType::Udp));
  EXPECT_EQ(udp->getDefName(), std::string_view("udp_buf"));
}

// "child c1(); ifc i1(); prg p1();" (Syntax 23-5): each instance is bound to the module, interface or program it
// names. Creating the instances themselves is an instance-model step (23.3), checked in the elaborated model only.
TEST_F(ItemsInstancesTest, InstancesAreBound) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::RefInstance *const c1 = hldb::findByName<hldb::RefInstance>("c1", module->getRefInstances());
  const hldb::RefInstance *const i1 = hldb::findByName<hldb::RefInstance>("i1", module->getRefInstances());
  const hldb::RefInstance *const p1 = hldb::findByName<hldb::RefInstance>("p1", module->getRefInstances());
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(i1, nullptr);
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(c1->getTypespec(), nullptr);
  ASSERT_NE(i1->getTypespec(), nullptr);
  ASSERT_NE(p1->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const child = c1->getTypespec()->getActual<hldb::ModuleTypespec>();
  const hldb::InterfaceTypespec *const ifc = i1->getTypespec()->getActual<hldb::InterfaceTypespec>();
  const hldb::ProgramTypespec *const prg = p1->getTypespec()->getActual<hldb::ProgramTypespec>();
  ASSERT_NE(child, nullptr) << "c1 is not bound to a module";
  ASSERT_NE(ifc, nullptr) << "i1 is not bound to an interface";
  ASSERT_NE(prg, nullptr) << "p1 is not bound to a program";
  EXPECT_EQ(child->getName(), std::string_view("child"));
  EXPECT_EQ(ifc->getName(), std::string_view("ifc"));
  EXPECT_EQ(prg->getName(), std::string_view("prg"));

  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("m", m_design->getTopModules());
    ASSERT_NE(top, nullptr);
    const hldb::Module *const c1Instance = hldb::findByName<hldb::Module>("c1", top->getModules());
    ASSERT_NE(c1Instance, nullptr);
    EXPECT_EQ(c1Instance->getDefName(), std::string_view("child"));
    EXPECT_NE(hldb::findByName<hldb::Interface>("i1", top->getInterfaces()), nullptr);
    EXPECT_NE(hldb::findByName<hldb::Program>("p1", top->getPrograms()), nullptr);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
