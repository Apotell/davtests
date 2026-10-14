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

// Validates the HLDB model built for tests/ParamCast/dut.sv:
//
//   package  otp_ctrl_part_pkg;
//   typedef enum {
//       CreatorSwCfgIdx,
//       Blah,
//       Boo,
//       NumAgentsIdx
//     } part_idx_e;
//    parameter int NumAgents = int'(NumAgentsIdx);
//   endpackage
//
//   module otp_ctrl();
//     import otp_ctrl_part_pkg::*;
//     logic [NumAgents-1:0]    part_otp_arb_req;
//   endmodule
//
// The point of the fixture is a package parameter computed by casting an enum
// name to int (IEEE 1800-2023 6.24.1), then used through a wildcard import
// (26.3) to size a module variable. The enum names carry no explicit values.
// 6.19 says "If the first name is not assigned a value, it is given the
// initial value of 0" and "A name without a value is automatically assigned
// an increment of the value of the previous name", so NumAgentsIdx, the
// fourth name, is 3. The regression this file exists to catch is HLC
// mis-numbering the implicit enum values or losing the cast.
//
// What is checked, and why:
//   Package otp_ctrl_part_pkg
//     - exactly 1 Typedef, part_idx_e, an enum (6.19) with exactly 4 names in
//       source order: CreatorSwCfgIdx, Blah, Boo, NumAgentsIdx
//     - exactly 1 parameter, NumAgents, a local (6.20.4) signed int whose
//       unreduced value is Operation vpiCastOp (6.24.1) to int of RefObj
//       'NumAgentsIdx', bound to the EnumConst
//     - a parameter value is a constant expression (6.20.2), so on an
//       elaborated design NumAgents is reduced to 3
//   Module otp_ctrl
//     - '()' declares one null port (37.14 detail 10, "module M();")
//     - records exactly 1 import, the wildcard import of otp_ctrl_part_pkg
//     - exactly 1 variable, part_otp_arb_req, of logic with the single packed
//       range [NumAgents-1:0]: left bound Operation vpiSubOp over RefObj
//       'NumAgents', bound through the import to the package's Parameter, and
//       Constant "1"; right bound Constant "0"
//     - on an elaborated design that range is reduced to [2:0]
//   Elaboration (23.3.1)
//     - otp_ctrl appears in no instantiation, so it is the only top-level
//       instance
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: NumAgents and the variable's range are checked
// in both modes, unreduced on the definitions and reduced, only when the
// design is elaborated, on Design::getTopPackages() and
// Design::getTopModules().
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models 'module otp_ctrl();' with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port. OtpCtrlHasOneNullPort is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - Whether HLC materializes the implicit enum values (0 to 3) as value
//     expressions is a tool convention; their effect is asserted through the
//     reduced NumAgents.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

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
#include <hldb/import_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamCastTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamCast.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("otp_ctrl_part_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getOtpCtrl() {
    return hldb::findByDefName<hldb::Module>("otp_ctrl", m_design->getAllModules());
  }

  static const hldb::Parameter *getNumAgents() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("NumAgents", pkg->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Enum *getPartIdxEnum() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("part_idx_e", pkg->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (et == nullptr) return nullptr;
    return et->getEnum();
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal. The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted.
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if ((ch != '_') && (ch != ' ')) text.push_back(ch);
    }
    int base = 10;
    const std::string::size_type tick = text.find('\'');
    if (tick != std::string::npos) {
      std::string::size_type pos = tick + 1;
      if ((pos < text.size()) && ((text[pos] == 's') || (text[pos] == 'S'))) ++pos;
      if (pos >= text.size()) return false;
      switch (text[pos]) {
        case 'h':
        case 'H': base = 16; break;
        case 'd':
        case 'D': base = 10; break;
        case 'o':
        case 'O': base = 8; break;
        case 'b':
        case 'B': base = 2; break;
        default: return false;
      }
      text = text.substr(pos + 1);
    }
    if (text.empty()) return false;
    const char *const last = text.data() + text.size();
    const std::from_chars_result res = std::from_chars(text.data(), last, *value, base);
    return (res.ec == std::errc()) && (res.ptr == last);
  }

  // Verifies 'expr' is a Constant whose integer value is 'expected'.
  static void ExpectIntConstant(const hldb::Any *expr, uint64_t expected, std::string_view what) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(expr);
    ASSERT_NE(c, nullptr) << what << " should be a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << what;
  }
};

TEST_F(ParamCastTest, PartIdxEHasFourNamesInOrder) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'otp_ctrl_part_pkg' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u);
  const hldb::Enum *const e = getPartIdxEnum();
  ASSERT_NE(e, nullptr) << "6.19: typedef 'part_idx_e' should alias an enumerated type";
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 4u);
  const char *const names[] = {"CreatorSwCfgIdx", "Blah", "Boo", "NumAgentsIdx"};
  for (size_t i = 0; i < 4; ++i) {
    ASSERT_NE(e->getEnumConsts()->at(i), nullptr);
    EXPECT_EQ(e->getEnumConsts()->at(i)->getName(), names[i]) << "enum name " << i << ", in source order";
  }
}

TEST_F(ParamCastTest, NumAgentsIsLocalIntCastOfNumAgentsIdx) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getNumAgents();
  ASSERT_NE(p, nullptr) << "parameter 'NumAgents' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'NumAgents' is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11, Table 6-8: 'int' is a 32-bit signed integer";
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "NumAgents");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const cast = pa->getRhs<hldb::Operation>();
  ASSERT_NE(cast, nullptr) << "unreduced, the cast of NumAgentsIdx to int is an Operation";
  EXPECT_EQ(cast->getOpType(), vpiCastOp) << "6.24.1: a cast";
  ASSERT_NE(cast->getTypespec(), nullptr);
  EXPECT_NE(cast->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "6.24.1: the cast is to 'int'";
  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 1u);
  const hldb::RefObj *const idx = any_cast<hldb::RefObj>(cast->getOperands()->at(0));
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getName(), "NumAgentsIdx");
  const hldb::Enum *const e = getPartIdxEnum();
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(idx->getActual(), hldb::findByName<hldb::EnumConst>("NumAgentsIdx", e->getEnumConsts()))
      << "'NumAgentsIdx' is a name of part_idx_e";
}

TEST_F(ParamCastTest, NumAgentsReducesToThree) {
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const pa =
        getParamAssign(hldb::findByName<hldb::Package>("otp_ctrl_part_pkg", m_design->getTopPackages()), "NumAgents");
    ASSERT_NE(pa, nullptr) << "no ParamAssign for 'NumAgents' in the elaborated package";
    ExpectIntConstant(pa->getRhs(), 3, "6.19: NumAgentsIdx is the fourth name with implicit values, so it is 3");
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamCastTest, OtpCtrlHasOneNullPort) {
  const hldb::Module *const m = getOtpCtrl();
  ASSERT_NE(m, nullptr) << "module 'otp_ctrl' not found";
  ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
}

TEST_F(ParamCastTest, OtpCtrlImportsAllOfPartPkg) {
  const hldb::Module *const m = getOtpCtrl();
  ASSERT_NE(m, nullptr);
  std::vector<const hldb::ImportTypespec *> imports;
  if (m->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *m->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "otp_ctrl writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "otp_ctrl_part_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
}

TEST_F(ParamCastTest, PartOtpArbReqRangeUsesImportedNumAgents) {
  const hldb::Module *const m = getOtpCtrl();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  ASSERT_EQ(m->getVariables()->size(), 1u);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("part_otp_arb_req", m->getVariables());
  ASSERT_NE(v, nullptr) << "variable 'part_otp_arb_req' not found";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = v->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "declared 'logic [NumAgents-1:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Operation *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "unreduced, the left bound 'NumAgents-1' is an expression";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const num = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(num, nullptr);
  EXPECT_EQ(num->getName(), "NumAgents");
  ASSERT_NE(getNumAgents(), nullptr);
  EXPECT_EQ(num->getActual(), getNumAgents()) << "26.3: NumAgents is imported from otp_ctrl_part_pkg";
  const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(ParamCastTest, ElaboratedPartOtpArbReqIs2DownTo0) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'otp_ctrl' appears in no instantiation";
    const hldb::Module *const top = m_design->getTopModules()->at(0);
    ASSERT_NE(top, nullptr);
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>("part_otp_arb_req", top->getVariables());
    ASSERT_NE(v, nullptr) << "'part_otp_arb_req' not found in the elaborated otp_ctrl";
    ASSERT_NE(v->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = v->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    ExpectIntConstant(lt->getRanges()->at(0)->getLeftExpr(), 2, "NumAgents - 1 = 3 - 1 = 2");
    ExpectIntConstant(lt->getRanges()->at(0)->getRightExpr(), 0, "the right bound is 0");
  }
}

TEST_F(ParamCastTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
