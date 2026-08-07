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
//   - design has module top
//   - module has 1 ClassDefn: getName()="C"
//   - ClassDefn has 1 Variable "x" (IntTypespec)
//   - module has 1 variable 'arr' ? ArrayTypespec (static=1 ? error recovery, NOT associative=3)
//   - ArrayTypespec elem type is IntTypespec
//   - top has no processes
//   - top has no continuous assignments
//
// Also checked:
//   - HLC emits EL0535 (ELAB_ILLEGAL_IMPLICIT_NET) -- class C is unresolved as
//     an associative-array index type
//   - index typespec is absent (null) in the error-recovery ArrayTypespec

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class Class : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(Class, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- class C definition ----

TEST_F(Class, ModuleHasOneClassDefn) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

TEST_F(Class, ClassDefnNameIsC) {
  // getName() returns the simple identifier stored by HLC.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getName(), "C");
}

TEST_F(Class, ClassDefnHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
}

TEST_F(Class, ClassVariableNameIsX) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->at(0)->getName(), "x");
}

TEST_F(Class, ClassVariableXHasIntTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const var = cls->getVariables()->at(0);
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const rt = var->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::IntTypespec>(), nullptr);
}

// --- variable arr (error-recovery: static array, not associative) ----

TEST_F(Class, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(Class, VariableNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "arr");
}

TEST_F(Class, VariableHasArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ArrayTypespec>(), nullptr);
}

TEST_F(Class, ArrayTypespecIsStaticDueToErrorRecovery) {
  // int arr[C] ? HLC could not resolve C as an index type (EL0535),
  // so the ArrayTypespec falls back to static(1) instead of associative(3)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1 (error recovery)
}

TEST_F(Class, ArrayTypespecElemTypeIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Class, ArrayTypespecIndexTypespecIsNull) {
  // int arr[C] -- C could not be resolved as an index type, so the
  // error-recovery ArrayTypespec has no index typespec at all.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getIndexTypespec(), nullptr);
}

TEST_F(Class, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(Class, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
