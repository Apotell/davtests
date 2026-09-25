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

// Tests for tests/EnumConst/dut.sv:
//
//   module dut ();
//   typedef enum logic [5:0] {
//     EXC_CAUSE_IRQ_SOFTWARE_M     = {1'b1, 5'd03},
//     EXC_CAUSE_IRQ_TIMER_M        = {1'b1, 5'd07}
//   } exc_cause_e;
//   endmodule
//
// IEEE 1800-2023 Sec 6.19 "Enumerations": an enum_base_type may be an
// explicit packed type ("logic [5:0]" here, a 6-bit packed vector); each
// enum_name_declaration may assign an explicit constant_expression as its
// value. Sec 11.4.12 "Concatenations": "{1'b1, 5'd03}" concatenates a
// 1-bit and a 5-bit operand into a 6-bit value, matching the enum's
// declared base width. No :should_fail_because: tag -- this is purely a
// legal, static enum declaration (no instances, no procedural code).
//
// Checked:
//   - module "dut" exists
//   - module has 1 typespec: TypedefTypespec "exc_cause_e"
//   - the typedef's alias resolves to an EnumTypespec
//   - the enum's explicit base typespec resolves to LogicTypespec with a
//     single range [5:0]
//   - the enum has 2 EnumConsts: EXC_CAUSE_IRQ_SOFTWARE_M, EXC_CAUSE_IRQ_TIMER_M
//   - each EnumConst's value is a concatenation Operation (vpiConcatOp)
//     with exactly 2 operands
//   - EXC_CAUSE_IRQ_SOFTWARE_M's operands are Constant 1'b1 (vpiBinaryConst,
//     size 1, value "1") and Constant 5'd03 (vpiDecConst, size 5, value "3")
//   - EXC_CAUSE_IRQ_TIMER_M's operands are Constant 1'b1 (vpiBinaryConst,
//     size 1, value "1") and Constant 5'd07 (vpiDecConst, size 5, value "7")
//   - module has no processes and no generate statements
//   - compiler reports zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumConstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EnumConst.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Enum *getExcCauseEnum() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr || dut->getTypespecs() == nullptr) return nullptr;
    const hldb::TypedefTypespec *const tt =
        hldb::findByName<hldb::TypedefTypespec>("exc_cause_e", dut->getTypespecs());
    if (tt == nullptr) return nullptr;
    const hldb::Typedef *const td = tt->getTypedef();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const enumTs = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (enumTs == nullptr) return nullptr;
    return enumTs->getEnum();
  }
};

// ---------------------------------------------------------------------------
// Module and typedef existence
// ---------------------------------------------------------------------------

TEST_F(EnumConstTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr) << "module 'dut' not found"; }

TEST_F(EnumConstTest, DutHasOneTypespec) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getTypespecs(), nullptr);
  EXPECT_GT(dut->getTypespecs()->size(), 1u);
}

TEST_F(EnumConstTest, ExcCauseETypedefResolvesToEnumTypespec) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::TypedefTypespec *const tt =
      hldb::findByName<hldb::TypedefTypespec>("exc_cause_e", dut->getTypespecs());
  ASSERT_NE(tt, nullptr) << "TypedefTypespec 'exc_cause_e' not found";
  const hldb::Typedef *const td = tt->getTypedef();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  EXPECT_NE(td->getAlias()->getActual<hldb::EnumTypespec>(), nullptr)
      << "'typedef enum ... exc_cause_e' alias must resolve to EnumTypespec";
}

// ---------------------------------------------------------------------------
// Explicit base type: logic [5:0]
// ---------------------------------------------------------------------------

TEST_F(EnumConstTest, EnumBaseTypespecIsSixBitLogic) {
  const hldb::Enum *const e = getExcCauseEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getBaseTypespec(), nullptr) << "'enum logic [5:0] {...}' must carry an explicit base typespec";
  const hldb::LogicTypespec *const logicTs = e->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicTs, nullptr) << "explicit base type 'logic [5:0]' must resolve to LogicTypespec";
  ASSERT_NE(logicTs->getRanges(), nullptr);
  ASSERT_EQ(logicTs->getRanges()->size(), 1u);
  const hldb::Range *const range = logicTs->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = any_cast<hldb::Constant>(range->getLeftExpr());
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(std::string(left->getDecompile()), "5");
  const hldb::Constant *const right = any_cast<hldb::Constant>(range->getRightExpr());
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(std::string(right->getDecompile()), "0");
}

// ---------------------------------------------------------------------------
// Two enum consts
// ---------------------------------------------------------------------------

TEST_F(EnumConstTest, EnumHasTwoConsts) {
  const hldb::Enum *const e = getExcCauseEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view("EXC_CAUSE_IRQ_SOFTWARE_M"));
  EXPECT_EQ(e->getEnumConsts()->at(1)->getName(), std::string_view("EXC_CAUSE_IRQ_TIMER_M"));
}

TEST_F(EnumConstTest, SoftwareMValueIsConcatOfOneAndThree) {
  const hldb::Enum *const e = getExcCauseEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  const hldb::EnumConst *const ec = e->getEnumConsts()->at(0);
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const concat = ec->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr) << "Sec 11.4.12: '{1'b1, 5'd03}' value must be a concatenation Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u);

  const hldb::Constant *const bit1 = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  ASSERT_NE(bit1, nullptr);
  EXPECT_EQ(bit1->getConstType(), vpiBinaryConst);
  EXPECT_EQ(bit1->getSize(), 1);
  EXPECT_EQ(std::string(bit1->getValue()), "1");

  const hldb::Constant *const dec3 = any_cast<hldb::Constant>(concat->getOperands()->at(1));
  ASSERT_NE(dec3, nullptr);
  EXPECT_EQ(dec3->getConstType(), vpiDecConst);
  EXPECT_EQ(dec3->getSize(), 5);
  EXPECT_EQ(std::string(dec3->getValue()), "3");
}

TEST_F(EnumConstTest, TimerMValueIsConcatOfOneAndSeven) {
  const hldb::Enum *const e = getExcCauseEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  const hldb::EnumConst *const ec = e->getEnumConsts()->at(1);
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const concat = ec->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr) << "Sec 11.4.12: '{1'b1, 5'd07}' value must be a concatenation Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u);

  const hldb::Constant *const bit1 = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  ASSERT_NE(bit1, nullptr);
  EXPECT_EQ(bit1->getConstType(), vpiBinaryConst);
  EXPECT_EQ(bit1->getSize(), 1);
  EXPECT_EQ(std::string(bit1->getValue()), "1");

  const hldb::Constant *const dec7 = any_cast<hldb::Constant>(concat->getOperands()->at(1));
  ASSERT_NE(dec7, nullptr);
  EXPECT_EQ(dec7->getConstType(), vpiDecConst);
  EXPECT_EQ(dec7->getSize(), 5);
  EXPECT_EQ(std::string(dec7->getValue()), "7");
}

// ---------------------------------------------------------------------------
// No procedural code, no generate statements
// ---------------------------------------------------------------------------

TEST_F(EnumConstTest, NoProcesses) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  EXPECT_TRUE(dut->getProcesses() == nullptr || dut->getProcesses()->empty());
}

TEST_F(EnumConstTest, NoGenerateStatements) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  EXPECT_TRUE(dut->getGenStmts() == nullptr || dut->getGenStmts()->empty());
}

// ---------------------------------------------------------------------------
// Compiler diagnostics
// ---------------------------------------------------------------------------

TEST_F(EnumConstTest, CompilerReportsZeroErrors) {
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
