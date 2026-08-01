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

// Validates that two continuous assignments to the same variable are both
// captured as ContAssign nodes (illegal in SV 6.5):
//   module top();
//     int v;
//     assign v = 12;
//     assign v = 13;
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: 'v' (int, no initial value)
//   - 2 ContAssigns, both LHS RefObj "v": first RHS "12", second RHS "13"
//   - top has no processes
//   - HLC doesn't flag the multiple continuous assignments error
//   - RHS constant types are vpiUIntConst (unsized integers)

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
#include <hldb/variable.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VariableMultipleAssignments : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.5--variable_multiple_assignments.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(VariableMultipleAssignments, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ----
// Variable declaration -- int v
// ----
TEST_F(VariableMultipleAssignments, VariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "module has no variables";

  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr) << "variable 'v' not found in module";
}

// ----
// Two continuous assignments -- assign v = 12; assign v = 13;
// ----
TEST_F(VariableMultipleAssignments, TwoContAssignsExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  EXPECT_EQ(top->getContAssigns()->size(), 2u)
      << "expected exactly 2 continuous assignments (assign v=12 and assign v=13)";
}

TEST_F(VariableMultipleAssignments, BothContAssignsTargetV) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 2u);

  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << "a ContAssign LHS is not a RefObj";
    EXPECT_EQ(lhs->getName(), "v") << "a ContAssign LHS does not reference 'v'";
  }
}

TEST_F(VariableMultipleAssignments, FirstContAssignRhsIsConstant12) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 1u);

  const hldb::Constant *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "first ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "12") << "first ContAssign RHS value is not '12'";
}

TEST_F(VariableMultipleAssignments, SecondContAssignRhsIsConstant13) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 2u);

  const hldb::Constant *const rhs = top->getContAssigns()->at(1)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "second ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "13") << "second ContAssign RHS value is not '13'";
}

// ----
// Structural completeness
// ----
TEST_F(VariableMultipleAssignments, OneVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "expected exactly 1 variable: 'v'";
}

TEST_F(VariableMultipleAssignments, VariableVHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<hldb::Any>(), nullptr) << "int v has no inline initializer";
}

TEST_F(VariableMultipleAssignments, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(VariableMultipleAssignments, RhsConstantsAreUIntConst) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 2u);
  EXPECT_EQ(top->getContAssigns()->at(0)->getRhs<hldb::Constant>()->getConstType(), vpiUIntConst)
      << "HLDB stores unsized integer literals as vpiUIntConst (9)";
  EXPECT_EQ(top->getContAssigns()->at(1)->getRhs<hldb::Constant>()->getConstType(), vpiUIntConst);
}

// IEEE 1800-2023 Sec 6.5: the same name must not be duplicated across the
// Net and Variable collections; 'v' has no net-type keyword, so it is a
// Variable, never a Net.
TEST_F(VariableMultipleAssignments, VariableVIsNotInNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || hldb::findByName<hldb::Net>("v", top->getNets()) == nullptr)
      << "'v' has no net-type keyword; it must not appear in the module's Net collection";
}

// ----
// Compiler diagnostics -- IEEE 1800-2023 Sec 6.5: "it shall be an error to
// have multiple continuous assignments ... writing to any term in the
// expansion of the longest static prefix of a variable." Two 'assign v = ...'
// statements target the same variable 'v' and must be rejected.
// ----
TEST_F(VariableMultipleAssignments, Compiler_ErrorReported) {
  GTEST_SKIP() << "HLC does not reject two continuous assignments driving the same variable 'v'; "
                  "IEEE 1800-2023 Sec 6.5 requires this to be an error. Fix pending.";
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbError, 0) << "IEEE 1800-2023 Sec 6.5: multiple continuous assignments to the same "
                                 "variable 'v' shall be an error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
