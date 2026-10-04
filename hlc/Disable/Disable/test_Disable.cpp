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

// Tests for tests/Disable/dut.sv:
//   module top;
//      initial begin
//         forever begin : loop
//            disable loop;
//            disable fork;
//         end
//      end
//   endmodule // top
//
// IEEE 1800-2023 constructs under test (Sec 9.6.1, "Sequential blocks" /
// disable_statement grammar in Annex A.6.5):
//   disable_statement ::=
//         disable hierarchical_task_identifier ;
//       | disable hierarchical_block_identifier ;
//       | disable fork ;
//
// "disable loop;" names the enclosing named block "loop" (a
// hierarchical_block_identifier) -- HLDB should model this as a
// hldb::Disable whose getExpr() is a RefObj named "loop".
// "disable fork;" is the dedicated disable-fork form (Sec 9.6.2) --
// HLDB should model this as a hldb::DisableFork, which (per
// disable_fork.h) carries no expr field at all, since "fork" is a
// keyword, not a named reference.
//
// Checked:
//   - module "top" exists with exactly one process (an Initial).
//   - Initial's stmt is a Begin (from "initial begin ... end").
//   - that Begin wraps exactly one statement: a ForeverStmt
//     ("forever begin : loop ... end").
//   - ForeverStmt's stmt is a Begin named "loop" (the named block
//     created by "begin : loop").
//   - the named "loop" Begin wraps exactly two statements:
//     1) a Disable whose getExpr() is a RefObj named "loop"
//     2) a DisableFork
//   - compiler reports zero errors (both forms are legal SV).
//
// NOT CHECKED (out of scope):
//   - RefObj::getActual() binding of "loop" back to the Begin object --
//     if HLC currently leaves this unbound, that is a separate binding
//     concern from what this file otherwise verifies (the statement shape
//     itself), so it is exercised in its own assertion below rather than
//     folded silently into the existence checks.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/disable.h>
#include <hldb/disable_fork.h>
#include <hldb/forever_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>

namespace hlc {

class DisableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Disable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  // Walks top -> Initial -> Begin -> ForeverStmt -> named Begin "loop",
  // returning the named Begin, or nullptr if the shape does not match.
  static const hldb::Begin *getLoopBlock() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->size() != 1u) return nullptr;

    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
    if (init == nullptr) return nullptr;

    const hldb::Begin *const outer = any_cast<hldb::Begin>(init->getStmt());
    if (outer == nullptr || outer->getStmts() == nullptr || outer->getStmts()->size() != 1u) return nullptr;

    const hldb::ForeverStmt *const forever = any_cast<hldb::ForeverStmt>(outer->getStmts()->at(0));
    if (forever == nullptr) return nullptr;

    const hldb::Begin *const loop = any_cast<hldb::Begin>(forever->getStmt());
    if (loop == nullptr || loop->getName() != "loop") return nullptr;
    return loop;
  }
};

// --- module / process shape ----

TEST_F(DisableTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr) << "module 'top' not found"; }

TEST_F(DisableTest, TopHasExactlyOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->front()), nullptr) << "the single process must be an Initial";
}

TEST_F(DisableTest, ForeverWrapsNamedBlockLoop) {
  const hldb::Begin *const loop = getLoopBlock();
  ASSERT_NE(loop, nullptr) << "initial->begin->forever->begin:loop shape not found";
  EXPECT_EQ(loop->getName(), "loop");
}

// --- disable statement shape ----

TEST_F(DisableTest, LoopBlockHasExactlyTwoDisableStatements) {
  const hldb::Begin *const loop = getLoopBlock();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getStmts(), nullptr);
  ASSERT_EQ(loop->getStmts()->size(), 2u) << "'disable loop;' and 'disable fork;' are the only two statements";
}

TEST_F(DisableTest, DisableLoopHasRefObjExprNamedLoop) {
  const hldb::Begin *const loop = getLoopBlock();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getStmts(), nullptr);
  ASSERT_GE(loop->getStmts()->size(), 1u);

  const hldb::Disable *const dis = any_cast<hldb::Disable>(loop->getStmts()->at(0));
  ASSERT_NE(dis, nullptr) << "'disable loop;' must produce a hldb::Disable (IEEE 1800-2023 Annex A.6.5)";

  ASSERT_NE(dis->getExpr(), nullptr) << "Disable::getExpr() must hold the disabled block's identifier";
  const hldb::RefObj *const ref = dis->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr) << "'disable loop;' expr must be a RefObj naming the block";
  EXPECT_EQ(ref->getName(), "loop");
}

TEST_F(DisableTest, DisableForkIsSecondStatement) {
  const hldb::Begin *const loop = getLoopBlock();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getStmts(), nullptr);
  ASSERT_GE(loop->getStmts()->size(), 2u);

  const hldb::DisableFork *const df = any_cast<hldb::DisableFork>(loop->getStmts()->at(1));
  ASSERT_NE(df, nullptr) << "'disable fork;' must produce a hldb::DisableFork (IEEE 1800-2023 Sec 9.6.2)";
}

TEST_F(DisableTest, DisableLoopIsNotDisableFork) {
  // Sec 9.6.1 vs 9.6.2: the two forms are distinct statement kinds -- a
  // named-block disable must never be modeled as a DisableFork and
  // vice-versa.
  const hldb::Begin *const loop = getLoopBlock();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getStmts(), nullptr);
  ASSERT_EQ(loop->getStmts()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::DisableFork>(loop->getStmts()->at(0)), nullptr);
  EXPECT_EQ(any_cast<hldb::Disable>(loop->getStmts()->at(1)), nullptr);
}

// --- compiler diagnostics ----

TEST_F(DisableTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
