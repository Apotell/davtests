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

// Tests for 11.4.12.1--simple_repl_op-sim.sv (tags: 11.4.12.1)
//   module top(input [1:0] a, output [15:0] b);
//     assign b = {8{a}};
//   endmodule
//
// Checked:
//   - design has module top with exactly 2 nets: "a" [1:0] (input),
//     "b" [15:0] (output), both vpiNetType wire, each RefTypespec ->
//     LogicTypespec with its own Range. Per IEEE 1800-2023 Sec
//     6.7/23.2.2.3: an input port always defaults to a net, and an
//     output port with no explicit data type also defaults to a net, so
//     both being nets here is correct; module has no variables
//     (getVariables() is null)
//   - module has exactly 1 continuous assignment: lhs RefObj "b", rhs
//     Operation (vpiOpType=multi-concatenation) with 2 operands: Constant
//     "8" (the replication count), and a nested Operation
//     (vpiOpType=concatenation) with 1 operand: RefObj "a"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//   - no processes
//
// Not checked:
//   - this file is annotated "(without result verification)" and has no
//     $display assertions, so there is no runtime value to check even in
//     principle.

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
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SimpleReplOpSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.12.1--simple_repl_op-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----

TEST_F(SimpleReplOpSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SimpleReplOpSimTest, ModuleHasTwoNetsWithExpectedRanges) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 2u);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getNetType(), vpiWire);
  const hldb::LogicTypespec *const aType = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(aType, nullptr);
  EXPECT_EQ(aType->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(aType->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getNetType(), vpiWire);
  const hldb::LogicTypespec *const bType = b->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(bType, nullptr);
  EXPECT_EQ(bType->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "15");
  EXPECT_EQ(bType->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(SimpleReplOpSimTest, ModuleHasNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr) << "both ports default to nets per IEEE 1800-2023 "
                                              "Sec 6.7/23.2.2.3, so the module should have no "
                                              "variables";
}

// --- continuous assignment: replication operator ----

TEST_F(SimpleReplOpSimTest, ContAssignIsEightTimesReplicationOfA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getLhs<hldb::RefObj>()->getName(), "b");
  const hldb::Operation *const repl = ca->getRhs<hldb::Operation>();
  ASSERT_NE(repl, nullptr);
  EXPECT_EQ(repl->getOpType(), vpiMultiConcatOp);
  ASSERT_NE(repl->getOperands(), nullptr);
  ASSERT_EQ(repl->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(repl->getOperands()->at(0))->getDecompile(), "8");
  const hldb::Operation *const inner = any_cast<hldb::Operation>(repl->getOperands()->at(1));
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getOpType(), vpiConcatOp);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_EQ(inner->getOperands()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::RefObj>(inner->getOperands()->at(0))->getName(), "a");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(SimpleReplOpSimTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(SimpleReplOpSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(SimpleReplOpSimTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
