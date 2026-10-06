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

// Validates the HLDB model built for tests/PackageType/dut.sv:
//
//   package bp_common_aviary_pkg;
//   typedef struct packed
//   {
//     integer num_core;
//     integer num_cce;
//   }  bp_proc_param_s;
//     // Suitably high enough to not run out of configs.
//     localparam max_cfgs    = 128;
//     localparam lg_max_cfgs = $clog2(max_cfgs);
//     localparam bp_proc_param_s bp_inv_cfg_p =
//       '{default: "inv"};
//     /* verilator lint_off WIDTH */
//     parameter bp_proc_param_s [max_cfgs-1:0] all_cfgs_gp =
//     {
//      bp_inv_cfg_p
//     };
//     /* verilator lint_on WIDTH */
//   endpackage
//
// The point of the fixture is package parameters whose type is a package
// typedef of a packed structure: one of the structure type itself, set with
// a 'default:' assignment pattern, and one of a packed array of that
// structure whose dimension depends on another parameter. The regression
// this file exists to catch is HLC failing to resolve the typedef as a
// parameter type, or mis-evaluating the assignment pattern.
//
// What is checked, and why:
//   typedef struct packed { ... } bp_proc_param_s; (6.18, 7.2.1)
//     - the package declares exactly 1 Typedef, 'bp_proc_param_s', whose
//       alias is a StructTypespec
//     - the Struct is packed, with exactly 2 members in source order,
//       num_core then num_cce, each an 'integer': an IntegerTypespec that
//       is signed (6.11)
//   Parameters (6.20)
//     - the package declares exactly 4: max_cfgs, lg_max_cfgs,
//       bp_inv_cfg_p and all_cfgs_gp. All are local parameters: three are
//       declared 'localparam', and in a package the keyword 'parameter' is a
//       synonym for 'localparam' (6.20.4)
//     - max_cfgs: ParamAssign RHS Constant "128"
//     - lg_max_cfgs: unreduced, the SysFuncCall "$clog2" (20.8.1) with
//       exactly 1 argument, RefObj 'max_cfgs' bound to that Parameter. A
//       parameter value is a constant expression (6.20.2), so on an
//       elaborated design it is reduced to $clog2(128) = 7
//     - bp_inv_cfg_p: typed by bp_proc_param_s. Unreduced, its value is an
//       Operation vpiAssignmentPatternOp (10.9.2) with exactly 1 operand,
//       the 'default:' pattern whose value is the string literal "inv"
//       (vpiStringConst, 5.9). On an elaborated design it is reduced to
//       64'h00696E76_00696E76: the default value is assigned to every member
//       (10.9.2), the 24-bit string "inv" is right-justified and zero-padded
//       into each 32-bit integer member (5.9), and num_core, the first
//       member, is the most significant (7.2.1)
//     - all_cfgs_gp: typed as a packed array (7.4.1) with exactly 1 packed
//       dimension [max_cfgs-1:0] -- left bound Operation vpiSubOp over
//       RefObj 'max_cfgs' and Constant "1", right bound Constant "0" --
//       whose element type is bp_proc_param_s. Unreduced, its value is an
//       Operation vpiConcatOp (11.4.12) with exactly 1 operand, RefObj
//       'bp_inv_cfg_p' bound to that Parameter
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics. Assigning
//       a string literal to an integral member (5.9) and a narrower value to
//       a wider parameter (10.7) are both legal
//
// Reduction and elaboration: lg_max_cfgs and bp_inv_cfg_p are checked in
// both modes. The unreduced form is asserted on the package definition in
// Design::getAllPackages(), which holds the source form in every run; the
// reduced value is asserted, only when the design is elaborated, on the
// elaborated package in Design::getTopPackages().
//
// What is NOT checked, and why:
//   - The reduced value of all_cfgs_gp. It is 128 x 64 = 8192 bits wide,
//     with bp_inv_cfg_p in the low 64 bits and zeros above (10.7). How HLC
//     records a constant of that width is a tool convention, so only its
//     unreduced form is asserted.
//   - The type of the untyped max_cfgs and lg_max_cfgs: a parameter with no
//     data type takes the type of its final value (6.20.2); whether HLC
//     records a typespec for it is a tool convention.
//   - How a parameter's type refers to bp_proc_param_s: the RefTypespec may
//     resolve to the TypedefTypespec or to the StructTypespec it aliases.
//     Both are that type (6.18), so either is accepted.
//   - How HLC labels the 'default:' key of the assignment pattern is a tool
//     convention; the pattern's single operand and its value are asserted.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention, so the package count is
//     not asserted; the package is looked up by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The comments, including the 'verilator lint_off/lint_on' block
//     comments, are not design objects (5.4).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/integer_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {

constexpr std::string_view kPkgName = "bp_common_aviary_pkg";

class PackageTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageType.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The package definition, which holds the source form of every value.
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>(kPkgName, m_design->getAllPackages()); }

  // The elaborated package, which holds the reduced values.
  static const hldb::Package *getTopPkg() {
    return hldb::findByName<hldb::Package>(kPkgName, m_design->getTopPackages());
  }

  static const hldb::Parameter *getParam(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
  }

  // The ParamAssign in 'pkg' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Package *pkg, std::string_view name) {
    if (pkg == nullptr || pkg->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *pkg->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Typedef *getProcParamS() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("bp_proc_param_s", pkg->getTypedefs());
  }

  static const hldb::Struct *getProcParamStruct() {
    const hldb::Typedef *const td = getProcParamS();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr) return nullptr;
    return st->getStruct();
  }

  // Whether 'rts' names the type bp_proc_param_s: it may resolve to the
  // TypedefTypespec or to the StructTypespec the typedef aliases (6.18).
  static bool isProcParamS(const hldb::RefTypespec *rts) {
    const hldb::Typedef *const td = getProcParamS();
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr) return false;
    const hldb::Typespec *const actual = rts->getActual();
    if (actual == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual());
  }

  // Integer value of a Constant, from its decompiled text ("7") or from a
  // sized or based literal ("32'd7", "64'h..."). The radix HLC picks for a
  // folded value is a tool convention, so every integer form is accepted and
  // the numeric value is what gets compared.
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if (ch != '_') text.push_back(ch);
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
    ASSERT_NE(c, nullptr) << what << " should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << what;
  }
};

// ---------------------------------------------------------------------------
// typedef struct packed { integer num_core; integer num_cce; } bp_proc_param_s;
// ---------------------------------------------------------------------------

TEST_F(PackageTypeTest, PackageDeclaresOneTypedefProcParamS) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'bp_common_aviary_pkg' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'bp_proc_param_s' is the package's only typedef";
  const hldb::Typedef *const td = getProcParamS();
  ASSERT_NE(td, nullptr) << "typedef 'bp_proc_param_s' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  EXPECT_NE(td->getAlias()->getActual<hldb::StructTypespec>(), nullptr)
      << "6.18: 'bp_proc_param_s' names a structure type";
}

TEST_F(PackageTypeTest, ProcParamSIsPackedStructOfTwoIntegers) {
  const hldb::Struct *const s = getProcParamStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 2u) << "'num_core' and 'num_cce'";
  const char *const names[] = {"num_core", "num_cce"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TypespecMember *const m = s->getMembers()->at(i);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getName(), names[i]) << "member " << i << ", in source order";
    ASSERT_NE(m->getTypespec(), nullptr);
    const hldb::IntegerTypespec *const it = m->getTypespec()->getActual<hldb::IntegerTypespec>();
    ASSERT_NE(it, nullptr) << "'" << names[i] << "' is declared 'integer'";
    EXPECT_TRUE(it->getSigned()) << "6.11: 'integer' is signed";
  }
}

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

TEST_F(PackageTypeTest, PackageDeclaresExactlyFourParameters) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 4u);
  for (std::string_view name : {"max_cfgs", "lg_max_cfgs", "bp_inv_cfg_p", "all_cfgs_gp"}) {
    EXPECT_NE(getParam(name), nullptr) << "parameter '" << name << "' not found";
  }
}

TEST_F(PackageTypeTest, AllParametersAreLocalParams) {
  for (std::string_view name : {"max_cfgs", "lg_max_cfgs", "bp_inv_cfg_p", "all_cfgs_gp"}) {
    const hldb::Parameter *const p = getParam(name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: '" << name << "' is a local parameter of the package";
  }
}

TEST_F(PackageTypeTest, MaxCfgsIs128) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "max_cfgs");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'max_cfgs'";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'max_cfgs' is assigned a literal";
  EXPECT_EQ(rhs->getDecompile(), "128");
}

TEST_F(PackageTypeTest, LgMaxCfgsIsClog2OfMaxCfgs) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "lg_max_cfgs");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'lg_max_cfgs'";
  const hldb::SysFuncCall *const clog2 = pa->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(clog2, nullptr) << "unreduced, '$clog2(max_cfgs)' is a system function call";
  EXPECT_EQ(clog2->getName(), "$clog2");
  ASSERT_NE(clog2->getArguments(), nullptr);
  ASSERT_EQ(clog2->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(clog2->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "max_cfgs");
  ASSERT_NE(getParam("max_cfgs"), nullptr);
  EXPECT_EQ(arg->getActual(), getParam("max_cfgs"));
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const elab = getParamAssign(getTopPkg(), "lg_max_cfgs");
    ASSERT_NE(elab, nullptr) << "no ParamAssign for 'lg_max_cfgs' in the elaborated package";
    ExpectIntConstant(elab->getRhs(), 7, "6.20.2, 20.8.1: $clog2(128) = 7");
  }
}

TEST_F(PackageTypeTest, BpInvCfgPIsTypedByProcParamS) {
  const hldb::Parameter *const p = getParam("bp_inv_cfg_p");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(isProcParamS(p->getTypespec())) << "6.18: 'bp_inv_cfg_p' is declared with the type bp_proc_param_s";
}

TEST_F(PackageTypeTest, BpInvCfgPIsDefaultPatternOfInv) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "bp_inv_cfg_p");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'bp_inv_cfg_p'";
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, '{default: \"inv\"} is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2: an assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 1u) << "the pattern has the single item 'default: \"inv\"'";
  const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(0));
  ASSERT_NE(item, nullptr) << "a 'key: value' pattern item";
  const hldb::Constant *const value = item->getPattern<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "the default value is a literal";
  EXPECT_EQ(value->getDecompile(), "\"inv\"");
  EXPECT_EQ(value->getConstType(), vpiStringConst) << "5.9: \"inv\" is a string literal";
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const elab = getParamAssign(getTopPkg(), "bp_inv_cfg_p");
    ASSERT_NE(elab, nullptr) << "no ParamAssign for 'bp_inv_cfg_p' in the elaborated package";
    ExpectIntConstant(elab->getRhs(), 0x00696E7600696E76ULL,
                      "10.9.2, 5.9, 7.2.1: each integer member holds 32'h00696E76 (\"inv\"), num_core first");
  }
}

TEST_F(PackageTypeTest, AllCfgsGpIsPackedArrayOfProcParamS) {
  const hldb::Parameter *const p = getParam("all_cfgs_gp");
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = p->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'bp_proc_param_s [max_cfgs-1:0]' is an array type";
  EXPECT_TRUE(at->getPacked()) << "7.4.1: '[max_cfgs-1:0]' is written before the name, so it is packed";
  const hldb::Range *const r = at->getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Operation *const left = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "the left bound 'max_cfgs-1' is an expression";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const maxCfgs = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(maxCfgs, nullptr);
  EXPECT_EQ(maxCfgs->getName(), "max_cfgs");
  EXPECT_EQ(maxCfgs->getActual(), getParam("max_cfgs"));
  const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
  EXPECT_TRUE(isProcParamS(at->getElemTypespec())) << "7.4.1: the element type is bp_proc_param_s";
}

TEST_F(PackageTypeTest, AllCfgsGpIsConcatOfBpInvCfgP) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "all_cfgs_gp");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'all_cfgs_gp'";
  const hldb::Operation *const concat = pa->getRhs<hldb::Operation>();
  ASSERT_NE(concat, nullptr) << "unreduced, '{ bp_inv_cfg_p }' is an Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "11.4.12: a concatenation";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 1u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(concat->getOperands()->at(0));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "bp_inv_cfg_p");
  ASSERT_NE(getParam("bp_inv_cfg_p"), nullptr);
  EXPECT_EQ(ref->getActual(), getParam("bp_inv_cfg_p"));
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageTypeTest, NoFatalSyntaxOrErrorDiagnostics) {
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
