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

// Tests for 6.5--variable_multiple_assignments.sv (tags: 6.5,
// :should_fail_because: two continuous assignments to the same variable
// are illegal)
//   module top();
//     int v;
//     assign v = 12;
//     assign v = 13;
//   endmodule
//
// What to check and why (IEEE 1800-2023 10.3.2 "Continuous assignment",
// p.248-249, checked before any test code was written):
//   "Variables can only be driven by one continuous assignment ..." "int
//   v" is not a net-type keyword (IEEE 1800-2023 6.7 does not list it),
//   so "v" must be a Variable per 6.8, and this file drives that Variable
//   with TWO separate continuous assignments -- exactly the construct
//   the quoted sentence forbids.
//
//   A prior version of this test used hldb::Net/getNets() for "v"
//   throughout, and ended with a test asserting nbError == 0 as
//   "Compiler_NoErrorsReported" -- treating this spec violation as
//   correct, passing behavior. This version fixes "v" to Variable (real
//   bug if it's still a Net) and replaces the old "no errors" test with
//   one that documents the actual bug: the compiler currently reports NO
//   error for a construct the spec says is limited to "one" continuous
//   assignment.
//
// What is checked:
//   - module top exists, has zero Nets and exactly 1 Variable "v" (no
//     declaration-time initializer)
//   - exactly 2 ContAssigns, both LHS RefObj "v" resolving via
//     getActual<hldb::Variable>() (NOT Net); first RHS Constant "12",
//     second RHS Constant "13"; both RHS constants are vpiUIntConst
//   - top has no processes
//   - THE POINT OF THIS FILE: the compiler currently reports zero errors
//     for driving "v" with two continuous assignments, which IEEE
//     1800-2023 10.3.2 limits to "one" -- a real, non-skipped,
//     currently-failing assertion documenting this bug (not yet
//     personally verified by re-running with the check removed, so no
//     GTEST_SKIP() is added per the established gating rule)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VariableMultipleAssignmentsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.5--variable_multiple_assignments.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(VariableMultipleAssignmentsTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Declaration -- int v (Variable, NOT Net)
// ---------------------------------------------------------------------------
TEST_F(VariableMultipleAssignmentsTest, ModuleHasNoNetsAndOneVariableV) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'int v' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'int v' should be represented as a Variable (IEEE 1800-2023 6.8); if this is null, "
         "hldb likely misclassified 'v' as a Net instead";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr) << "Variable 'v' not found";
}

TEST_F(VariableMultipleAssignmentsTest, VariableHasNoInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<hldb::Any>(), nullptr) << "'int v' has no inline initializer";
}

// ---------------------------------------------------------------------------
// Two continuous assignments -- assign v = 12; assign v = 13;
// ---------------------------------------------------------------------------
TEST_F(VariableMultipleAssignmentsTest, TwoContAssignsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  EXPECT_EQ(top->getContAssigns()->size(), 2u)
      << "expected exactly 2 continuous assignments (assign v=12 and assign v=13)";
}

TEST_F(VariableMultipleAssignmentsTest, BothContAssignsTargetVariableVNotNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 2u);

  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << "a ContAssign LHS is not a RefObj";
    EXPECT_EQ(lhs->getName(), "v") << "a ContAssign LHS does not reference 'v'";
    EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr)
        << "'v' must NOT resolve to a Net -- 'int' is a variable-type keyword (IEEE 1800-2023 "
           "6.7 does not list it)";
    EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr)
        << "'v' should resolve to the Variable 'v'";
  }
}

TEST_F(VariableMultipleAssignmentsTest, FirstContAssignRhsIsConstant12) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 1u);
  const hldb::Constant *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "first ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "12") << "first ContAssign RHS value is not '12'";
}

TEST_F(VariableMultipleAssignmentsTest, SecondContAssignRhsIsConstant13) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 2u);
  const hldb::Constant *const rhs = top->getContAssigns()->at(1)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "second ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "13") << "second ContAssign RHS value is not '13'";
}

TEST_F(VariableMultipleAssignmentsTest, RhsConstantsAreUIntConst) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 2u);
  EXPECT_EQ(top->getContAssigns()->at(0)->getRhs<hldb::Constant>()->getConstType(), vpiUIntConst)
      << "HLDB stores unsized integer literals as vpiUIntConst (9)";
  EXPECT_EQ(top->getContAssigns()->at(1)->getRhs<hldb::Constant>()->getConstType(), vpiUIntConst);
}

TEST_F(VariableMultipleAssignmentsTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// The actual point of this file: v is driven by TWO continuous assignments
// ---------------------------------------------------------------------------
TEST_F(VariableMultipleAssignmentsTest, CompilerShouldRejectTwoContinuousAssignmentsToOneVariableButDoesNot) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 10.3.2: 'Variables can only be driven by one continuous assignment.' "
         "'v' is driven by both 'assign v = 12;' and 'assign v = 13;' -- HLC currently reports "
         "zero errors for this, which is a compiler bug";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
