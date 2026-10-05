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

// Tests for tests/LoopVar/dut.sv (tags: LoopVar)
//   module dut;
//       int array[16];
//       initial begin
//           foreach(array[i])
//               array[i] = i;
//       end
//   endmodule
//
// The construct under test is the implicitly declared foreach-loop variable
// "i" and its uses inside the loop body.
//
// What is checked (IEEE 1800-2023):
//   - 7.4.2: "int array[16]" is a fixed-size unpacked array; "[size] shall
//     mean the same as [0:size-1]", so its range is [0:15]; the element type
//     is int (6.11).
//   - 12.7.3: the foreach argument names the array ("array") and lists one
//     loop variable "i" for its single dimension. "a foreach-loop creates an
//     implicit begin-end block around the loop statement, containing
//     declarations of the loop variables with automatic lifetime. This
//     block creates a new hierarchical scope, making the variables local to
//     the loop scope." So "i" is a Variable owned by the ForeachStmt scope,
//     automatic, and not a module-level variable.
//   - 12.7.3: "When loop variables are used in expressions other than as
//     indices to the designated array, they are auto-cast into a type
//     consistent with the type of index. For fixed-size and dynamic arrays,
//     the auto-cast type is int." -- i's type is int.
//   - 12.7.3: "It shall be an error for any loop variable to have the same
//     identifier as the array" -- "i" differs from "array", so no
//     COMP_ILLEGAL_LOOP_VARIABLE.
//   - the body "array[i] = i;" is a blocking Assignment (10.4.1) whose LHS
//     is a bit-select of "array" indexed by "i" and whose RHS is "i"; both
//     uses of "i" bind to the loop variable.
//
// What is NOT checked and why:
//   - run-time iteration order/values (0..15): simulation semantics.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/foreach_stmt.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdlib>
#include <optional>
#include <string>

namespace hlc {

class LoopVarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LoopVar.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Variable *getArray() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("array", dut->getVariables());
  }

  static const hldb::ForeachStmt *getForeach() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr || dut->getProcesses() == nullptr || dut->getProcesses()->size() != 1) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(dut->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
    if (blk == nullptr || blk->getStmts() == nullptr || blk->getStmts()->size() != 1) return nullptr;
    return any_cast<hldb::ForeachStmt>(blk->getStmts()->at(0));
  }

  static const hldb::Variable *getLoopVarI() {
    const hldb::ForeachStmt *const fe = getForeach();
    if (fe == nullptr || fe->getLoopVars() == nullptr || fe->getLoopVars()->size() != 1) return nullptr;
    return any_cast<hldb::Variable>(fe->getLoopVars()->at(0));
  }

  // Evaluates a literal or a binary +/- of literals; nullopt otherwise.
  static std::optional<long> evalConst(const hldb::Any *expr) {
    if (expr == nullptr) return std::nullopt;
    if (const hldb::Constant *const c = any_cast<hldb::Constant>(expr)) {
      const std::string text(c->getDecompile());
      char *end = nullptr;
      const long v = std::strtol(text.c_str(), &end, 10);
      if (end == text.c_str() || *end != '\0') return std::nullopt;
      return v;
    }
    if (const hldb::Operation *const op = any_cast<hldb::Operation>(expr)) {
      if (op->getOperands() == nullptr || op->getOperands()->size() != 2) return std::nullopt;
      const std::optional<long> a = evalConst(op->getOperands()->at(0));
      const std::optional<long> b = evalConst(op->getOperands()->at(1));
      if (!a || !b) return std::nullopt;
      if (op->getOpType() == vpiSubOp) return *a - *b;
      if (op->getOpType() == vpiAddOp) return *a + *b;
    }
    return std::nullopt;
  }
};

TEST_F(LoopVarTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr); }

// 7.4.2: fixed-size unpacked array of int.
TEST_F(LoopVarTest, ArrayIsFixedSizeUnpackedArrayOfInt) {
  const hldb::Variable *const arr = getArray();
  ASSERT_NE(arr, nullptr) << "variable 'array' not found in dut";
  ASSERT_NE(arr->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = arr->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'int array[16]' should have an ArrayTypespec";
  EXPECT_EQ(at->getArrayType(), vpiStaticArray);
  EXPECT_FALSE(at->getPacked());
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "element type is int";
}

// 7.4.2: "[size] shall mean the same as [0:size-1]".
TEST_F(LoopVarTest, ArrayRangeIsZeroTo15) {
  const hldb::Variable *const arr = getArray();
  ASSERT_NE(arr, nullptr);
  ASSERT_NE(arr->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = arr->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::Range *const r = at->getRange();
  ASSERT_NE(r, nullptr);
  ASSERT_NE(r->getLeftExpr(), nullptr);
  ASSERT_NE(r->getRightExpr(), nullptr) << "7.4.2: [16] is [0:15]; both bounds must be present";
  const std::optional<long> left = evalConst(r->getLeftExpr());
  const std::optional<long> right = evalConst(r->getRightExpr());
  ASSERT_TRUE(left.has_value());
  ASSERT_TRUE(right.has_value());
  EXPECT_EQ(*left, 0);
  EXPECT_EQ(*right, 15);
}

TEST_F(LoopVarTest, ForeachStmtExists) {
  EXPECT_NE(getForeach(), nullptr) << "initial begin ... end should contain exactly one ForeachStmt";
}

// 12.7.3: the foreach argument names the array.
TEST_F(LoopVarTest, ForeachArrayBindsToArray) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::RefObj *const v = fe->getVariable();
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getName(), "array");
  ASSERT_NE(v->getActual(), nullptr);
  EXPECT_EQ(v->getActual(), getArray());
}

// 12.7.3: one loop variable for the single dimension.
TEST_F(LoopVarTest, ForeachHasSingleLoopVariableI) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_EQ(fe->getLoopVars()->size(), 1u);
  const hldb::Variable *const i = getLoopVarI();
  ASSERT_NE(i, nullptr) << "loop variable should be a Variable";
  EXPECT_EQ(i->getName(), "i");
}

// 12.7.3: loop variables have automatic lifetime.
TEST_F(LoopVarTest, LoopVariableIsAutomatic) {
  const hldb::Variable *const i = getLoopVarI();
  ASSERT_NE(i, nullptr);
  EXPECT_TRUE(i->getAutomatic()) << "12.7.3: loop variables are declared with automatic lifetime";
}

// 12.7.3: auto-cast type for a fixed-size array index is int.
TEST_F(LoopVarTest, LoopVariableTypeIsInt) {
  const hldb::Variable *const i = getLoopVarI();
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->getTypespec(), nullptr) << "12.7.3: loop variable type is implicitly declared";
  ASSERT_NE(i->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(i->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
}

// 12.7.3: the loop variable is local to the foreach scope.
TEST_F(LoopVarTest, LoopVariableIsLocalToForeachScope) {
  const hldb::ForeachStmt *const fe = getForeach();
  const hldb::Variable *const i = getLoopVarI();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getParent(), fe) << "12.7.3: the implicit block around the loop declares i";
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("i", dut->getVariables()), nullptr)
      << "12.7.3: i is local to the loop scope, not a module variable";
}

// array[i] = i;
TEST_F(LoopVarTest, BodyIsBlockingAssignmentUsingLoopVariable) {
  const hldb::ForeachStmt *const fe = getForeach();
  const hldb::Variable *const i = getLoopVarI();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(i, nullptr);
  const hldb::Assignment *const as = fe->getStmt<hldb::Assignment>();
  ASSERT_NE(as, nullptr) << "loop body should be an Assignment";
  EXPECT_TRUE(as->getBlocking());

  const hldb::BitSelect *const lhs = as->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr) << "LHS 'array[i]' should be a BitSelect";
  const hldb::RefObj *const prefix = lhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "array");
  EXPECT_EQ(prefix->getActual(), getArray());
  const hldb::RefObj *const idx = lhs->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getName(), "i");
  EXPECT_EQ(idx->getActual(), i);

  const hldb::RefObj *const rhs = as->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "i");
  EXPECT_EQ(rhs->getActual(), i);
}

TEST_F(LoopVarTest, NoLoopVariableDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_LOOP_VARIABLE), nullptr)
      << "12.7.3: 'i' does not share the array's identifier";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "i"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
