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

// Validates the HLDB model built for tests/ParamConstPush/dut.sv:
//
//   package base_pkg;
//   localparam logic [1:0]  BASE   = 2'b00;
//   localparam logic [1:0]  USAGE  = BASE | 2'b01;
//   endpackage
//
//   package error_pkg;
//   localparam logic [1:0]  FROM_USAGE = base_pkg::USAGE;
//   endpackage
//
//   module top import error_pkg::*; #() ();
//   endmodule
//
// The point of the fixture is a package localparam whose value is a
// package-scoped reference (IEEE 1800-2023 26.3) to a localparam of another
// package, itself computed from a third localparam, together with a module
// header that imports a package (26.4) and has an empty parameter port list
// '#()' (A.1.3). The regression this file exists to catch is HLC failing to
// push the constant value across packages, or rejecting the header.
//
// Values: BASE = 2'b00, USAGE = BASE | 2'b01 = 2'b01 = 1, and FROM_USAGE =
// base_pkg::USAGE = 1.
//
// What is checked, and why:
//   Package base_pkg
//     - exactly 2 parameters, BASE and USAGE, local (6.20.4) logic [1:0]
//     - BASE's value is the Constant "2'b00"; USAGE's unreduced value is
//       Operation vpiBitOrOp (11.4.8) over RefObj 'BASE', bound to the
//       package's Parameter, and Constant "2'b01"
//   Package error_pkg
//     - exactly 1 parameter, FROM_USAGE, local logic [1:0], whose unreduced
//       value is the package-scoped name base_pkg::USAGE: the scope names
//       base_pkg and the item is bound to base_pkg's Parameter USAGE (26.3)
//   Module top
//     - '()' declares one null port (37.14 detail 10, "module M();")
//     - '#()' is an empty parameter_port_list (A.1.3), so top has no
//       parameter
//     - records exactly 1 import, the wildcard import of error_pkg written in
//       the module header (26.4)
//   Elaboration
//     - top is the only module, so it is the only top-level instance
//       (23.3.1)
//     - on the elaborated packages, BASE is reduced to 0, USAGE to 1 and
//       FROM_USAGE to 1
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced values are checked only when the
// design is elaborated, on Design::getTopPackages(); the unreduced values are
// asserted on Design::getAllPackages() in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models 'module top ... ();' with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port. TopHasOneNullPort is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - Whether HLC records anything for the empty '#()' is a model
//     convention; only the absence of parameters is asserted.
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
#include <hldb/import_typespec.h>
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
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamConstPushTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamConstPush.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
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

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  // Verifies 'p' is a local parameter declared 'logic [1:0]'.
  static void ExpectLocalLogic1To0(const hldb::Parameter *p, std::string_view what) {
    ASSERT_NE(p, nullptr) << "parameter '" << what << "' not found";
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: '" << what << "' is declared 'localparam'";
    ASSERT_NE(p->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "'" << what << "' is declared 'logic [1:0]'";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->getDecompile(), "1");
    const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getDecompile(), "0");
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

  // Verifies the reduced value of parameter 'name' in the elaborated package
  // 'pkgName' is 'expected'.
  static void ExpectElabParam(std::string_view pkgName, std::string_view name, uint64_t expected,
                              std::string_view why) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>(pkgName, m_design->getTopPackages());
    ASSERT_NE(pkg, nullptr) << "the elaborated package '" << pkgName << "' not found";
    const hldb::ParamAssign *const pa = getParamAssign(pkg, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "' in the elaborated " << pkgName;
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << why;
  }
};

// ---------------------------------------------------------------------------
// packages base_pkg and error_pkg
// ---------------------------------------------------------------------------

TEST_F(ParamConstPushTest, BasePkgDeclaresBaseAndUsage) {
  const hldb::Package *const pkg = getPkg("base_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'base_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 2u);
  ExpectLocalLogic1To0(getParam(pkg, "BASE"), "BASE");
  ExpectLocalLogic1To0(getParam(pkg, "USAGE"), "USAGE");
  const hldb::ParamAssign *const base = getParamAssign(pkg, "BASE");
  ASSERT_NE(base, nullptr);
  ASSERT_NE(base->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(base->getRhs<hldb::Constant>()->getDecompile(), "2'b00");
}

TEST_F(ParamConstPushTest, UsageIsBaseOrOne) {
  const hldb::Package *const pkg = getPkg("base_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "USAGE");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "unreduced, 'BASE | 2'b01' is an Operation";
  EXPECT_EQ(op->getOpType(), vpiBitOrOp) << "11.4.8: a bitwise or";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const base = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "BASE");
  EXPECT_EQ(base->getActual(), getParam(pkg, "BASE")) << "'BASE' is base_pkg's own localparam";
  const hldb::Constant *const one = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "2'b01");
}

TEST_F(ParamConstPushTest, FromUsageIsScopedReferenceToUsage) {
  const hldb::Package *const pkg = getPkg("error_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'error_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  ExpectLocalLogic1To0(getParam(pkg, "FROM_USAGE"), "FROM_USAGE");
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "FROM_USAGE");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const path = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "unreduced, 'base_pkg::USAGE' is a reference";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package, then the item";
  const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "base_pkg");
  EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg("base_pkg")) << "26.3: the scope prefix names base_pkg";
  const hldb::RefObj *const item = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getName(), "USAGE");
  ASSERT_NE(getParam(getPkg("base_pkg"), "USAGE"), nullptr);
  EXPECT_EQ(item->getActual(), getParam(getPkg("base_pkg"), "USAGE")) << "26.3: 'USAGE' is declared in base_pkg";
}

// ---------------------------------------------------------------------------
// module top import error_pkg::*; #() ();
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamConstPushTest, TopHasOneNullPort) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(top->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  EXPECT_EQ(top->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(top->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
}

TEST_F(ParamConstPushTest, EmptyParameterPortListDeclaresNoParameter) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getParameters() == nullptr || top->getParameters()->empty())
      << "A.1.3: '#()' is an empty parameter_port_list";
  EXPECT_TRUE(top->getParamAssigns() == nullptr || top->getParamAssigns()->empty());
}

TEST_F(ParamConstPushTest, TopHeaderImportsAllOfErrorPkg) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  std::vector<const hldb::ImportTypespec *> imports;
  if (top->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *top->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "26.4: top's header writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "error_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamConstPushTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: top is the only module";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamConstPushTest, ValuesReduceAcrossPackages) {
  if (m_design->getElaborated()) {
    ExpectElabParam("base_pkg", "BASE", 0, "BASE = 2'b00");
    ExpectElabParam("base_pkg", "USAGE", 1, "11.4.8: 2'b00 | 2'b01 = 2'b01");
    ExpectElabParam("error_pkg", "FROM_USAGE", 1, "26.3: base_pkg::USAGE = 2'b01");
  }
}

TEST_F(ParamConstPushTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "A.1.3: '#()' is legal; 26.4: an import may appear in a module header";
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
