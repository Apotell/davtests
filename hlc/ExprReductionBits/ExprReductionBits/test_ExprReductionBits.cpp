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

// Tests for tests/ExprReductionBits/dut.sv:
//
//   module top(output int o);
//       parameter int PARAM = 4;
//       typedef enum logic[$bits(PARAM) - 32 + 85 + PARAM:0] {
//           LOGIC90PB_0_ITEM_0 = 0,
//           LOGIC90PB_0_ITEM_1 = 1
//       } logic90pb_0_e;
//   endmodule
//
// NOTE: despite this test's name, tests/ExprReductionBits/dut.sv contains
// no reduction operator (&, |, ^, ~&, ~|, ~^ -- IEEE 1800-2023 Sec 11.4.9)
// anywhere in the source; byte-for-byte it is the same file as
// tests/ExprEvalBits/dut.sv (confirmed identical -- both compute a packed
// range bound from "$bits(PARAM) - 32 + 85 + PARAM"). Per the test-writing
// guide's instruction to read the actual .sv source rather than assume
// what a test name implies, this file exercises exactly what is present:
// a bit-width-related constant expression forming an enum's packed range,
// the same construct verified in test_ExprEvalBits.cpp. There is nothing
// reduction-operator-specific to add; this file mirrors that coverage
// under its own required class name so both .hlc/dut.sv fixtures have a
// corresponding test.
//
// IEEE 1800-2023 Sec 20.6.2 defines "$bits" as a constant system function.
// Sec 11.3.1's operator-precedence table places binary '+' and '-' at the
// same precedence, left-associative, so the range's left bound parses as
// "((($bits(PARAM) - 32) + 85) + PARAM)" -- a top-level vpiAddOp.
//
// Checked (mirrors test_ExprEvalBits.cpp):
//   - module "top" exists with 1 output IODecl "o", and parameter "PARAM"
//     (not localparam, default Constant "4")
//   - "top" declares exactly 1 Typedef "logic90pb_0_e" whose alias
//     resolves to an EnumTypespec, whose base typespec resolves to a
//     LogicTypespec with exactly 1 Range
//   - the range's right bound is Constant "0"; the left bound is the
//     top-level '+' Operation described above, decomposed down to the
//     innermost '-' Operation whose first operand is SysFuncCall "$bits"
//     applied to a RefObj "PARAM" resolving to the module's own Parameter
//   - the enum declares 2 EnumConsts with values "0" and "1"
//   - compiler reports zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/typedef.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ExprReductionBitsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ExprReductionBits.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  }

  static const hldb::Parameter *getParam() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *top->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == "PARAM") return param;
    }
    return nullptr;
  }

  static const hldb::Typedef *getEnumTypedef() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTypedefs() == nullptr || top->getTypedefs()->empty()) return nullptr;
    return top->getTypedefs()->at(0);
  }

  static const hldb::EnumTypespec *getEnumTypespec() {
    const hldb::Typedef *const td = getEnumTypedef();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    return td->getAlias()->getActual<hldb::EnumTypespec>();
  }

  static const hldb::LogicTypespec *getBaseLogicTypespec() {
    const hldb::EnumTypespec *const et = getEnumTypespec();
    if (et == nullptr || et->getEnum() == nullptr || et->getEnum()->getBaseTypespec() == nullptr) return nullptr;
    return et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Range *getRange() {
    const hldb::LogicTypespec *const lt = getBaseLogicTypespec();
    if (lt == nullptr || lt->getRanges() == nullptr || lt->getRanges()->empty()) return nullptr;
    return lt->getRanges()->at(0);
  }
};

// ---------------------------------------------------------------------------
// module / parameter
// ---------------------------------------------------------------------------

TEST_F(ExprReductionBitsTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(ExprReductionBitsTest, ModuleHasOutputPortO) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getIODecls(), nullptr);
  ASSERT_EQ(top->getIODecls()->size(), 1u);
  const hldb::IODecl *const o = top->getIODecls()->at(0);
  ASSERT_NE(o, nullptr);
  EXPECT_EQ(o->getName(), "o");
  EXPECT_EQ(o->getDirection(), vpiOutput);
}

TEST_F(ExprReductionBitsTest, ParamExistsAndIsNotLocalParam) {
  const hldb::Parameter *const param = getParam();
  ASSERT_NE(param, nullptr) << "'parameter int PARAM = 4' not found on module 'top'";
  EXPECT_FALSE(param->getLocalParam());
}

TEST_F(ExprReductionBitsTest, ParamDefaultIsFour) {
  const hldb::Parameter *const param = getParam();
  ASSERT_NE(param, nullptr);
  const hldb::Constant *const expr = param->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), "4");
}

// ---------------------------------------------------------------------------
// typedef logic90pb_0_e
// ---------------------------------------------------------------------------

TEST_F(ExprReductionBitsTest, ModuleHasOneTypedef) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypedefs(), nullptr);
  EXPECT_EQ(top->getTypedefs()->size(), 1u);
  const hldb::Typedef *const td = getEnumTypedef();
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getName(), "logic90pb_0_e");
}

TEST_F(ExprReductionBitsTest, TypedefAliasResolvesToEnumTypespec) {
  const hldb::Typedef *const td = getEnumTypedef();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  EXPECT_NE(getEnumTypespec(), nullptr) << "'typedef enum ...' alias should resolve to an EnumTypespec";
}

TEST_F(ExprReductionBitsTest, EnumBaseTypespecIsLogic) {
  EXPECT_NE(getBaseLogicTypespec(), nullptr) << "'enum logic[...:0] {...}' base typespec should be LogicTypespec";
}

// ---------------------------------------------------------------------------
// packed range: $bits(PARAM) - 32 + 85 + PARAM : 0
// ---------------------------------------------------------------------------

TEST_F(ExprReductionBitsTest, LogicTypespecHasOneRange) {
  const hldb::LogicTypespec *const lt = getBaseLogicTypespec();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  EXPECT_EQ(lt->getRanges()->size(), 1u);
}

TEST_F(ExprReductionBitsTest, RangeRightExprIsZero) {
  const hldb::Range *const r = getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(ExprReductionBitsTest, RangeLeftExprIsTopLevelAddOp) {
  const hldb::Range *const r = getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Operation *const top = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(top, nullptr) << "Sec 11.3.1: '+'/'-' are left-associative at the same precedence, so the outermost "
                              "node of '$bits(PARAM) - 32 + 85 + PARAM' should be a '+' Operation";
  EXPECT_EQ(top->getOpType(), vpiAddOp);
  ASSERT_NE(top->getOperands(), nullptr);
  ASSERT_EQ(top->getOperands()->size(), 2u);
}

TEST_F(ExprReductionBitsTest, TopAddOpRhsOperandIsParamRefObj) {
  const hldb::Range *const r = getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Operation *const top = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getOperands(), nullptr);
  ASSERT_EQ(top->getOperands()->size(), 2u);
  const hldb::RefObj *const paramRef = any_cast<hldb::RefObj>(top->getOperands()->at(1));
  ASSERT_NE(paramRef, nullptr) << "rightmost operand of the top-level '+' should be the bare 'PARAM' RefObj";
  EXPECT_EQ(paramRef->getName(), "PARAM");
  EXPECT_EQ(paramRef->getActual<hldb::Parameter>(), getParam());
}

TEST_F(ExprReductionBitsTest, InnermostOperationIsBitsMinus32) {
  const hldb::Range *const r = getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Operation *const top = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getOperands(), nullptr);
  ASSERT_GT(top->getOperands()->size(), 0u);
  const hldb::Operation *const mid = any_cast<hldb::Operation>(top->getOperands()->at(0));
  ASSERT_NE(mid, nullptr) << "'($bits(PARAM) - 32) + 85' should be a nested '+' Operation";
  EXPECT_EQ(mid->getOpType(), vpiAddOp);
  ASSERT_NE(mid->getOperands(), nullptr);
  ASSERT_GT(mid->getOperands()->size(), 0u);

  const hldb::Operation *const inner = any_cast<hldb::Operation>(mid->getOperands()->at(0));
  ASSERT_NE(inner, nullptr) << "'$bits(PARAM) - 32' should be a '-' Operation";
  EXPECT_EQ(inner->getOpType(), vpiSubOp);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_EQ(inner->getOperands()->size(), 2u);

  const hldb::SysFuncCall *const bits = any_cast<hldb::SysFuncCall>(inner->getOperands()->at(0));
  ASSERT_NE(bits, nullptr) << "Sec 20.6.2: '$bits(PARAM)' should be a SysFuncCall";
  EXPECT_EQ(bits->getName(), "$bits");
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "PARAM");
  EXPECT_EQ(arg->getActual<hldb::Parameter>(), getParam());
}

// ---------------------------------------------------------------------------
// enum constants
// ---------------------------------------------------------------------------

TEST_F(ExprReductionBitsTest, EnumHasTwoConsts) {
  const hldb::EnumTypespec *const et = getEnumTypespec();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 2u);

  const hldb::EnumConst *const item0 = et->getEnum()->getEnumConsts()->at(0);
  ASSERT_NE(item0, nullptr);
  EXPECT_EQ(item0->getName(), "LOGIC90PB_0_ITEM_0");
  const hldb::Constant *const item0Val = item0->getValue<hldb::Constant>();
  ASSERT_NE(item0Val, nullptr);
  EXPECT_EQ(item0Val->getDecompile(), "0");

  const hldb::EnumConst *const item1 = et->getEnum()->getEnumConsts()->at(1);
  ASSERT_NE(item1, nullptr);
  EXPECT_EQ(item1->getName(), "LOGIC90PB_0_ITEM_1");
  const hldb::Constant *const item1Val = item1->getValue<hldb::Constant>();
  ASSERT_NE(item1Val, nullptr);
  EXPECT_EQ(item1Val->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// compiler diagnostics
// ---------------------------------------------------------------------------

TEST_F(ExprReductionBitsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0)
      << "'PARAM' is declared, so the whole bit-width expression should evaluate without error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
