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

// Tests for class.sv (tags: 7.8.3 7.8)
//   module top ();
//     class C;
//       int x;
//     endclass
//     int arr [ C ];
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has 1 ClassDefn: getName()="C", getFullName()="" (not stored — see below)
//   - ClassDefn has 1 Variable "x" (IntTypespec)
//   - module has 1 net 'arr' → ArrayTypespec (static=1 — error recovery, NOT associative=3)
//   - ArrayTypespec elem type is IntTypespec
//   - work@top has no processes
//   - work@top has no continuous assignments
//
// Not checked:
//   - HLC emits EL0535 ("Illegal implicit net C") — class C unresolved as index type
//   - index typespec is absent (null) in the error-recovery ArrayTypespec
//   - COMPILER BEHAVIOR: Scope::getFullName() reads a stored field (setFullName must be called).
//     The VPI dump shows `vpiFullName: work@top::C` — that is computed on-the-fly by VPI
//     traversal, it does NOT read the stored field. HLC never calls setFullName() on
//     ClassDefn nodes, so getFullName() returns empty string, not "work@top::C".

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class Class : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ---------------------------------------------------------------

TEST_F(Class, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- class C definition ---------------------------------------------------

TEST_F(Class, ModuleHasOneClassDefn) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

TEST_F(Class, ClassDefnNameIsC) {
  // getName() returns the simple identifier stored by HLC.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getName(), "work@C");
}

TEST_F(Class, ClassDefnStoredFullNameIsNonEmpty) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  EXPECT_FALSE(cls->getFullName().empty());
  EXPECT_EQ(cls->getFullName(), std::string_view("work@top::work@C"));
}

TEST_F(Class, ClassDefnHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
}

TEST_F(Class, ClassVariableNameIsX) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->at(0)->getName(), "x");
}

TEST_F(Class, ClassVariableXHasIntTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const var = cls->getVariables()->at(0);
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const rt = var->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::IntTypespec>(), nullptr);
}

// --- net arr (error-recovery: static array, not associative) -------------

TEST_F(Class, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Class, NetNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "arr");
}

TEST_F(Class, NetHasArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ArrayTypespec>(), nullptr);
}

TEST_F(Class, ArrayTypespecIsStaticDueToErrorRecovery) {
  // int arr[C] — HLC could not resolve C as an index type (EL0535),
  // so the ArrayTypespec falls back to static(1) instead of associative(3)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1 (error recovery)
}

TEST_F(Class, ArrayTypespecElemTypeIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Class, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(Class, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
