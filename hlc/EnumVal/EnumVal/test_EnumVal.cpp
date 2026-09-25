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

// Validates the UHDM graph for a typedef'd enum whose enumeration
// constants are each explicitly assigned a value built from a
// concatenation of sized literals:
//   module top;
//     typedef enum logic [5:0] {
//       EXC_CAUSE_IRQ_SOFTWARE_M     = {1'b1, 5'd03},
//       ...
//       EXC_CAUSE_ECALL_MMODE        = {1'b0, 5'd11}
//     } exc_cause_e;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.19 "Enumerations", p.119-120):
//   "enum_name_declaration ::= enum_identifier [ [ integer_atom_size ] ]
//   [ = constant_expr ]" -- the value after '=' is a constant_expression,
//   and Sec 11.4.12 ("Concatenations") allows a brace-delimited
//   concatenation of expressions to appear anywhere an expression may
//   appear, including here. "{1'b1, 5'd03}" is therefore a 2-operand
//   concatenation Operation (vpiConcatOp) used as an enum constant's
//   explicit value -- distinct from the plain sized-literal case covered
//   by other enum tests in this suite.
//
//   Also (6.19.1): "typedef enum logic [5:0] {...} exc_cause_e;" gives
//   the enum type a name so it can be used elsewhere -- a TypedefTypespec
//   named "exc_cause_e" wraps the EnumTypespec.
//
// Checked:
//   - design has module top
//   - module owns a TypedefTypespec "exc_cause_e" whose alias resolves to
//     an EnumTypespec with an explicit LogicTypespec base (logic[5:0])
//   - the EnumTypespec has 12 consts, in source order, by name
//   - the first const's value is a 2-operand Operation (vpiConcatOp)
//   - that concatenation's operands are Constants: a 1-bit binary literal
//     (vpiBinaryConst) and a 5-bit decimal literal (vpiDecConst)
//   - a later const (EXC_CAUSE_ECALL_MMODE) follows the same shape
//   - top has no processes (pure declaration)
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
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumValTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EnumVal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Enum *getEnum() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    const hldb::TypedefTypespec *const tt = hldb::findByName<hldb::TypedefTypespec>("exc_cause_e", top->getTypespecs());
    if (tt == nullptr) return nullptr;
    const hldb::Typedef *const td = tt->getTypedef();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const enumTs = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (enumTs == nullptr) return nullptr;
    return enumTs->getEnum();
  }
};

// ---------------------------------------------------------------------------
// Existence
// ---------------------------------------------------------------------------

TEST_F(EnumValTest, ModuleExists) { EXPECT_NE(getTop(), nullptr) << "module 'top' not found"; }

TEST_F(EnumValTest, TypedefExcCauseEExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const tt = hldb::findByName<hldb::TypedefTypespec>("exc_cause_e", top->getTypespecs());
  ASSERT_NE(tt, nullptr) << "module should own a TypedefTypespec named 'exc_cause_e'";
  EXPECT_EQ(tt->getName(), std::string_view("exc_cause_e"));
}

TEST_F(EnumValTest, EnumBaseTypeIsLogic) {
  const hldb::Enum *const e = getEnum();
  ASSERT_NE(e, nullptr);
  const hldb::RefTypespec *const base = e->getBaseTypespec();
  ASSERT_NE(base, nullptr) << "'enum logic [5:0]' should have an explicit base typespec";
  EXPECT_NE(base->getActual<hldb::LogicTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// 12 consts, in source order
// ---------------------------------------------------------------------------

TEST_F(EnumValTest, EnumHasTwelveConstsInOrder) {
  const hldb::Enum *const e = getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  const auto *const consts = e->getEnumConsts();
  ASSERT_EQ(consts->size(), 12u);
  static constexpr std::string_view kNames[12] = {
      "EXC_CAUSE_IRQ_SOFTWARE_M",     "EXC_CAUSE_IRQ_TIMER_M",     "EXC_CAUSE_IRQ_EXTERNAL_M",
      "EXC_CAUSE_IRQ_NM",             "EXC_CAUSE_INSN_ADDR_MISA",  "EXC_CAUSE_INSTR_ACCESS_FAULT",
      "EXC_CAUSE_ILLEGAL_INSN",       "EXC_CAUSE_BREAKPOINT",      "EXC_CAUSE_LOAD_ACCESS_FAULT",
      "EXC_CAUSE_STORE_ACCESS_FAULT", "EXC_CAUSE_ECALL_UMODE",     "EXC_CAUSE_ECALL_MMODE"};
  for (size_t i = 0; i < 12u; ++i) {
    EXPECT_EQ(consts->at(i)->getName(), kNames[i]) << "mismatch at index " << i;
  }
}

// ---------------------------------------------------------------------------
// First const: EXC_CAUSE_IRQ_SOFTWARE_M = {1'b1, 5'd03}
// ---------------------------------------------------------------------------

TEST_F(EnumValTest, FirstConstValueIsTwoOperandConcat) {
  const hldb::Enum *const e = getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 12u);
  const hldb::EnumConst *const first = e->getEnumConsts()->at(0);
  ASSERT_NE(first, nullptr);
  const hldb::Operation *const concat = first->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr) << "Sec 11.4.12: '{1'b1, 5'd03}' should be a concatenation Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  EXPECT_EQ(concat->getOperands()->size(), 2u);
}

TEST_F(EnumValTest, FirstConstConcatOperandsAreBinaryThenDecConstants) {
  const hldb::Enum *const e = getEnum();
  ASSERT_NE(e, nullptr);
  const hldb::EnumConst *const first = e->getEnumConsts()->at(0);
  ASSERT_NE(first, nullptr);
  const hldb::Operation *const concat = first->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u);

  const hldb::Constant *const bit = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  ASSERT_NE(bit, nullptr) << "first operand '1'b1' must be a Constant";
  EXPECT_EQ(bit->getConstType(), vpiBinaryConst);

  const hldb::Constant *const dec = any_cast<hldb::Constant>(concat->getOperands()->at(1));
  ASSERT_NE(dec, nullptr) << "second operand '5'd03' must be a Constant";
  EXPECT_EQ(dec->getConstType(), vpiDecConst);
}

// ---------------------------------------------------------------------------
// Last const: EXC_CAUSE_ECALL_MMODE = {1'b0, 5'd11} -- same shape
// ---------------------------------------------------------------------------

TEST_F(EnumValTest, LastConstValueIsTwoOperandConcatOfBinaryAndDec) {
  const hldb::Enum *const e = getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 12u);
  const hldb::EnumConst *const last = e->getEnumConsts()->at(11);
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(last->getName(), std::string_view("EXC_CAUSE_ECALL_MMODE"));

  const hldb::Operation *const concat = last->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u);

  const hldb::Constant *const bit = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  ASSERT_NE(bit, nullptr);
  EXPECT_EQ(bit->getConstType(), vpiBinaryConst);

  const hldb::Constant *const dec = any_cast<hldb::Constant>(concat->getOperands()->at(1));
  ASSERT_NE(dec, nullptr);
  EXPECT_EQ(dec->getConstType(), vpiDecConst);
}

// ---------------------------------------------------------------------------
// No processes (pure declaration)
// ---------------------------------------------------------------------------

TEST_F(EnumValTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(EnumValTest, CompilerReportsZeroErrors) {
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
