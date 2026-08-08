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
//   - module has 1 variable 'arr' -- ArrayTypespec, elem type is IntTypespec
//   - per IEEE 1800-2023 7.8.3 ("Class index"), `int arr[C]` for a
//     previously-declared class C is legal and must produce an associative
//     ArrayTypespec (vpiAssocArray) with a non-null index typespec resolving
//     to class C
//   - top has no processes
//   - top has no continuous assignments
//
// KNOWN COMPILER BUG (verified by a fresh `hlc -f class.hlc` run, not from
// any .log file): HLC emits zero diagnostics (0 FATAL/SYNTAX/ERROR/WARNING/
// NOTE) for `int arr[C]`, but it does not recognize the class-typed index at
// all. Instead it misparses the `[C]` dimension as a numeric packed-range
// bound, producing vpiArrayType: static(1) with a vpiRange whose
// vpiLeftRange is a malformed one-operand "subtract" Operation over a RefObj
// named "C", and no index typespec whatsoever. Per 7.8.3 this should instead
// be an associative array (vpiAssocArray) with a non-null index typespec
// resolving to class C. The tests below assert the spec-required behavior
// (they are therefore expected to fail against the current compiler) rather
// than encode the buggy static-array shape as ground truth; no GTEST_SKIP()
// is added since this defect has not yet been human-verified/annotated.
//
// Also checked:
//   - HLC reports no compile errors for `int arr[C]` (legal per 7.8.3; also
//     true of the current buggy misparse, which is silent)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class ArrayAssociativeClassTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(ArrayAssociativeClassTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- class C definition ----

TEST_F(ArrayAssociativeClassTest, ModuleHasOneClassDefn) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

TEST_F(ArrayAssociativeClassTest, ClassDefnNameIsC) {
  // getName() returns the simple identifier stored by HLC.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getName(), "C");
}

TEST_F(ArrayAssociativeClassTest, ClassDefnHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
}

TEST_F(ArrayAssociativeClassTest, ClassVariableNameIsX) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ClassDefn *const cls = top->getClassDefns()->at(0);
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->at(0)->getName(), "x");
}

TEST_F(ArrayAssociativeClassTest, ClassVariableXHasIntTypespec) {
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

// --- variable arr (spec 7.8.3 requires an associative array with a class-typed index) ----

TEST_F(ArrayAssociativeClassTest, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(ArrayAssociativeClassTest, VariableNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "arr");
}

TEST_F(ArrayAssociativeClassTest, VariableHasArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ArrayTypespec>(), nullptr);
}

TEST_F(ArrayAssociativeClassTest, ArrayTypespecIsAssociativePerSpec783) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiAssocArray);
}

TEST_F(ArrayAssociativeClassTest, ArrayTypespecElemTypeIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(ArrayAssociativeClassTest, ArrayTypespecIndexTypespecResolvesToClassC) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  const hldb::ClassTypespec *const cts = at->getIndexTypespec()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(cts, nullptr);
  EXPECT_EQ(cts->getName(), "C");
}

TEST_F(ArrayAssociativeClassTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(ArrayAssociativeClassTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(ArrayAssociativeClassTest, CompilerHasNoErrors) {
  // int arr[C] is legal SystemVerilog per 7.8.3; HLC must accept it without
  // diagnostics. (Verified true today, though for the wrong reason -- see
  // header comment: the compiler silently misparses the construct instead
  // of rejecting or correctly elaborating it.)
  const hlc::ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "class-indexed associative array declaration must not produce compile errors";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
