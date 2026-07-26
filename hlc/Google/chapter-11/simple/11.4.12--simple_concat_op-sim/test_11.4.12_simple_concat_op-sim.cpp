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

// Tests for 11.4.12--simple_concat_op-sim.sv (tags: 11.4.12)
//   module top(input [1:0] a, input [1:0] b, output [3:0] c);
//     assign c = {a, b};
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 nets: "a" [1:0] (input),
//     "b" [1:0] (input), "c" [3:0] (output), all vpiNetType wire, each
//     RefTypespec -> LogicTypespec with its own Range
//   - module has exactly 1 continuous assignment: lhs RefObj "c", rhs
//     Operation (vpiOpType=concatenation) with 2 operands: RefObj "a",
//     RefObj "b"
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

class SimpleConcatOpSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.12--simple_concat_op-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ---------------------------------------------------------

TEST_F(SimpleConcatOpSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SimpleConcatOpSimTest, ModuleHasThreeNetsWithExpectedRanges) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 3u);
  struct Expected {
    const char *name;
    const char *left;
    const char *right;
  };
  const Expected expected[3] = {{"a", "1", "0"}, {"b", "1", "0"}, {"c", "3", "0"}};
  for (const Expected &exp : expected) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(exp.name, top->getNets());
    ASSERT_NE(net, nullptr) << "net " << exp.name;
    EXPECT_EQ(net->getNetType(), vpiWire);
    const hldb::LogicTypespec *const lt = net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "net " << exp.name;
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), exp.left) << "net " << exp.name;
    EXPECT_EQ(lt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), exp.right) << "net " << exp.name;
  }
}

// --- continuous assignment: concatenation operator --------------------------

TEST_F(SimpleConcatOpSimTest, ContAssignIsConcatenationOfAAndB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getLhs<hldb::RefObj>()->getName(), "c");
  const hldb::Operation *const op = ca->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConcatOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(1))->getName(), "b");
}

// --- design-level typespecs / compiler diagnostics -----------------------

TEST_F(SimpleConcatOpSimTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(SimpleConcatOpSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(SimpleConcatOpSimTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
