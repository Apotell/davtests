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

// Tests for dut.sv (tags: LeftPadding)
//   module socket_1n ();
//     parameter bit [7:0] P1 = {1{4'h2, 4'h2}};
//     parameter bit [7:0] P2 = {2{4'h2}};
//     parameter bit [7:0] P3 = {1{4'b10, 4'h2}};
//     parameter bit [7:0] P4 = {2{4'b10}};
//
//     if (P1 == P2 && P2 == P3 && P3 == P4 && P4 == 34) begin
//       GOOD good();
//     end
//   endmodule
//
// The file exercises left-padding of based literals (5.7.1): "4'b10" is a
// 4-bit literal whose value is left-padded with zeros to 4'b0010, so every
// one of P1..P4 is 8'h22 == 34 and the generate condition is true.
//
// What is checked (IEEE 1800-2023):
//   - module 'socket_1n' exists with exactly 4 parameters P1..P4, none of
//     them local (6.20.1)
//   - each parameter is typed 'bit [7:0]' (6.20.2: type and range given)
//   - each parameter's default value is a multiple concatenation
//     (11.4.12.1): vpiMultiConcatOp whose first operand is the replication
//     constant (1 or 2) and whose second operand is a concatenation
//     (vpiConcatOp) of the listed 4-bit literals
//   - "4'b10" is a 4-bit binary constant (5.7.1: the size is given by the
//     size constant, not by the number of digits written)
//   - the conditional generate (27.5): condition is the left-associative
//     chain of '&&' (11.3.2) whose operands are '==' comparisons; the
//     body instantiates 'GOOD' as 'good'
//
// What is NOT checked and why:
//   - the numerical evaluation of P1..P4 and of the generate condition:
//     this .hlc does not request elaboration, so parameter values remain
//     unreduced expressions in the folded model.
//   - whether a missing definition for module 'GOOD' is diagnosed: that is
//     an elaboration-time check (23.3.1) and elaboration is not requested.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LeftPaddingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LeftPadding.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByDefName<hldb::Module>("socket_1n", m_design->getAllModules());
  }

  static const hldb::Parameter *findParam(std::string_view name) {
    const hldb::Module *const m = getModule();
    if (m == nullptr || m->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *m->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  static const hldb::ParamAssign *findParamAssign(std::string_view name) {
    const hldb::Module *const m = getModule();
    if (m == nullptr || m->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *m->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if (lhs != nullptr && lhs->getName() == name) return pa;
      const hldb::Parameter *const lhsp = pa->getLhs<hldb::Parameter>();
      if (lhsp != nullptr && lhsp->getName() == name) return pa;
    }
    return nullptr;
  }

  static const hldb::GenIf *getGenIf() {
    const hldb::Module *const m = getModule();
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const s : *m->getGenStmts()) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(s)) return gi;
    }
    return nullptr;
  }

  // Checks 'name = {repl{lits...}}' with each literal a 4-bit constant whose
  // source text is given in 'lits'.
  static void checkMultiConcat(std::string_view name, std::string_view repl,
                               const std::vector<std::string_view> &lits) {
    const hldb::ParamAssign *const pa = findParamAssign(name);
    ASSERT_NE(pa, nullptr) << "ParamAssign for '" << name << "' not found";
    const hldb::Operation *const mc = pa->getRhs<hldb::Operation>();
    ASSERT_NE(mc, nullptr) << "'" << name << "' default must be an Operation";
    EXPECT_EQ(mc->getOpType(), vpiMultiConcatOp) << "11.4.12.1: '{N{...}}' is a multiple concatenation";
    ASSERT_NE(mc->getOperands(), nullptr);
    ASSERT_EQ(mc->getOperands()->size(), 2u) << "replication constant + concatenation";
    const hldb::Constant *const rc = any_cast<hldb::Constant>(mc->getOperands()->at(0));
    ASSERT_NE(rc, nullptr) << "replication multiplier must be a Constant";
    EXPECT_EQ(rc->getDecompile(), repl);
    const hldb::Operation *const cc = any_cast<hldb::Operation>(mc->getOperands()->at(1));
    ASSERT_NE(cc, nullptr) << "replicated item must be a concatenation";
    EXPECT_EQ(cc->getOpType(), vpiConcatOp);
    ASSERT_NE(cc->getOperands(), nullptr);
    ASSERT_EQ(cc->getOperands()->size(), lits.size());
    for (size_t i = 0; i < lits.size(); ++i) {
      const hldb::Constant *const c = any_cast<hldb::Constant>(cc->getOperands()->at(i));
      ASSERT_NE(c, nullptr) << "operand " << i << " must be a Constant";
      EXPECT_EQ(c->getDecompile(), lits[i]);
      EXPECT_EQ(c->getSize(), 4) << "5.7.1: '" << lits[i] << "' has an explicit size of 4 bits";
      const bool isBin = (lits[i].find("'b") != std::string_view::npos);
      EXPECT_EQ(c->getConstType(), isBin ? vpiBinaryConst : vpiHexConst);
    }
  }

  // '(lhs == rhs)'
  static void checkEq(const hldb::Any *any, std::string_view lhs, std::string_view rhs) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(any);
    ASSERT_NE(op, nullptr) << "expected '" << lhs << " == " << rhs << "'";
    EXPECT_EQ(op->getOpType(), vpiEqOp);
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 2u);
    const hldb::RefObj *const l = any_cast<hldb::RefObj>(op->getOperands()->at(0));
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(l->getName(), lhs);
    ASSERT_NE(l->getActual(), nullptr);
    EXPECT_EQ(l->getActual()->getAnyType(), hldb::AnyType::Parameter);
    if (const hldb::RefObj *const r = any_cast<hldb::RefObj>(op->getOperands()->at(1))) {
      EXPECT_EQ(r->getName(), rhs);
    } else {
      const hldb::Constant *const c = any_cast<hldb::Constant>(op->getOperands()->at(1));
      ASSERT_NE(c, nullptr);
      EXPECT_EQ(c->getDecompile(), rhs);
    }
  }
};

// ---------------------------------------------------------------------------
// Module and parameters -- 6.20.1, 6.20.2
// ---------------------------------------------------------------------------

TEST_F(LeftPaddingTest, ModuleExists) { EXPECT_NE(getModule(), nullptr) << "module 'socket_1n' not found"; }

TEST_F(LeftPaddingTest, HasFourNonLocalParameters) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 4u);
  for (std::string_view name : {"P1", "P2", "P3", "P4"}) {
    const hldb::Parameter *const p = findParam(name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_FALSE(p->getLocalParam()) << name;
  }
}

TEST_F(LeftPaddingTest, ParametersAreBit7To0) {
  for (std::string_view name : {"P1", "P2", "P3", "P4"}) {
    const hldb::Parameter *const p = findParam(name);
    ASSERT_NE(p, nullptr) << name;
    ASSERT_NE(p->getTypespec(), nullptr) << name;
    const hldb::BitTypespec *const bt = p->getTypespec()->getActual<hldb::BitTypespec>();
    ASSERT_NE(bt, nullptr) << "6.20.2: '" << name << "' is declared 'bit [7:0]'";
    EXPECT_FALSE(bt->getSigned()) << name;
    ASSERT_NE(bt->getRanges(), nullptr) << name;
    ASSERT_EQ(bt->getRanges()->size(), 1u) << name;
    const hldb::Range *const r = bt->getRanges()->at(0);
    const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << name;
    ASSERT_NE(right, nullptr) << name;
    EXPECT_EQ(left->getValue(), "7") << name;
    EXPECT_EQ(right->getValue(), "0") << name;
  }
}

// ---------------------------------------------------------------------------
// Default values -- 11.4.12.1 replication, 5.7.1 sized literals
// ---------------------------------------------------------------------------

TEST_F(LeftPaddingTest, P1IsOneTimesTwoHexNibbles) { checkMultiConcat("P1", "1", {"4'h2", "4'h2"}); }

TEST_F(LeftPaddingTest, P2IsTwoTimesOneHexNibble) { checkMultiConcat("P2", "2", {"4'h2"}); }

TEST_F(LeftPaddingTest, P3IsOneTimesPaddedBinaryAndHex) { checkMultiConcat("P3", "1", {"4'b10", "4'h2"}); }

TEST_F(LeftPaddingTest, P4IsTwoTimesPaddedBinary) { checkMultiConcat("P4", "2", {"4'b10"}); }

// ---------------------------------------------------------------------------
// Conditional generate -- 27.5, 11.3.2 (&& is left-associative, == binds
// tighter than &&)
// ---------------------------------------------------------------------------

TEST_F(LeftPaddingTest, GenIfExists) { EXPECT_NE(getGenIf(), nullptr) << "'if (...) begin ... end' not found"; }

TEST_F(LeftPaddingTest, GenIfConditionIsLeftAssociativeAndChain) {
  const hldb::GenIf *const gi = getGenIf();
  ASSERT_NE(gi, nullptr);
  // (((P1 == P2 && P2 == P3) && P3 == P4) && P4 == 34)
  const hldb::Operation *const and3 = gi->getCondition<hldb::Operation>();
  ASSERT_NE(and3, nullptr);
  EXPECT_EQ(and3->getOpType(), vpiLogAndOp);
  ASSERT_NE(and3->getOperands(), nullptr);
  ASSERT_EQ(and3->getOperands()->size(), 2u);
  checkEq(and3->getOperands()->at(1), "P4", "34");

  const hldb::Operation *const and2 = any_cast<hldb::Operation>(and3->getOperands()->at(0));
  ASSERT_NE(and2, nullptr);
  EXPECT_EQ(and2->getOpType(), vpiLogAndOp);
  ASSERT_NE(and2->getOperands(), nullptr);
  ASSERT_EQ(and2->getOperands()->size(), 2u);
  checkEq(and2->getOperands()->at(1), "P3", "P4");

  const hldb::Operation *const and1 = any_cast<hldb::Operation>(and2->getOperands()->at(0));
  ASSERT_NE(and1, nullptr);
  EXPECT_EQ(and1->getOpType(), vpiLogAndOp);
  ASSERT_NE(and1->getOperands(), nullptr);
  ASSERT_EQ(and1->getOperands()->size(), 2u);
  checkEq(and1->getOperands()->at(0), "P1", "P2");
  checkEq(and1->getOperands()->at(1), "P2", "P3");
}

TEST_F(LeftPaddingTest, GenIfBodyInstantiatesGood) {
  const hldb::GenIf *const gi = getGenIf();
  ASSERT_NE(gi, nullptr);
  ASSERT_NE(gi->getStmt(), nullptr) << "'begin GOOD good(); end' body missing";
  const hldb::RefInstance *ri = gi->getStmt<hldb::RefInstance>();
  if (ri == nullptr) {
    const hldb::Begin *const b = gi->getStmt<hldb::Begin>();
    ASSERT_NE(b, nullptr) << "body must be the begin-end block";
    ASSERT_NE(b->getStmts(), nullptr);
    ASSERT_EQ(b->getStmts()->size(), 1u);
    ri = any_cast<hldb::RefInstance>(b->getStmts()->at(0));
  }
  ASSERT_NE(ri, nullptr) << "'GOOD good();' must be a module instance";
  EXPECT_EQ(ri->getName(), "good");
  ASSERT_NE(ri->getTypespec(), nullptr);
  EXPECT_EQ(ri->getTypespec()->getName(), "GOOD");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
