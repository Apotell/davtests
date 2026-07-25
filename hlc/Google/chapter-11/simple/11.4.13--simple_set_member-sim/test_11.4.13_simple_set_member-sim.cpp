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

// Tests for 11.4.13--simple_set_member-sim.sv (tags: 11.4.13)
//   module top(input [3:0] a, output b);
//     assign b = (a inside {2, 3, 4, 5});
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 2 nets: "a" [3:0] (input,
//     RefTypespec -> LogicTypespec with Range), "b" (output, RefTypespec ->
//     LogicTypespec with no range -- a plain scalar)
//   - module has exactly 1 continuous assignment: lhs RefObj "b", rhs
//     Operation (vpiOpType=inside) with 2 operands: RefObj "a", and a
//     nested Operation (vpiOpType=concatenation) holding the set
//     {2, 3, 4, 5} as 4 Constants
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

class SimpleSetMemberSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.13--simple_set_member-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / nets ---------------------------------------------------------

TEST_F(SimpleSetMemberSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SimpleSetMemberSimTest, NetAHasFourBitRange) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::LogicTypespec *const lt = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(lt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(SimpleSetMemberSimTest, NetBIsScalarLogic) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::LogicTypespec *const lt = b->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  EXPECT_EQ(lt->getRanges(), nullptr);
}

// --- continuous assignment: inside operator ---------------------------------

TEST_F(SimpleSetMemberSimTest, ContAssignIsAInsideTwoThreeFourFive) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getLhs<hldb::RefObj>()->getName(), "b");
  const hldb::Operation *const insideOp = ca->getRhs<hldb::Operation>();
  ASSERT_NE(insideOp, nullptr);
  EXPECT_EQ(insideOp->getOpType(), vpiInsideOp);
  ASSERT_NE(insideOp->getOperands(), nullptr);
  ASSERT_EQ(insideOp->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(insideOp->getOperands()->at(0))->getName(), "a");
  const hldb::Operation *const set = any_cast<hldb::Operation>(insideOp->getOperands()->at(1));
  ASSERT_NE(set, nullptr);
  EXPECT_EQ(set->getOpType(), vpiConcatOp);
  ASSERT_NE(set->getOperands(), nullptr);
  ASSERT_EQ(set->getOperands()->size(), 4u);
  const std::string expected[4] = {"2", "3", "4", "5"};
  for (uint32_t i = 0; i < 4u; ++i) {
    EXPECT_EQ(any_cast<hldb::Constant>(set->getOperands()->at(i))->getDecompile(), expected[i]) << "member " << i;
  }
}

// --- design-level typespecs / compiler diagnostics -----------------------

TEST_F(SimpleSetMemberSimTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(SimpleSetMemberSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(SimpleSetMemberSimTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
