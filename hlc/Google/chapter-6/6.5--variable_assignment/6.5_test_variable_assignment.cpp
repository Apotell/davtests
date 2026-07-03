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

// Validates that a continuous assignment to a variable (assign v = 12) is
// captured correctly in the UHDM graph:
//   module top();
//     int v;
//     assign v = 12;
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'v' (IntTypespec, no initial value)
//   - 1 ContAssign: LHS RefObj "v" resolves to the net 'v'
//   - ContAssign RHS is Constant "12" (vpiUIntConst — unsized integer)
//   - work@top has no processes
//
// Not checked:
//   - continuous assignment to a variable (vs wire) is unusual but valid in SV

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/cont_assign.h>
#include <uhdm/design.h>
#include <uhdm/int_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class VariableAssignment : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.5--variable_assignment.hlc"});

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

TEST_F(VariableAssignment, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declaration — int v
// ---------------------------------------------------------------------------
TEST_F(VariableAssignment, NetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";

  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr) << "net 'v' not found in module";
}

TEST_F(VariableAssignment, NetHasTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr) << "net 'v' not found";
  EXPECT_NE(v->getTypespec(), nullptr) << "net 'v' has no typespec (expected IntTypespec)";
}

// ---------------------------------------------------------------------------
// Continuous assignment — assign v = 12
// ---------------------------------------------------------------------------
TEST_F(VariableAssignment, ContAssignExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  ASSERT_FALSE(top->getContAssigns()->empty()) << "continuous assignments list is empty";
}

TEST_F(VariableAssignment, ContAssignLhsReferencesV) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);

  const uhdm::RefObj *const lhs = ca->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "v") << "ContAssign LHS does not reference 'v'";
}

TEST_F(VariableAssignment, ContAssignRhsIsConstant12) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);

  const uhdm::Constant *const rhs = ca->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "12") << "ContAssign RHS constant value is not '12'";
}

// ---------------------------------------------------------------------------
// Additional structural checks
// ---------------------------------------------------------------------------
TEST_F(VariableAssignment, OneNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "expected exactly 1 net: 'v'";
}

TEST_F(VariableAssignment, NetVIsIntType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  EXPECT_NE(v->getTypespec()->getActual<uhdm::IntTypespec>(), nullptr)
      << "int keyword maps to IntTypespec";
}

TEST_F(VariableAssignment, NetVHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<uhdm::Any>(), nullptr)
      << "int v; has no inline initializer — value comes from the assign statement";
}

TEST_F(VariableAssignment, ContAssignLhsResolvesToNetV) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const uhdm::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<uhdm::Net>(), nullptr)
      << "ContAssign LHS RefObj 'v' should resolve to the net 'v'";
}

TEST_F(VariableAssignment, ContAssignRhsConstType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const uhdm::Constant *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst)
      << "Surelog stores unsized integer literals as vpiUIntConst (9)";
}

TEST_F(VariableAssignment, NoProcesses) {
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
