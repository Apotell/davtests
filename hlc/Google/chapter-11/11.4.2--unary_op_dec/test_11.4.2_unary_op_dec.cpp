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

// Tests for 11.4.2--unary_op_dec.sv (tags: 11.4.2)
//   int a = 12;
//   initial begin
//     a--;
//   end
//
// IEEE 1800-2017 11.4.2 defines the "++"/"--" increment/decrement
// operators. Used as a bare statement ("a--;", not "b = a--"), the
// operator has no assignment target other than "a" itself -- so the
// corner this file exercises is that the whole statement decodes directly
// to a post-decrement Operation on "a" (not wrapped in an Assignment, and
// not confused with the pre/post-increment opcode it is paired with in
// the sibling 11.4.2--unary_op_inc.sv file).
//
// Checked:
//   - module work@top has exactly 1 net, "a", int (RefTypespec ->
//     IntTypespec), with a declaration-time getValue<Constant>() of "12"
//   - module getTypespecs() is null/absent: "int" carries no separate
//     packed-range typespec the way "reg [N:0]" does elsewhere in this
//     chapter (contrast with 11.4.1--assignment-sim.sv and
//     11.4.5--equality-op.sv, which both have module-level LogicTypespec
//     entries)
//   - the initial block is a Begin with exactly 1 statement: an Operation
//     with vpiOpType vpiPostDecOp and exactly 1 operand, RefObj "a" --
//     confirming "a--;" as a standalone statement is a bare post-decrement
//     Operation, not an Assignment
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//
// Not checked:
//   - this file has no $display assertion at all (unlike its "-sim"
//     sibling, 11.4.2--unary_op_dec-sim.sv), so there is no runtime
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

class UnaryOpDecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.2--unary_op_dec.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / net --------------------------------------------------------

TEST_F(UnaryOpDecTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(UnaryOpDecTest, NetAIsIntInitializedToTwelve) {
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

TEST_F(UnaryOpDecTest, ModuleHasNoPackedRangeTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getTypespecs(), nullptr) << "'int' has no separate module-level packed-range "
                                              "typespec the way 'reg [N:0]' does";
}

// --- the point of the file: bare "a--;" is a standalone post-dec Operation

TEST_F(UnaryOpDecTest, InitialBlockHasOneStatement) {
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

TEST_F(UnaryOpDecTest, StatementIsPostDecrementOfA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Operation *const postDec = any_cast<hldb::Operation>(blk->getStmts()->at(0));
  ASSERT_NE(postDec, nullptr) << "'a--;' as a bare statement should be an Operation, not an Assignment";
  EXPECT_EQ(postDec->getOpType(), vpiPostDecOp);
  ASSERT_NE(postDec->getOperands(), nullptr);
  ASSERT_EQ(postDec->getOperands()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::RefObj>(postDec->getOperands()->at(0))->getName(), "a");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(UnaryOpDecTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(UnaryOpDecTest, CompilerReportsZeroErrors) {
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
