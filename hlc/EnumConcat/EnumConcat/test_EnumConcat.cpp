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

// Tests for tests/EnumConcat/dut.sv:
//
//   module GOOD();
//   endmodule
//
//   module dut ();
//   typedef enum logic [5:0] {
//     EXC_CAUSE_IRQ_SOFTWARE_M     = {1'b1, 5'd03}
//   } exc_cause_e;
//
//     if (EXC_CAUSE_IRQ_SOFTWARE_M == 6'b100011) begin
//       GOOD good();
//     end
//
//   endmodule
//
// The enum constant "EXC_CAUSE_IRQ_SOFTWARE_M" is defined via a
// concatenation (IEEE 1800-2023 Sec 11.4.12): "{1'b1, 5'd03}" packs a
// 1-bit value and a 5-bit value into the 6-bit value 6'b100011 (binary
// 1 concatenated with binary 00011 = 100011 = decimal 35). The
// unconditional generate-if (Sec 27.5 "Conditional generate constructs")
// then uses that enum constant directly (used inside a numerical/logical
// expression, per Sec 6.19.4 the enum value is used as its base-type
// value) in an equality comparison against the literal 6'b100011, so the
// condition should statically evaluate true and instantiate "GOOD good()".
// No :should_fail_because: tag.
//
// Checked:
//   - modules "GOOD" and "dut" both exist
//   - "dut" has 1 typespec: TypedefTypespec "exc_cause_e" -> EnumTypespec
//     with 1 EnumConst "EXC_CAUSE_IRQ_SOFTWARE_M" whose value is a
//     concatenation Operation (vpiConcatOp) of Constant 1'b1 and
//     Constant 5'd03
//   - "dut" has exactly 1 generate statement: a GenIf
//   - the GenIf's condition is an equality Operation (vpiEqOp) with 2
//     operands: a RefObj naming "EXC_CAUSE_IRQ_SOFTWARE_M" that resolves
//     to the EnumConst, and a Constant "6'b100011" (vpiBinaryConst, size 6)
//   - the GenIf's body is a Begin containing a RefInstance "good" whose
//     typespec resolves (ModuleTypespec) to "GOOD"
//   - compiler reports zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumConcatTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EnumConcat.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getGood() {
    return hldb::findByDefName<hldb::Module>("GOOD", m_design->getAllModules());
  }

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

  static const hldb::GenIf *getGenIf() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr || dut->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *dut->getGenStmts()) {
      if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(stmt)) return genIf;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

TEST_F(EnumConcatTest, ModuleGoodExists) { EXPECT_NE(getGood(), nullptr) << "module 'GOOD' not found"; }

TEST_F(EnumConcatTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr) << "module 'dut' not found"; }

// ---------------------------------------------------------------------------
// typedef enum logic [5:0] { EXC_CAUSE_IRQ_SOFTWARE_M = {1'b1, 5'd03} } exc_cause_e;
// ---------------------------------------------------------------------------

TEST_F(EnumConcatTest, EnumHasOneConstNamedSoftwareM) {
  const hldb::Enum *const e = getExcCauseEnum();
  ASSERT_NE(e, nullptr) << "EnumTypespec for 'exc_cause_e' not found";
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 1u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view("EXC_CAUSE_IRQ_SOFTWARE_M"));
}

TEST_F(EnumConcatTest, SoftwareMValueIsConcatOfOneAndThree) {
  const hldb::Enum *const e = getExcCauseEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 1u);
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

// ---------------------------------------------------------------------------
// if (EXC_CAUSE_IRQ_SOFTWARE_M == 6'b100011) begin GOOD good(); end
// ---------------------------------------------------------------------------

TEST_F(EnumConcatTest, DutHasExactlyOneGenIf) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getGenStmts(), nullptr);
  EXPECT_EQ(dut->getGenStmts()->size(), 1u);
  EXPECT_NE(getGenIf(), nullptr) << "the single generate statement must be a GenIf";
}

TEST_F(EnumConcatTest, GenIfConditionIsEqualityOfEnumConstAndBinaryConstant) {
  const hldb::GenIf *const genIf = getGenIf();
  ASSERT_NE(genIf, nullptr);
  const hldb::Operation *const eq = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr) << "'EXC_CAUSE_IRQ_SOFTWARE_M == 6'b100011' condition must be an equality Operation";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("EXC_CAUSE_IRQ_SOFTWARE_M"));
  EXPECT_NE(lhs->getActual(), nullptr) << "'EXC_CAUSE_IRQ_SOFTWARE_M' must resolve to the EnumConst declaration";

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(eq->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiBinaryConst);
  EXPECT_EQ(rhs->getSize(), 6);
  EXPECT_EQ(std::string(rhs->getValue()), "100011");
}

TEST_F(EnumConcatTest, GenIfBodyInstantiatesGoodAsGood) {
  const hldb::GenIf *const genIf = getGenIf();
  ASSERT_NE(genIf, nullptr);
  const hldb::Begin *const body = genIf->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "'begin GOOD good(); end' body must be a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  const hldb::RefInstance *good = nullptr;
  for (const hldb::Any *const stmt : *body->getStmts()) {
    if (const hldb::RefInstance *const ri = any_cast<hldb::RefInstance>(stmt)) {
      if (ri->getName() == "good") {
        good = ri;
        break;
      }
    }
  }
  ASSERT_NE(good, nullptr) << "'GOOD good();' RefInstance not found inside the generate-if body";
  ASSERT_NE(good->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = good->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "'good's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("GOOD"));
}

// ---------------------------------------------------------------------------
// Compiler diagnostics
// ---------------------------------------------------------------------------

TEST_F(EnumConcatTest, CompilerReportsZeroErrors) {
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
