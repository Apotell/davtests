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

// Tests for 12.7.2--repeat.sv (tags: 12.7.2)
//   module repeat_tb ();
//     int a = 128;
//     initial begin
//       repeat(a)
//         $display("repeat");
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.7.2 "The repeat-loop",
// p.331, checked before any test code was written):
//   "loop_statement ::= ... | repeat ( expression ) statement_or_null
//   ...". "The repeat-loop executes a statement a fixed number of
//   times. The loop evaluates its expression once before the loop
//   starts". Here the repeat count expression is simply the RefObj "a"
//   (declared at module scope, outside the loop -- unlike 12.7.1's "int
//   i", which was declared inline in the for_initialization and thus
//   scoped to the loop itself).
//
//   Also (IEEE 1800-2023 6.8): "int" is an integer_atom_type keyword, a
//   non-net data type, so "int a" must be a Variable, never a Net.
//
// What is checked:
//   - module repeat_tb has zero Nets and exactly 1 Variable "a" (int,
//     initial value Constant "128")
//   - the Initial process body is a single-item Begin containing one
//     statement, resolving to Repeat
//   - Repeat's getCondition() is RefObj "a" resolving to the Variable
//     "a" (not a Net) -- confirming the repeat count expression is
//     read, not redeclared, since "a" lives at module scope
//   - Repeat's getStmt() is SysTaskCall "$display" with 1 argument,
//     Constant "\"repeat\""
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the runtime "evaluate once before the loop starts" and "negative/
//     unknown/high-impedance treated as zero" behaviors are simulation-
//     time concepts, not static/structural compile-time properties

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/repeat.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class RepeatTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.7.2--repeat.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("repeat_tb", m_design->getAllModules());
  }

  static const hldb::Repeat *getRepeat() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    const hldb::Begin *const body = initial->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::Repeat>(body->getStmts()->at(0));
  }
};

TEST_F(RepeatTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(RepeatTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
  ASSERT_NE(top->getVariables(), nullptr) << "'int a' should be a Variable, not a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "Variable 'a' not found";
}

TEST_F(RepeatTest, VariableAInitialValueIs128) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "128");
}

TEST_F(RepeatTest, InitialProcessBodyIsRepeat) {
  const hldb::Repeat *const repeat = getRepeat();
  ASSERT_NE(repeat, nullptr) << "the single statement in the initial body should resolve to Repeat";
}

TEST_F(RepeatTest, ConditionResolvesToVariableANotNet) {
  const hldb::Repeat *const repeat = getRepeat();
  ASSERT_NE(repeat, nullptr);
  const hldb::RefObj *const cond = repeat->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "repeat condition is not a RefObj";
  EXPECT_EQ(cond->getName(), "a");
  EXPECT_NE(cond->getActual<hldb::Variable>(), nullptr) << "'a' should resolve to the Variable, not a Net";
}

TEST_F(RepeatTest, LoopBodyIsDisplayOfRepeatLiteral) {
  const hldb::Repeat *const repeat = getRepeat();
  ASSERT_NE(repeat, nullptr);
  const hldb::SysTaskCall *const display = repeat->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr) << "repeat-loop body should be a plain SysTaskCall (single statement, no begin-end)";
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getConstType(), vpiStringConst);
  EXPECT_EQ(arg->getDecompile(), "\"repeat\"");
}

TEST_F(RepeatTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
