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

// Tests for dut.sv (tags: NegInt)
//   module find_first_one #( parameter int WIDTH = -1) ();
//       for (genvar i = 0; i < WIDTH; i++) begin
//           assign in_tmp[i] = FLIP ? in_i[WIDTH-1-i] : in_i[i];
//       end
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 6.20.2: 'WIDTH' is a (non-local) value parameter of explicit type
//     'int' (6.11: int is a signed 2-state 32-bit type).
//   - 5.7.1: "A plus or minus operator preceding the size constant is a
//     unary plus or minus operator" -- the default '-1' is a unary minus
//     (vpiMinusOp) applied to the literal 1, not a negative literal.
//   - 27.4: the loop generate construct: genvar 'i' initialized to 0, loop
//     condition 'i < WIDTH' (vpiLtOp) whose 'WIDTH' operand binds to the
//     parameter, iteration 'i++' (vpiPostIncOp), and a begin-end generate
//     block containing exactly one continuous assignment.
//   - 11.4.11: the RHS is a conditional operator (vpiConditionOp) with three
//     operands.
//   - 11.3.2: binary '-' is left-associative, so 'WIDTH-1-i' is
//     '(WIDTH-1)-i': an outer vpiSubOp whose first operand is the inner
//     vpiSubOp 'WIDTH-1' and whose second operand is 'i'.
//   - the genvar 'i' references inside the generate block bind to the loop
//     variable.
//
// What is NOT checked and why:
//   - with the default WIDTH = -1, '0 < -1' is false, so the loop generates
//     zero blocks (27.4). Because the block is never instantiated, no
//     diagnostics are asserted for the undeclared 'FLIP' / 'in_i' or for the
//     implicit net that 'in_tmp' would otherwise introduce (6.10): this
//     non-elaborating flow does not decide which generate blocks exist.
//   - the HLDB Module::getName() of the definition (HLC decorates it with
//     the default parameterization); the module is looked up by def name.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NegIntTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NegInt.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByDefName<hldb::Module>("find_first_one", m_design->getAllModules());
  }

  static const hldb::Parameter *getWidth() {
    const hldb::Module *const m = getMod();
    if (m == nullptr || m->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *m->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == "WIDTH") return param;
    }
    return nullptr;
  }

  static const hldb::GenFor *getGenFor() {
    const hldb::Module *const m = getMod();
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const s : *m->getGenStmts()) {
      if (const hldb::GenFor *const gf = any_cast<hldb::GenFor>(s)) return gf;
    }
    return nullptr;
  }

  static const hldb::Variable *getGenvar() {
    const hldb::GenFor *const gf = getGenFor();
    return (gf == nullptr) ? nullptr : hldb::findByName<hldb::Variable>("i", gf->getVariables());
  }

  static const hldb::ContAssign *getAssign() {
    const hldb::GenFor *const gf = getGenFor();
    if (gf == nullptr) return nullptr;
    const hldb::Begin *const b = any_cast<hldb::Begin>(gf->getStmt());
    if (b == nullptr || b->getStmts() == nullptr || b->getStmts()->empty()) return nullptr;
    return any_cast<hldb::ContAssign>(b->getStmts()->at(0));
  }

  static const hldb::Operation *getCond() {
    const hldb::ContAssign *const ca = getAssign();
    return (ca == nullptr) ? nullptr : any_cast<hldb::Operation>(ca->getRhs());
  }
};

TEST_F(NegIntTest, ModuleExists) { EXPECT_NE(getMod(), nullptr); }

// ===========================================================================
// 6.20.2 / 5.7.1: parameter int WIDTH = -1
// ===========================================================================

TEST_F(NegIntTest, WidthIsIntParameter) {
  const hldb::Parameter *const w = getWidth();
  ASSERT_NE(w, nullptr) << "parameter 'WIDTH' not found";
  EXPECT_FALSE(w->getLocalParam()) << "declared with 'parameter', not 'localparam'";
  ASSERT_NE(w->getTypespec(), nullptr);
  const hldb::IntTypespec *const its = any_cast<hldb::IntTypespec>(w->getTypespec()->getActual());
  ASSERT_NE(its, nullptr) << "explicit type 'int'";
  EXPECT_TRUE(its->getSigned()) << "6.11: int is signed";
}

TEST_F(NegIntTest, DefaultIsUnaryMinusOne) {
  const hldb::Module *const m = getMod();
  ASSERT_NE(m, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("WIDTH", hldb::getParamAssigns(m));
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'WIDTH' not found";
  const hldb::Operation *const op = any_cast<hldb::Operation>(pa->getRhs());
  ASSERT_NE(op, nullptr) << "5.7.1: '-1' is a unary minus applied to 1";
  EXPECT_EQ(op->getOpType(), vpiMinusOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const hldb::Constant *const one = any_cast<hldb::Constant>(op->getOperands()->at(0));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
}

// ===========================================================================
// 27.4: for (genvar i = 0; i < WIDTH; i++) begin ... end
// ===========================================================================

TEST_F(NegIntTest, GenForExistsWithGenvarI) {
  ASSERT_NE(getGenFor(), nullptr) << "loop generate construct not found";
  EXPECT_NE(getGenvar(), nullptr) << "'genvar i' declared in the loop header";
}

TEST_F(NegIntTest, InitIsIEqualsZero) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getForInitStmts(), nullptr);
  ASSERT_EQ(gf->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(gf->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getLhs(), nullptr);
  EXPECT_EQ(init->getLhs(), getGenvar());
  const hldb::Constant *const zero = any_cast<hldb::Constant>(init->getRhs());
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "0");
}

TEST_F(NegIntTest, ConditionIsILessThanWidth) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  const hldb::Operation *const cond = any_cast<hldb::Operation>(gf->getCondition());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getName(), "i");
  EXPECT_EQ(lhs->getActual(), getGenvar());
  EXPECT_EQ(rhs->getName(), "WIDTH");
  ASSERT_NE(rhs->getActual(), nullptr);
  EXPECT_EQ(rhs->getActual(), getWidth());
}

TEST_F(NegIntTest, IncrementIsPostIncI) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getForIncStmts(), nullptr);
  ASSERT_EQ(gf->getForIncStmts()->size(), 1u);
  const hldb::Operation *const inc = any_cast<hldb::Operation>(gf->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
  ASSERT_NE(inc->getOperands(), nullptr);
  ASSERT_EQ(inc->getOperands()->size(), 1u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(inc->getOperands()->at(0));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getActual(), getGenvar());
}

TEST_F(NegIntTest, BodyIsBeginWithOneContAssign) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  const hldb::Begin *const b = any_cast<hldb::Begin>(gf->getStmt());
  ASSERT_NE(b, nullptr) << "generate block is an explicit begin-end";
  ASSERT_NE(b->getStmts(), nullptr);
  ASSERT_EQ(b->getStmts()->size(), 1u);
  EXPECT_NE(getAssign(), nullptr) << "the single item is a continuous assignment";
}

// ===========================================================================
// assign in_tmp[i] = FLIP ? in_i[WIDTH-1-i] : in_i[i];
// ===========================================================================

TEST_F(NegIntTest, LhsIsSelectIndexedByGenvar) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(ca->getLhs());
  ASSERT_NE(bs, nullptr);
  const hldb::RefObj *const prefix = any_cast<hldb::RefObj>(bs->getPrefix());
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "in_tmp");
  const hldb::RefObj *const idx = any_cast<hldb::RefObj>(bs->getIndex());
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getActual(), getGenvar());
}

TEST_F(NegIntTest, RhsIsConditionalWithThreeOperands) {
  const hldb::Operation *const cond = getCond();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiConditionOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3u);
  const hldb::RefObj *const sel = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getName(), "FLIP");
}

TEST_F(NegIntTest, IndexWidthMinus1MinusIIsLeftAssociative) {
  const hldb::Operation *const cond = getCond();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3u);
  const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(cond->getOperands()->at(1));
  ASSERT_NE(bs, nullptr) << "'in_i[WIDTH-1-i]'";
  const hldb::Operation *const outer = any_cast<hldb::Operation>(bs->getIndex());
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getOpType(), vpiSubOp);
  ASSERT_NE(outer->getOperands(), nullptr);
  ASSERT_EQ(outer->getOperands()->size(), 2u);

  const hldb::Operation *const inner = any_cast<hldb::Operation>(outer->getOperands()->at(0));
  ASSERT_NE(inner, nullptr) << "11.3.2: '(WIDTH-1)' is the left operand of the outer subtraction";
  EXPECT_EQ(inner->getOpType(), vpiSubOp);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_EQ(inner->getOperands()->size(), 2u);
  const hldb::RefObj *const width = any_cast<hldb::RefObj>(inner->getOperands()->at(0));
  ASSERT_NE(width, nullptr);
  EXPECT_EQ(width->getActual(), getWidth());
  const hldb::Constant *const one = any_cast<hldb::Constant>(inner->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");

  const hldb::RefObj *const i = any_cast<hldb::RefObj>(outer->getOperands()->at(1));
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getActual(), getGenvar());
}

TEST_F(NegIntTest, ElseBranchIsInIIndexedByGenvar) {
  const hldb::Operation *const cond = getCond();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3u);
  const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(cond->getOperands()->at(2));
  ASSERT_NE(bs, nullptr);
  const hldb::RefObj *const prefix = any_cast<hldb::RefObj>(bs->getPrefix());
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "in_i");
  const hldb::RefObj *const idx = any_cast<hldb::RefObj>(bs->getIndex());
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getActual(), getGenvar());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
