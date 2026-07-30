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

// Tests for 11.5.1--idx_select.sv (tags: 11.5.1)
//   logic [15:0] a;
//   logic b;
//
//   initial begin
//     b = a[11];
//   end
//
// Per IEEE 1800-2017 11.5.1, "b = a[11]" is a *procedural* bit-select: unlike
// the "simple_idx_select" continuous-assignment sibling of this file, the
// select lives inside an initial block as a blocking Assignment, so the
// corners here are about the Initial/Begin process shape, not a ContAssign.
//
// Checked:
//   - module top has exactly 2 nets: "a" (LogicTypespec, vector [15:0], no
//     decl-assignment) and "b" (LogicTypespec, scalar -- no packed
//     dimension in the source, so no Range at all)
//   - module has zero continuous assignments (the select is procedural)
//   - module has exactly 1 process: an Initial whose body is a Begin with
//     exactly 1 statement: a blocking Assignment
//   - the Assignment: lhs RefObj "b" resolving to Net "b"; rhs BitSelect
//     "a[11]" whose vpiPrefix RefObj "a" resolves to Net "a" and whose
//     vpiIndex is Constant "11" (decompile "11", value 11)
//   - both "a" and "b" have no getValue<Constant>() (neither is
//     decl-assigned; "b"'s value only ever comes from the statement below)
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//
// Not checked:
//   - this file carries no $display, so there is no runtime value to check
//     even in principle (see the "-sim" sibling for that).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IdxSelectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.5.1--idx_select.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----------------------------------------------------------

TEST_F(IdxSelectTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(IdxSelectTest, ModuleHasVectorNetAAndScalarNetBNeitherDeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 2u);

  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  const hldb::LogicTypespec *const aType = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(aType, nullptr);
  ASSERT_NE(aType->getRanges(), nullptr);
  EXPECT_EQ(aType->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "15");
  EXPECT_EQ(aType->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");

  const hldb::LogicTypespec *const bType = b->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(bType, nullptr);
  EXPECT_EQ(bType->getRanges(), nullptr) << "'b' is declared as a bare scalar 'logic b;'";

  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr) << "'a' has no decl-assignment";
  EXPECT_EQ(b->getValue<hldb::Constant>(), nullptr) << "'b' is only ever assigned inside the initial block";
}

TEST_F(IdxSelectTest, ModuleHasNoContinuousAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr) << "the select is procedural (inside initial), not a continuous assign";
}

// --- initial block: procedural bit-select -----------------------------------

TEST_F(IdxSelectTest, InitialBlockHasOneBlockingAssignment) {
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

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "b");
  EXPECT_NE(assign->getLhs<hldb::RefObj>()->getActual<hldb::Net>(), nullptr);
}

TEST_F(IdxSelectTest, AssignmentRhsIsBitSelectAOfElevenIndexingNetA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);

  const hldb::BitSelect *const sel = assign->getRhs<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getName(), "a[11]");
  EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "a");
  EXPECT_NE(sel->getPrefix<hldb::RefObj>()->getActual<hldb::Net>(), nullptr);
  ASSERT_NE(sel->getIndex<hldb::Constant>(), nullptr);
  EXPECT_EQ(sel->getIndex<hldb::Constant>()->getDecompile(), "11");
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(IdxSelectTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(IdxSelectTest, CompilerReportsZeroErrors) {
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
