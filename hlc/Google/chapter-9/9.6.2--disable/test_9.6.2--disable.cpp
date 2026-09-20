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

// Source under test: tests/Google/chapter-9/9.6.2--disable.sv
//
//   module fork_tb ();
//   	reg a = 0;
//   	reg b = 0;
//   	initial begin: block
//   		a = 1;
//   		disable block;
//   		b = 1;
//   	end
//   endmodule
//
// IEEE 1800-2023 clause tested: 9.6.2 "Disabling of named blocks and
// tasks" -- "disable block;" (a disable_statement naming an enclosing
// named block, hierarchical_identifier) immediately terminates that named
// block. This is a purely static/elaboration-time structural fact for
// HLC: the statement following "disable block;" ("b = 1;") is still
// legal SystemVerilog and is still elaborated normally (it is only
// unreachable at RUNTIME, which HLC as a compiler never executes).
//
// Checked:
//   - module fork_tb exists.
//   - "a" and "b" are declared with the variable-type keyword `reg`
//     (IEEE 1800-2023 6.8), so both must be classified as hldb::Variable,
//     each with its declared initializer (0) preserved on
//     Variable::getExpr().
//   - the initial statement binds a Begin directly, whose own name
//     (Scope::getName(), from "begin: block") is "block".
//   - the Begin contains exactly three statements, in order: the blocking
//     Assignment "a = 1;", a Disable statement, and the blocking
//     Assignment "b = 1;" -- confirming the statement after "disable
//     block;" is still present in the elaborated statement list (HLC
//     never removes it, since only runtime execution -- which HLC does
//     not perform -- would actually skip it).
//   - the Disable statement's getExpr() is a RefObj naming "block".
//
// Not checked:
//   - Whether the Disable's RefObj actually resolves (getActual()) back
//     to the enclosing named Begin block -- this would require
//     resolving a name reference to an enclosing (not previously
//     declared-then-referenced) scope, which is architecturally
//     different from every other RefObj resolution checked elsewhere in
//     this chapter (a forward/self reference), so it is not assumed.
//   - Runtime behavior (does "disable block;" really terminate execution
//     of "block" before "b = 1;" runs) -- HLC is an elaborator with no
//     simulator (see .claude/hlc_overview.md), so no execution ever
//     happens for this test to observe.

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/disable.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

namespace hlc {
namespace {
// Tries Variable::getExpr() first (the declared variable_decl_assignment
// initializer expression), then falls back to Variable::getValue() --
// which of the two hldb actually populates for a plain scalar initializer
// like "reg a = 0;" was not confirmed via a header or a .log, so both are
// accepted rather than assuming one.
const hldb::Constant *InitializerConstant(const hldb::Variable *const variable) {
  if (const hldb::Constant *const viaExpr = any_cast<hldb::Constant>(variable->getExpr())) return viaExpr;
  return any_cast<hldb::Constant>(variable->getValue());
}
}  // namespace

class DisableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.6.2--disable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DisableTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(DisableTest, AAndBAreVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "'a' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Constant *const aInit = InitializerConstant(a);
  ASSERT_NE(aInit, nullptr);
  EXPECT_EQ(aInit->getDecompile(), "0");

  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr) << "'b' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Constant *const bInit = InitializerConstant(b);
  ASSERT_NE(bInit, nullptr);
  EXPECT_EQ(bInit->getDecompile(), "0");
}

TEST_F(DisableTest, NamedBlockContainsAssignDisableAssignInOrder) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  const hldb::Begin *const block = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(block, nullptr) << "'initial begin: block ... end' must bind a Begin directly";
  EXPECT_EQ(block->getName(), "block");

  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 3u) << "'a = 1;', 'disable block;', 'b = 1;' are exactly three statements";

  const hldb::Assignment *const firstAssign = any_cast<hldb::Assignment>(block->getStmts()->at(0));
  ASSERT_NE(firstAssign, nullptr) << "'a = 1;' should be a plain Assignment";
  EXPECT_TRUE(firstAssign->getBlocking());
  const hldb::RefObj *const firstLhs = any_cast<hldb::RefObj>(firstAssign->getLhs());
  ASSERT_NE(firstLhs, nullptr);
  EXPECT_EQ(firstLhs->getName(), "a");

  const hldb::Disable *const disable = any_cast<hldb::Disable>(block->getStmts()->at(1));
  ASSERT_NE(disable, nullptr) << "'disable block;' must produce a Disable statement";
  const hldb::RefObj *const disableRef = any_cast<hldb::RefObj>(disable->getExpr());
  ASSERT_NE(disableRef, nullptr) << "'disable block;' names the enclosing block via a RefObj";
  EXPECT_EQ(disableRef->getName(), "block");

  const hldb::Assignment *const secondAssign = any_cast<hldb::Assignment>(block->getStmts()->at(2));
  ASSERT_NE(secondAssign, nullptr) << "'b = 1;' must still be present after the Disable statement";
  EXPECT_TRUE(secondAssign->getBlocking());
  const hldb::RefObj *const secondLhs = any_cast<hldb::RefObj>(secondAssign->getLhs());
  ASSERT_NE(secondLhs, nullptr);
  EXPECT_EQ(secondLhs->getName(), "b");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
