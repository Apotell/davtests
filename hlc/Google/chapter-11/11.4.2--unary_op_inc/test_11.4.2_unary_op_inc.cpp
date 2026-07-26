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

// Tests for 11.4.2--unary_op_inc.sv (tags: 11.4.2)
//   int a = 12;
//   initial begin
//     a++;
//   end
//
// The "++" sibling of 11.4.2--unary_op_dec.sv's "--". Same reasoning:
// used as a bare statement, "a++;" has no assignment target beyond "a"
// itself, so it should decode directly to a post-increment Operation, not
// an Assignment. Having both operators as separate files exercises that
// the parser/AST-builder distinguishes vpiPostIncOp from vpiPostDecOp
// rather than one silently aliasing the other.
//
// Checked:
//   - module work@top has exactly 1 net, "a", int (RefTypespec ->
//     IntTypespec), with a declaration-time getValue<Constant>() of "12"
//   - module getTypespecs() is null/absent, same reasoning as the "--"
//     sibling: "int" carries no separate packed-range typespec
//   - the initial block is a Begin with exactly 1 statement: an Operation
//     with vpiOpType vpiPostIncOp (distinct from vpiPostDecOp) and
//     exactly 1 operand, RefObj "a"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//
// Not checked:
//   - this file has no $display assertion at all (unlike its "-sim"
//     sibling, 11.4.2--unary_op_inc-sim.sv), so there is no runtime
//     numeric outcome authored into the source to check even in
//     principle.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>

namespace hlc {

class UnaryOpIncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.2--unary_op_inc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / net --------------------------------------------------------

TEST_F(UnaryOpIncTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(UnaryOpIncTest, NetAIsIntInitializedToTwelve) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  ASSERT_NE(a->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>()->getDecompile(), "12");
}

TEST_F(UnaryOpIncTest, ModuleHasNoPackedRangeTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getTypespecs(), nullptr);
}

// --- the point of the file: bare "a++;" is a standalone post-inc Operation

TEST_F(UnaryOpIncTest, InitialBlockHasOneStatement) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
}

TEST_F(UnaryOpIncTest, StatementIsPostIncrementOfA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Operation *const postInc = any_cast<hldb::Operation>(blk->getStmts()->at(0));
  ASSERT_NE(postInc, nullptr) << "'a++;' as a bare statement should be an Operation, not an Assignment";
  EXPECT_EQ(postInc->getOpType(), vpiPostIncOp);
  ASSERT_NE(postInc->getOperands(), nullptr);
  ASSERT_EQ(postInc->getOperands()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::RefObj>(postInc->getOperands()->at(0))->getName(), "a");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(UnaryOpIncTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(UnaryOpIncTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
