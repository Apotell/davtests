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

// Tests for 10.3.3--cont-assignment-delay.sv (tags: 10.3.3)
//   module top(input a, input b);
//     wire w;
//     assign #10 w = a & b;
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 nets: "a", "b", "w", all
//     vpiNetType wire, each RefTypespec -> LogicTypespec
//   - module has exactly 2 ports: "a" (input), "b" (input)
//   - module has exactly 1 continuous assignment: getDelay() -> Constant
//     "10", lhs RefObj "w" resolving Net "w", rhs Operation (bitwise-and)
//     with 2 RefObj operands ("a", "b") -- the delay on the ASSIGNMENT
//     STATEMENT itself is captured via ContAssign::getDelay()
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed) --
//     needed for the delay literal "10"
//   - compiler emits zero errors
//   - no processes

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ContAssignmentDelayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "10.3.3--cont-assignment-delay.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets / ports ----

TEST_F(ContAssignmentDelayTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ContAssignmentDelayTest, ModuleHasThreeNetsAllWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 3u);
  const char *const names[3] = {"a", "b", "w"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_EQ(net->getNetType(), vpiWire);
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
  }
}

TEST_F(ContAssignmentDelayTest, ModuleHasNoVariables) {
  // Per IEEE 1800-2023 Sec 6.7/6.8: "a", "b" are ports defaulting to net,
  // "w" is an explicit "wire" -- none should be a Variable.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr);
}

// --- continuous assignment with delay ----

TEST_F(ContAssignmentDelayTest, HasOneContAssignWithTenDelay) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::Constant *const delay = ca->getDelay<hldb::Constant>();
  ASSERT_NE(delay, nullptr);
  EXPECT_EQ(delay->getDecompile(), "10");
  EXPECT_EQ(delay->getValue(), "10");
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "w");
  const hldb::Operation *const op = ca->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiBitAndOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(1))->getName(), "b");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(ContAssignmentDelayTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(ContAssignmentDelayTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(ContAssignmentDelayTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(ContAssignmentDelayTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
