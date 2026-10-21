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

// Validates the HLDB model built for tests/ParamNoImport/dut.sv:
//
//   module test #( parameter PWR_AST_RSP = '0)();
//
//   endmodule
//
//   package pkg;
//   typedef struct packed {
//     logic [1:0] slow_clk_val;
//   } pwr_ast_rsp_t;
//   parameter pwr_ast_rsp_t PWR_AST_RSP_DEFAULT = '{
//     slow_clk_val: 2'b10
//   };
//   endpackage
//
//   module dut();
//   //import pkg::*;
//   test #(.PWR_AST_RSP(pkg::PWR_AST_RSP_DEFAULT)) t();
//   //test #(.PWR_AST_RSP(PWR_AST_RSP_DEFAULT)) t();
//   endmodule
//
// The point of the fixture is an override written as a package-scoped name
// (26.3) in a module that imports nothing: the import line is commented out.
// 26.3: "One way to use declarations made in a package is to reference them
// using the package scope resolution operator ::", and the import declaration
// is only "An alternate method", so no import is needed. The
// regression this file exists to catch is HLC resolving package items only
// through imports, and so failing to bind pkg::PWR_AST_RSP_DEFAULT, or
// pretending that dut imports pkg.
//
// Values: PWR_AST_RSP_DEFAULT = '{slow_clk_val: 2'b10} = 2'b10 = 2.
// PWR_AST_RSP in t is untyped, so 6.20.2 gives it "the type and range of the
// final value assigned to the parameter, after any value overrides have been
// applied", and its value is 2.
//
// What is checked, and why:
//   Module test and module dut
//     - each '()' declares one null port (37.14 detail 10, "module M();")
//   Module test
//     - exactly 1 parameter, PWR_AST_RSP, overridable (6.20.4), defaulting
//       to the literal '0
//   Package pkg
//     - pwr_ast_rsp_t is a packed structure (7.2.1) with exactly 1 member,
//       slow_clk_val, of type logic [1:0]
//     - exactly 1 parameter, PWR_AST_RSP_DEFAULT, local because it is in a
//       package (6.20.1), of type pwr_ast_rsp_t, whose value is Operation
//       vpiAssignmentPatternOp with one member-keyed item slow_clk_val: 2'b10
//       whose key is bound to the structure's member
//   Module dut
//     - records no import: the import line is a comment (5.4)
//     - t instantiates test and overrides PWR_AST_RSP with the scoped name
//       pkg::PWR_AST_RSP_DEFAULT, whose scope names pkg and whose item is
//       bound to pkg's Parameter (26.3)
//   Elaboration
//     - PWR_AST_RSP_DEFAULT is reduced to 2 on the elaborated pkg
//     - t's PWR_AST_RSP is 2
//     - dut is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced values are checked only when the
// design is elaborated, on Design::getTopPackages() and
// Design::getTopModules(); the unreduced values and bindings are asserted on
// the definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list '()' with no port at all, although 37.14 detail
// 10 names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The typespec HLC gives the untyped PWR_AST_RSP before elaboration is a
//     model convention; 6.20.2 fixes its type only from the final value.
//   - The radix HLC picks for a reduced value is a tool convention, so every
//     integer form is accepted.
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
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
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

class ParamNoImportTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamNoImport.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("pkg", m_design->getAllPackages()); }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
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

  static const hldb::Typedef *getRspT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("pwr_ast_rsp_t", pkg->getTypedefs());
  }

  // Whether 'rts' names pwr_ast_rsp_t.
  static bool namesRspT(const hldb::RefTypespec *rts) {
    const hldb::Typedef *const td = getRspT();
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr || rts->getActual() == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = rts->getActual<hldb::TypedefTypespec>();
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
           (rts->getActual() == td->getAlias()->getActual());
  }

  // The key a member-keyed pattern item writes. Whether HLC keeps the key as
  // a tag reference, as the item's name or as its typespec's name is a model
  // convention.
  static std::string_view getKey(const hldb::TaggedPattern *tp) {
    if (const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>()) return tag->getName();
    if (!tp->getName().empty()) return tp->getName();
    if (tp->getTypespec() != nullptr) return tp->getTypespec()->getName();
    return {};
  }

  // The override expression for PWR_AST_RSP that 't' writes.
  static const hldb::Any *getTOverride() {
    const hldb::Module *const dut = getModule("dut");
    if (dut == nullptr) return nullptr;
    const hldb::RefInstance *const t = hldb::findByName<hldb::RefInstance>("t", dut->getRefInstances());
    if (t == nullptr || t->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = t->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr || mt->getParamAssigns() == nullptr || mt->getParamAssigns()->size() != 1u) return nullptr;
    return mt->getParamAssigns()->at(0)->getRhs();
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

  // Verifies the reduced value of parameter 'name' in the elaborated scope
  // 'scope' is 'expected'.
  static void ExpectElabParam(const hldb::Scope *scope, std::string_view name, uint64_t expected,
                              std::string_view why) {
    ASSERT_NE(scope, nullptr);
    const hldb::ParamAssign *const pa = getParamAssign(scope, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "' in the elaborated scope";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << why;
  }
};

// ---------------------------------------------------------------------------
// module test and module dut ports
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamNoImportTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"test", "dut"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: " << name << "'s '()' declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: " << name << "'s '()' declares exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

// ---------------------------------------------------------------------------
// module test
// ---------------------------------------------------------------------------

TEST_F(ParamNoImportTest, TestDeclaresOverridablePwrAstRspDefaultingToZero) {
  const hldb::Module *const test = getModule("test");
  ASSERT_NE(test, nullptr) << "module 'test' not found";
  ASSERT_NE(test->getParameters(), nullptr);
  EXPECT_EQ(test->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(test, "PWR_AST_RSP");
  ASSERT_NE(p, nullptr) << "parameter 'PWR_AST_RSP' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.4: 'PWR_AST_RSP' is a parameter port, not a localparam";
  const hldb::ParamAssign *const pa = getParamAssign(test, "PWR_AST_RSP");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "'0") << "5.7.1: the unbased unsized literal '0";
}

// ---------------------------------------------------------------------------
// package pkg
// ---------------------------------------------------------------------------

TEST_F(ParamNoImportTest, PwrAstRspTIsPackedStructOfSlowClkVal) {
  const hldb::Typedef *const td = getRspT();
  ASSERT_NE(td, nullptr) << "typedef 'pwr_ast_rsp_t' not found in pkg";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: pwr_ast_rsp_t is declared 'packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 1u);
  const hldb::TypespecMember *const m = st->getStruct()->getMembers()->at(0);
  EXPECT_EQ(m->getName(), "slow_clk_val");
  ASSERT_NE(m->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'slow_clk_val' is declared 'logic [1:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ASSERT_NE(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
}

TEST_F(ParamNoImportTest, DefaultIsLocalPwrAstRspT) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(pkg, "PWR_AST_RSP_DEFAULT");
  ASSERT_NE(p, nullptr) << "parameter 'PWR_AST_RSP_DEFAULT' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.1: a param_assignment in a package becomes a localparam";
  EXPECT_TRUE(namesRspT(p->getTypespec())) << "'PWR_AST_RSP_DEFAULT' is declared 'pwr_ast_rsp_t'";
}

TEST_F(ParamNoImportTest, DefaultIsPatternOfSlowClkValTwoBOne0) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "PWR_AST_RSP_DEFAULT");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an assignment pattern";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 1u);
  const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(0));
  ASSERT_NE(item, nullptr) << "the item is keyed by a member name";
  EXPECT_EQ(getKey(item), "slow_clk_val");
  ASSERT_NE(item->getTag<hldb::RefObj>(), nullptr);
  const hldb::Typedef *const td = getRspT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_EQ(item->getTag<hldb::RefObj>()->getActual(),
            hldb::findByName<hldb::TypespecMember>("slow_clk_val", st->getStruct()->getMembers()))
      << "10.9.2: the key names pwr_ast_rsp_t's member slow_clk_val";
  const hldb::Constant *const value = item->getPattern<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "2'b10");
}

// ---------------------------------------------------------------------------
// module dut
// ---------------------------------------------------------------------------

TEST_F(ParamNoImportTest, DutRecordsNoImport) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr) << "module 'dut' not found";
  size_t imports = 0;
  if (dut->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *dut->getTypespecs()) {
      if (any_cast<hldb::ImportTypespec>(ts) != nullptr) ++imports;
    }
  }
  EXPECT_EQ(imports, 0u) << "5.4: '//import pkg::*;' is a one-line comment";
}

TEST_F(ParamNoImportTest, TOverridesWithPackageScopedDefault) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  const hldb::RefInstance *const t = hldb::findByName<hldb::RefInstance>("t", dut->getRefInstances());
  ASSERT_NE(t, nullptr) << "instance 't' not found";
  ASSERT_NE(t->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = t->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("test"));
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = mt->getParamAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "PWR_AST_RSP");
}

TEST_F(ParamNoImportTest, ScopedNameBindsToPkgParameter) {
  const hldb::RefObj *const path = any_cast<hldb::RefObj>(getTOverride());
  ASSERT_NE(path, nullptr) << "the override value is the name pkg::PWR_AST_RSP_DEFAULT";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package, then the item";
  const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "pkg");
  EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names package pkg";
  const hldb::RefObj *const item = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getName(), "PWR_AST_RSP_DEFAULT");
  ASSERT_NE(getParam(getPkg(), "PWR_AST_RSP_DEFAULT"), nullptr);
  EXPECT_EQ(item->getActual(), getParam(getPkg(), "PWR_AST_RSP_DEFAULT"))
      << "26.3: the item is declared in pkg, and no import is needed";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamNoImportTest, DefaultReducesToTwo) {
  if (m_design->getElaborated()) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>("pkg", m_design->getTopPackages());
    ExpectElabParam(pkg, "PWR_AST_RSP_DEFAULT", 2u, "10.9.2: '{slow_clk_val: 2'b10} = 2'b10");
  }
}

TEST_F(ParamNoImportTest, TReceivesTwo) {
  if (m_design->getElaborated()) {
    const hldb::Module *const dut = hldb::findByDefName<hldb::Module>("dut", m_design->getTopModules());
    ASSERT_NE(dut, nullptr) << "the elaborated dut not found";
    const hldb::Module *const t = hldb::findByName<hldb::Module>("t", dut->getModules());
    ASSERT_NE(t, nullptr) << "the instance t is elaborated";
    ExpectElabParam(t, "PWR_AST_RSP", 2u, "23.10.2.2: '.PWR_AST_RSP(pkg::PWR_AST_RSP_DEFAULT)' = 2'b10");
  }
}

TEST_F(ParamNoImportTest, DutIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: test is instantiated by dut";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "dut");
  }
}

TEST_F(ParamNoImportTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "26.3: a package-scoped name needs no import";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
