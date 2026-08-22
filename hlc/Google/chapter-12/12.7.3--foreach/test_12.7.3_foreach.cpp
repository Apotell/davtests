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

// Tests for 12.7.3--foreach.sv (tags: 12.7.3)
//   module foreach_tb ();
//     string test [4] = '{"111", "222", "333", "444"};
//     initial begin
//       foreach(test[i])
//         $display(i, test[i]);
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.7.3 "The foreach-loop",
// p.331-332, checked before any test code was written):
//   "foreach ( ps_or_hierarchical_array_identifier [ loop_variables ] )
//   statement". "As in a for-loop (12.7.1), a foreach-loop creates an
//   implicit begin-end block around the loop statement, containing
//   declarations of the loop variables with automatic lifetime. This
//   block creates a new hierarchical scope, making the variables local
//   to the loop scope." So "i" in "foreach(test[i])" must be declared
//   in the ForeachStmt's own scope, not the enclosing "initial begin"
//   scope -- matching the ForStmt precedent already confirmed in
//   12.7.1--for.cpp.
//
//   Also (IEEE 1800-2023 6.8): "string" is a data type, not a net_type
//   keyword, so "test" must be a Variable, never a Net.
//
// What is checked:
//   - module foreach_tb has zero Nets and exactly 1 Variable "test"
//     whose typespec resolves to ArrayTypespec (getArrayType() ==
//     vpiStaticArray)
//   - the initial process's single statement resolves to ForeachStmt
//   - ForeachStmt's own scope has exactly 1 Variable "i" -- confirming
//     the implicit local scope described above
//   - ForeachStmt.getVariable() is RefObj "test" resolving to the
//     Variable "test" (the array being iterated)
//   - ForeachStmt.getLoopVars() has exactly 1 item, the same Variable
//     "i"
//   - ForeachStmt.getStmt() is a plain SysTaskCall "$display" (no
//     begin-end wrapper, since the source has none), with 2 arguments:
//     RefObj "i" (resolving to the loop Variable) and BitSelect
//     "test[i]" whose prefix resolves to Variable "test" and whose
//     index resolves to Variable "i"
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the exact contents of the array's '{"111","222","333","444"}
//     initializer are chapter-7/11 territory (assignment patterns);
//     only existence and typespec of "test" are checked here, since
//     this file's topic is the foreach-loop mechanism, not array
//     literals
//   - the runtime iteration behavior itself (that i actually takes
//     values 0..3) is a simulation-time concept, not a static/
//     structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/design.h>
#include <hldb/foreach_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForeachTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.7.3--foreach.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("foreach_tb", m_design->getAllModules());
  }

  static const hldb::ForeachStmt *getForeachStmt() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    const hldb::Begin *const body = initial->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::ForeachStmt>(body->getStmts()->at(0));
  }
};

TEST_F(ForeachTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ForeachTest, ModuleHasNoNetsAndOneVariableTest) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
  ASSERT_NE(top->getVariables(), nullptr) << "'string test[4]' should be a Variable, not a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const test = hldb::findByName<hldb::Variable>("test", top->getVariables());
  ASSERT_NE(test, nullptr) << "Variable 'test' not found";
}

TEST_F(ForeachTest, TestVariableTypespecIsStaticArray) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const test = hldb::findByName<hldb::Variable>("test", top->getVariables());
  ASSERT_NE(test, nullptr);
  const hldb::RefTypespec *const rts = test->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::ArrayTypespec *const at = rts->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'test' typespec should resolve to ArrayTypespec";
  EXPECT_EQ(at->getArrayType(), vpiStaticArray);
}

TEST_F(ForeachTest, InitialBodyIsForeachStmt) {
  const hldb::ForeachStmt *const fe = getForeachStmt();
  ASSERT_NE(fe, nullptr) << "the single statement in the initial body should resolve to ForeachStmt";
}

// ---------------------------------------------------------------------------
// "i" is declared local to the ForeachStmt's own implicit scope
// ---------------------------------------------------------------------------
TEST_F(ForeachTest, ForeachStmtScopeHasExactlyOneVariableI) {
  const hldb::ForeachStmt *const fe = getForeachStmt();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getVariables(), nullptr)
      << "the loop variable 'i' should live in the ForeachStmt's own local scope";
  ASSERT_EQ(fe->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", fe->getVariables()), nullptr) << "Variable 'i' not found";
}

TEST_F(ForeachTest, ForeachVariableIsTestResolvingToTheArrayVariable) {
  const hldb::ForeachStmt *const fe = getForeachStmt();
  ASSERT_NE(fe, nullptr);
  const hldb::RefObj *const arr = fe->getVariable<hldb::RefObj>();
  ASSERT_NE(arr, nullptr) << "ForeachStmt.getVariable() is not a RefObj";
  EXPECT_EQ(arr->getName(), "test");
  EXPECT_NE(arr->getActual<hldb::Variable>(), nullptr) << "'test' should resolve to the Variable";
}

TEST_F(ForeachTest, LoopVarsHasExactlyOneEntryMatchingI) {
  const hldb::ForeachStmt *const fe = getForeachStmt();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_EQ(fe->getLoopVars()->size(), 1u);
  const hldb::Variable *const loopVar = any_cast<hldb::Variable>(fe->getLoopVars()->at(0));
  ASSERT_NE(loopVar, nullptr) << "loop var entry should be the Variable 'i'";
  EXPECT_EQ(loopVar->getName(), "i");
}

// ---------------------------------------------------------------------------
// loop body: $display(i, test[i])
// ---------------------------------------------------------------------------
TEST_F(ForeachTest, LoopBodyDisplaysIAndTestOfI) {
  const hldb::ForeachStmt *const fe = getForeachStmt();
  ASSERT_NE(fe, nullptr);
  const hldb::SysTaskCall *const display = fe->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr) << "foreach-loop body should be a plain SysTaskCall (single statement, no begin-end)";
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);

  const hldb::RefObj *const iArg = any_cast<hldb::RefObj>(display->getArguments()->at(0));
  ASSERT_NE(iArg, nullptr);
  EXPECT_EQ(iArg->getName(), "i");
  EXPECT_NE(iArg->getActual<hldb::Variable>(), nullptr) << "'i' should resolve to the loop Variable";

  const hldb::BitSelect *const testI = any_cast<hldb::BitSelect>(display->getArguments()->at(1));
  ASSERT_NE(testI, nullptr) << "'test[i]' should be a BitSelect";
  EXPECT_EQ(testI->getName(), "test[i]");
  const hldb::RefObj *const prefix = testI->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr) << "'test' should resolve to the array Variable";
  const hldb::RefObj *const index = testI->getIndex<hldb::RefObj>();
  ASSERT_NE(index, nullptr);
  EXPECT_NE(index->getActual<hldb::Variable>(), nullptr) << "'i' index should resolve to the loop Variable";
}

TEST_F(ForeachTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
