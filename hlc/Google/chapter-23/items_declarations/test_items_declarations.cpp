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

// Test for tests/Google/chapter-23/23.2.4--items-declarations.sv
//
// THE SOURCE
//
//   20: module m;
//   21:   wire w;
//   22:   logic v;
//   23:   parameter int P = 1;
//   24:   localparam int L = 2;
//   25:   typedef logic [3:0] nibble_t;
//   26:   genvar g;
//   27:   function automatic int f(int x);
//   28:     return x;
//   29:   endfunction
//   30:   task automatic t;
//   31:   endtask
//   32: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module definition can contain zero or more module items." (23.2.4).
// Syntax 23-5: module_or_generate_item_declaration ::=
// package_or_generate_item_declaration | genvar_declaration | ..., which
// covers net and data declarations, parameter and local parameter
// declarations, type declarations, and task and function declarations. The
// object model of an instance (37.10) reaches them through net, variables,
// parameter, typedef ("the user-defined typespecs that have typedefs
// explicitly declared", detail 1) and task func. HLDB gives them as
// getNets(), getVariables(), getParameters() (with
// Parameter::getLocalParam()), getTypedefs() and getTaskFuncs(), whose
// members are a Function and a Task.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   NetWAndVariableV           net w (Net::getNetType() vpiWire) and
//                              variable v are declared
//   ParametersPAndL            P is a parameter, L a local parameter
//   TypedefNibble              getTypedefs() holds nibble_t
//   GenvarG                    getVariables() holds the genvar g
//   FunctionFAndTaskT          getTaskFuncs() holds the function f and the
//                              task t
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - That g is a genvar and not a variable. HLDB has no genvar object or
//     property and gives g as a Variable, so it can be checked once HLDB
//     provides one.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/parameter.h>
#include <hldb/task_func.h>
#include <hldb/typedef.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
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

class ItemsDeclarationsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "items_declarations.hlc"}); }
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

TEST_F(ItemsDeclarationsTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ItemsDeclarationsTest, NetWAndVariableV) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("w", module->getNets());
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getNetType(), vpiWire);
  EXPECT_NE(hldb::findByName<hldb::Variable>("v", module->getVariables()), nullptr);
}

TEST_F(ItemsDeclarationsTest, ParametersPAndL) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("P", module->getParameters());
  const hldb::Parameter *const l = hldb::findByName<hldb::Parameter>("L", module->getParameters());
  ASSERT_NE(p, nullptr);
  ASSERT_NE(l, nullptr);
  EXPECT_FALSE(p->getLocalParam());
  EXPECT_TRUE(l->getLocalParam());
}

TEST_F(ItemsDeclarationsTest, TypedefNibble) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Typedef>("nibble_t", module->getTypedefs()), nullptr);
}

// "genvar g;" is a module_or_generate_item_declaration (Syntax 23-5); the definition keeps it.
TEST_F(ItemsDeclarationsTest, GenvarG) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("g", module->getVariables()), nullptr) << "the genvar g is missing";
}

TEST_F(ItemsDeclarationsTest, FunctionFAndTaskT) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(sizeOf(module->getTaskFuncs()), 2u);
  const hldb::TaskFunc *const f = hldb::findByName<hldb::TaskFunc>("f", module->getTaskFuncs());
  const hldb::TaskFunc *const t = hldb::findByName<hldb::TaskFunc>("t", module->getTaskFuncs());
  ASSERT_NE(f, nullptr);
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(typeName(f), hldb::AnyTypeName(hldb::AnyType::Function));
  EXPECT_EQ(typeName(t), hldb::AnyTypeName(hldb::AnyType::Task));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
