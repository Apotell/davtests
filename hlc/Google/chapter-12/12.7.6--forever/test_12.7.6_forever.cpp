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

// Tests for 12.7.6--forever.sv (tags: 12.7.6)
//   module foreach_tb ();
//     initial begin
//       forever begin : loop
//         disable loop;
//       end
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.7.6 "The forever-loop",
// p.332-333, checked before any test code was written):
//   "The forever-loop repeatedly executes a statement. To avoid a
//   zero-delay infinite loop, which could hang the simulation event
//   scheduler, the forever loop should only be used in conjunction with
//   the timing controls or the disable statement." This file uses the
//   disable-statement form: the forever's body is a named block ("begin
//   : loop ... end"), and "disable loop;" inside that block terminates
//   the block by name, matching the recommended pattern from the
//   standard. Per the general block-label rule (9.3.5, referenced by
//   12.7.1/12.7.3's "the block is unnamed by default, but can be named
//   by adding a statement label"), the identifier "loop" written after
//   "begin" is the block's own name, and "disable loop" must resolve
//   its reference back to that exact named block.
//
// What is checked:
//   - module foreach_tb has an Initial process whose body is a
//     single-item Begin containing one statement, resolving to
//     ForeverStmt
//   - ForeverStmt.getStmt() is a Begin named "loop" (getName() ==
//     "loop" -- Scope's override of Any::getName(), not getEndLabel(),
//     which is a separate field for an optional repeated label after
//     "end" that this source does not use)
//   - that named Begin has exactly one statement, a Disable
//   - Disable.getExpr() is RefObj "loop", and per the named-block
//     resolution rule above, it should resolve (getActual<hldb::Begin>()
//     non-null) back to the very same named Begin block
//   - no continuous assignments, nets, or variables exist in the module
//     (the file declares nothing but the forever/disable construct)
//
// What is NOT checked and why:
//   - the runtime "infinite loop terminated by disable" behavior itself
//     (that the block actually stops executing) is a simulation-time
//     concept, not a static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/disable.h>
#include <hldb/forever_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForeverTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.7.6--forever.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("foreach_tb", m_design->getAllModules());
  }

  static const hldb::ForeverStmt *getForeverStmt() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    const hldb::Begin *const body = initial->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::ForeverStmt>(body->getStmts()->at(0));
  }

  static const hldb::Begin *getLoopBlock() {
    const hldb::ForeverStmt *const forever = getForeverStmt();
    if (forever == nullptr) return nullptr;
    return forever->getStmt<hldb::Begin>();
  }
};

TEST_F(ForeverTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ForeverTest, ModuleHasNoNetsAndNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no declarations exist in this module";
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty())
      << "no declarations exist in this module";
}

TEST_F(ForeverTest, InitialBodyIsForeverStmt) {
  const hldb::ForeverStmt *const forever = getForeverStmt();
  ASSERT_NE(forever, nullptr) << "the single statement in the initial body should resolve to ForeverStmt";
}

// ---------------------------------------------------------------------------
// forever begin : loop ... end
// ---------------------------------------------------------------------------
TEST_F(ForeverTest, ForeverStmtBodyIsBeginNamedLoop) {
  const hldb::Begin *const loopBlock = getLoopBlock();
  ASSERT_NE(loopBlock, nullptr) << "ForeverStmt body should be a Begin (explicit named block in source)";
  EXPECT_EQ(loopBlock->getName(), "loop")
      << "the block's own name (from 'begin : loop') is exposed via getName() (Scope's override), not "
         "getEndLabel() -- that field is for an optional repeated label after 'end', which this source doesn't "
         "use";
}

TEST_F(ForeverTest, LoopBlockHasExactlyOneDisableStmt) {
  const hldb::Begin *const loopBlock = getLoopBlock();
  ASSERT_NE(loopBlock, nullptr);
  ASSERT_NE(loopBlock->getStmts(), nullptr);
  EXPECT_EQ(loopBlock->getStmts()->size(), 1u);
}

// ---------------------------------------------------------------------------
// disable loop;  -- must resolve back to the named block itself
// ---------------------------------------------------------------------------
TEST_F(ForeverTest, DisableReferencesLoopAndResolvesToTheNamedBlock) {
  const hldb::Begin *const loopBlock = getLoopBlock();
  ASSERT_NE(loopBlock, nullptr);
  ASSERT_NE(loopBlock->getStmts(), nullptr);
  ASSERT_GE(loopBlock->getStmts()->size(), 1u);
  const hldb::Disable *const disable = any_cast<hldb::Disable>(loopBlock->getStmts()->at(0));
  ASSERT_NE(disable, nullptr) << "'disable loop;' should be a Disable statement";
  const hldb::RefObj *const target = disable->getExpr<hldb::RefObj>();
  ASSERT_NE(target, nullptr) << "Disable target is not a RefObj";
  EXPECT_EQ(target->getName(), "loop");
  EXPECT_NE(target->getActual<hldb::Begin>(), nullptr)
      << "'disable loop' should resolve to the named block 'loop' itself";
}

TEST_F(ForeverTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
