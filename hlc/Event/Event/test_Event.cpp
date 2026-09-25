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

// Tests for Event/dut.sv (the SystemVerilog 'event' data type):
//
//   module t;
//     event e;
//   endmodule
//
// What to check and why (IEEE 1800-2023 Sec 6.8 "Variable declarations"
// and Sec 15.5 "Event variables", checked before any test code was
// written -- no .log file consulted for this file's expected shape, only
// the standard text and the real hldb API headers):
//
//   Sec 15.5.1: "A variable of the event data type ... shall be
//   declared explicitly by using the keyword event." 'event e;' declares
//   exactly one named event variable 'e'. hldb models a named event
//   variable as its own 'NamedEvent' object (not as a Variable), reached
//   through Scope::getNamedEvents() -- see build/include/hldb/scope.h and
//   build/include/hldb/named_event.h.
//
//   Sec 6.8 / Sec 15.5.1: 'event' is a data type like any other variable
//   type, so 'e' should carry a RefTypespec resolving to an EventTypespec
//   (build/include/hldb/event_typespec.h).
//
//   Sec 6.21 "Scope rules": with no 'automatic' keyword and 'e' declared
//   directly in a module (not inside a task/function/block), the default
//   lifetime is static, so NamedEvent::getAutomatic() should be false.
//
//   'e' is a single scalar named event, not an element of an event array
//   (Sec 7.4.1's array-of-events construct, e.g. 'event e[4];', uses a
//   separate NamedEventArray object per scope.h/named_event_array.h and
//   is not present in this file), so getArrayMember() should be false and
//   Scope::getNamedEventArrays() should be empty.
//
//   'event e;' is the only declaration in the module, with no
//   initial/always process, no continuous assignment, no plain variable,
//   and no net -- the module's other collections should be empty.
//
// What is NOT checked and why:
//   - event trigger ('->e;') and wait ('@e;' / 'wait(e.triggered)')
//     constructs are not present in this .sv file, so there is nothing to
//     assert about event_stmt.h / event_control.h here.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/event_typespec.h>
#include <hldb/module.h>
#include <hldb/named_event.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class EventTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Event.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("t", m_design->getAllModules()); }
};

TEST_F(EventTest, ModuleExists) { ASSERT_NE(getTop(), nullptr) << "module 't' not found"; }

// ===========================================================================
// event e;
// ===========================================================================

TEST_F(EventTest, NamedEventCollectionHasExactlyOneEntry) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNamedEvents(), nullptr) << "module 't' must have a named-event collection";
  EXPECT_EQ(top->getNamedEvents()->size(), 1u) << "'event e;' declares exactly one named event";
}

TEST_F(EventTest, E_ExistsWithEventTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNamedEvents(), nullptr);
  const hldb::NamedEvent *const e = hldb::findByName<hldb::NamedEvent>("e", top->getNamedEvents());
  ASSERT_NE(e, nullptr) << "named event 'e' not found";

  const hldb::RefTypespec *const rts = e->getTypespec();
  ASSERT_NE(rts, nullptr) << "'event e' should carry a typespec";
  EXPECT_NE(rts->getActual<hldb::EventTypespec>(), nullptr) << "'event e' should resolve to an EventTypespec";
}

TEST_F(EventTest, E_IsNotAutomaticAndNotAnArrayMember) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNamedEvents(), nullptr);
  const hldb::NamedEvent *const e = hldb::findByName<hldb::NamedEvent>("e", top->getNamedEvents());
  ASSERT_NE(e, nullptr);

  EXPECT_FALSE(e->getAutomatic())
      << "Sec 6.21: 'e' is declared directly in module 't' with no 'automatic' keyword, so it defaults to "
         "static lifetime";
  EXPECT_FALSE(e->getArrayMember()) << "'event e;' is a scalar named event, not an element of an event array";
}

TEST_F(EventTest, NoNamedEventArrays) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNamedEventArrays() == nullptr || top->getNamedEventArrays()->empty())
      << "no 'event array[N];' declarations appear in this file";
}

// ===========================================================================
// No other declarations
// ===========================================================================

TEST_F(EventTest, NoOtherModuleContent) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty())
      << "'event e;' is the only declaration; no plain variables expected";
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no nets expected";
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty())
      << "no initial/always process expected";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "no continuous assignments expected";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
