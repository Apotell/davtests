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

// Tests for dut.sv (tags: ForeachSquare)
//   module dut;
//     int array[16][16];
//     initial begin
//       foreach(array[i])
//         foreach(array[i][j])
//           array[i][j] = i * j;
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 Sec 12.7.3 "The foreach loop
// construct", checked before any test code was written):
//   "array" is a 2-dimensional unpacked array (16x16). The outer
//   "foreach(array[i])" supplies only one loop variable for a
//   2-dimensional array, which per 12.7.3 is legal: "If a dimension
//   variable is not needed... it can be omitted" -- omitting the second
//   dimension's loop variable simply means the outer loop iterates only
//   the first dimension, leaving the second dimension unindexed. The
//   nested "foreach(array[i][j])" then re-invokes foreach, this time on
//   the bit-select "array[i]" (itself a 1-dimensional 16-element array
//   -- row i of the outer array) with loop variable "j" for its single
//   remaining dimension. Both "array_name" arguments to ForeachStmt
//   resolve to a RefObj: the array-name grammar production does not
//   restrict this to a bare identifier, and HLC (consistent with how
//   hierarchical/indexed lvalues are elsewhere flattened into a single
//   RefObj with the full source text as name, e.g.
//   DoWhile/DoWhile/test_DoWhile.cpp's "m_sync[i].m_state") models
//   "array[i]" as one RefObj named "array[i]".
//
// What is checked:
//   - module 'dut' exists with exactly 1 Initial process
//   - initial's body is an explicit Begin (source has begin-end) holding
//     exactly 1 statement: the outer ForeachStmt
//   - outer ForeachStmt: array_name RefObj "array", exactly 1 implicit
//     loop variable "i" (Variable, getIsIterator())
//   - outer's body (no begin-end) is directly the inner ForeachStmt
//   - inner ForeachStmt: array_name RefObj "array[i]", exactly 1 implicit
//     loop variable "j"
//   - inner's body (no begin-end) is directly the Assignment
//     "array[i][j] = i * j": LHS RefObj "array[i][j]", RHS
//     Operation(vpiMultOp) over RefObj "i" and RefObj "j"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/foreach_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForeachSquareTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ForeachSquare.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() {
    return hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  }

  static const hldb::Initial *getInitial() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr || dut->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const p : *dut->getProcesses()) {
      if (const hldb::Initial *const init = any_cast<hldb::Initial>(p)) return init;
    }
    return nullptr;
  }

  static const hldb::ForeachStmt *getOuterForeach() {
    const hldb::Initial *const init = getInitial();
    if (init == nullptr) return nullptr;
    const hldb::Begin *const body = init->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::ForeachStmt>(body->getStmts()->at(0));
  }

  static void CheckLoopVar(const hldb::ForeachStmt *fe, std::string_view varName) {
    ASSERT_NE(fe, nullptr);
    ASSERT_NE(fe->getLoopVars(), nullptr);
    ASSERT_EQ(fe->getLoopVars()->size(), 1u);
    const hldb::Variable *const v = any_cast<hldb::Variable>(fe->getLoopVars()->at(0));
    ASSERT_NE(v, nullptr) << "loop variable '" << varName << "' must be an implicitly declared Variable";
    EXPECT_EQ(v->getName(), varName);
    EXPECT_TRUE(v->getIsIterator());
  }
};

// ---------------------------------------------------------------------------
// Module / process existence
// ---------------------------------------------------------------------------

TEST_F(ForeachSquareTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr); }

TEST_F(ForeachSquareTest, InitialExists) { EXPECT_NE(getInitial(), nullptr); }

TEST_F(ForeachSquareTest, InitialBodyIsExplicitBeginWithOneStmt) {
  const hldb::Initial *const init = getInitial();
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = init->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "initial body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u);
}

// ---------------------------------------------------------------------------
// Outer: foreach(array[i])
// ---------------------------------------------------------------------------

TEST_F(ForeachSquareTest, OuterForeachExists) { EXPECT_NE(getOuterForeach(), nullptr); }

TEST_F(ForeachSquareTest, OuterArrayNameIsRefObjArray) {
  const hldb::ForeachStmt *const outer = getOuterForeach();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getVariable(), nullptr) << "12.7.3: array_name must be present";
  EXPECT_EQ(outer->getVariable()->getName(), std::string_view{"array"});
}

TEST_F(ForeachSquareTest, OuterHasOneLoopVariableI) { CheckLoopVar(getOuterForeach(), "i"); }

// ---------------------------------------------------------------------------
// Inner: foreach(array[i][j]) -- directly the outer's body (no begin-end)
// ---------------------------------------------------------------------------

TEST_F(ForeachSquareTest, OuterBodyIsDirectlyInnerForeach) {
  const hldb::ForeachStmt *const outer = getOuterForeach();
  ASSERT_NE(outer, nullptr);
  const hldb::ForeachStmt *const inner = outer->getStmt<hldb::ForeachStmt>();
  ASSERT_NE(inner, nullptr) << "outer foreach has no begin-end, so its body is directly the inner ForeachStmt";
}

TEST_F(ForeachSquareTest, InnerArrayNameIsRefObjArrayI) {
  const hldb::ForeachStmt *const outer = getOuterForeach();
  ASSERT_NE(outer, nullptr);
  const hldb::ForeachStmt *const inner = outer->getStmt<hldb::ForeachStmt>();
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getVariable(), nullptr) << "12.7.3: array_name must be present";
  EXPECT_EQ(inner->getVariable()->getName(), std::string_view{"array[i]"});
}

TEST_F(ForeachSquareTest, InnerHasOneLoopVariableJ) {
  const hldb::ForeachStmt *const outer = getOuterForeach();
  ASSERT_NE(outer, nullptr);
  CheckLoopVar(outer->getStmt<hldb::ForeachStmt>(), "j");
}

// ---------------------------------------------------------------------------
// Innermost body: "array[i][j] = i * j;" -- directly the inner's body
// ---------------------------------------------------------------------------

TEST_F(ForeachSquareTest, InnerBodyIsDirectlyAssignArrayIJEqualsIMulJ) {
  const hldb::ForeachStmt *const outer = getOuterForeach();
  ASSERT_NE(outer, nullptr);
  const hldb::ForeachStmt *const inner = outer->getStmt<hldb::ForeachStmt>();
  ASSERT_NE(inner, nullptr);
  const hldb::Assignment *const assign = inner->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'array[i][j] = i * j;' has no begin-end, so it is directly the inner body";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"array[i][j]"});

  ASSERT_NE(assign->getRhs(), nullptr);
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'i * j' should be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiMultOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);
  const hldb::RefObj *const opI = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(opI, nullptr);
  EXPECT_EQ(opI->getName(), std::string_view{"i"});
  const hldb::RefObj *const opJ = any_cast<hldb::RefObj>(rhs->getOperands()->at(1));
  ASSERT_NE(opJ, nullptr);
  EXPECT_EQ(opJ->getName(), std::string_view{"j"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
