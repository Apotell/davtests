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

// Tests for wildcard.sv (tags: 7.8.1)
//   module top ();
//     int arr[*];
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: 'arr' (assoc ArrayTypespec, idx=wildcard, elem=IntTypespec)
//   - wildcard index [*]: IndexTypespec RefTypespec resolves to no concrete type (nullptr)
//   - variable has no initial value (plain declaration, no initializer)
//   - top has no processes
//   - top has no continuous assignments
//
// Also checked:
//   - HLC reports no compile errors for a wildcard-indexed associative array
//     declaration (structural proxy for "this construct is legal"; full
//     runtime behavior requires simulation, out of scope for this frontend)
//   - the wildcard index RefTypespec's untemplated getActual() (Typespec*)
//     is null, positively confirming no actual type was resolved at all

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/array_typespec.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class WildcardTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "wildcard.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(WildcardTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- variable "arr" : int[*] ----

TEST_F(WildcardTest, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(WildcardTest, VariableNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "arr");
}

TEST_F(WildcardTest, VariableHasAssociativeArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(WildcardTest, AssocArrayKeyIsWildcard) {
  // int arr[*] -- the wildcard index [*] is stored as a RefTypespec with no
  // resolved actual type (getActual() returns nullptr for any concrete type).
  // This distinguishes it from string.sv where getActual<StringTypespec>() != nullptr.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  // No concrete type resolved for the wildcard -- getActual<IntTypespec> is null
  EXPECT_EQ(at->getIndexTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(WildcardTest, AssocArrayValueTypeIsInt) {
  // element type is IntTypespec (from `int arr[*]`)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(WildcardTest, VariableHasNoInitialValue) {
  // int arr[*] -- plain declaration with no initializer
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getValue<hldb::Any>(), nullptr);
}

TEST_F(WildcardTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(WildcardTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(WildcardTest, IndexTypespecActualIsNull) {
  // Positive confirmation that the wildcard index resolves to no actual type
  // at all: RefTypespec::getActual() (untemplated) returns the raw Typespec*
  // and must be null, independent of what concrete type was probed for.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_EQ(at->getIndexTypespec()->getActual(), nullptr);
}

TEST_F(WildcardTest, CompilerHasNoErrors) {
  // int arr[*] is legal SystemVerilog; HLC must accept it without diagnostics.
  const hlc::ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "wildcard-indexed associative array declaration must not produce compile errors";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
