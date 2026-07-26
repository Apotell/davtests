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
//   - module has exactly 1 net: 'v' (int, no initial value)
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

// ---------------------------------------------------------------------------
// Net declaration — int v
// ---------------------------------------------------------------------------
TEST_F(VariableMultipleAssignments, NetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";

  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr) << "net 'v' not found in module";
}

// ---------------------------------------------------------------------------
// Two continuous assignments — assign v = 12; assign v = 13;
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Structural completeness
// ---------------------------------------------------------------------------
TEST_F(VariableMultipleAssignments, OneNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "expected exactly 1 net: 'v'";
}

TEST_F(VariableMultipleAssignments, NetVHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
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

// ---------------------------------------------------------------------------
// Compiler diagnostics -- the multiple continuous assignments are not flagged
// ---------------------------------------------------------------------------
TEST_F(VariableMultipleAssignments, Compiler_NoErrorsReported) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "HLC does not reject two continuous assignments driving the same net 'v'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
