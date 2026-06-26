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
//   - design has module work@top
//   - module has exactly 1 net: 'v' (int, no initial value)
//   - 2 ContAssigns, both LHS RefObj "v": first RHS "12", second RHS "13"
//   - work@top has no processes
//
// Not checked:
//   - Surelog doesn't flag the multiple continuous assignments error
//   - RHS constant types (vpiUIntConst for unsized integers)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/cont_assign.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>

namespace SURELOG {

class VariableMultipleAssignments : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.5--variable_multiple_assignments.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(VariableMultipleAssignments, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declaration — int v
// ---------------------------------------------------------------------------
TEST_F(VariableMultipleAssignments, NetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";

  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr) << "net 'v' not found in module";
}

// ---------------------------------------------------------------------------
// Two continuous assignments — assign v = 12; assign v = 13;
// ---------------------------------------------------------------------------
TEST_F(VariableMultipleAssignments, TwoContAssignsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  EXPECT_EQ(top->getContAssigns()->size(), 2u)
      << "expected exactly 2 continuous assignments (assign v=12 and assign v=13)";
}

TEST_F(VariableMultipleAssignments, BothContAssignsTargetV) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 2u);

  for (const uhdm::ContAssign *const ca : *top->getContAssigns()) {
    const uhdm::RefObj *const lhs = ca->getLhs<uhdm::RefObj>();
    ASSERT_NE(lhs, nullptr) << "a ContAssign LHS is not a RefObj";
    EXPECT_EQ(lhs->getName(), "v") << "a ContAssign LHS does not reference 'v'";
  }
}

TEST_F(VariableMultipleAssignments, FirstContAssignRhsIsConstant12) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 1u);

  const uhdm::Constant *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr) << "first ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "12") << "first ContAssign RHS value is not '12'";
}

TEST_F(VariableMultipleAssignments, SecondContAssignRhsIsConstant13) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 2u);

  const uhdm::Constant *const rhs =
      top->getContAssigns()->at(1)->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr) << "second ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "13") << "second ContAssign RHS value is not '13'";
}

// ---------------------------------------------------------------------------
// Structural completeness
// ---------------------------------------------------------------------------
TEST_F(VariableMultipleAssignments, OneNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "expected exactly 1 net: 'v'";
}

TEST_F(VariableMultipleAssignments, NetVHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<uhdm::Any>(), nullptr)
      << "int v has no inline initializer";
}

TEST_F(VariableMultipleAssignments, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
