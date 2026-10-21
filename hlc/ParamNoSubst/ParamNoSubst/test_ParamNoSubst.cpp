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

// Validates the HLDB model built for tests/ParamNoSubst/dut.sv:
//
//   package aes_pkg;
//   typedef struct packed {
//     logic[31:0] manual_operation;
//   } ctrl_reg_t;
//
//   parameter ctrl_reg_t CTRL_RESET = '{
//     manual_operation: '0
//   };
//   endpackage
//
//   module prim_subreg #(
//     parameter logic [31:0] RESVAL = '0
//   ) ();
//   endmodule
//
//   module prim_subreg_shadow #(
//     parameter logic [31:0] RESVAL = '0
//   ) ();
//     prim_subreg #(.RESVAL(RESVAL)) committed_reg();
//   endmodule
//
//   module aes_core;
//     import aes_pkg::*;
//     ctrl_reg_t ctrl_q;
//     prim_subreg_shadow #(.RESVAL(CTRL_RESET)) ctrl_shadowed_reg();
//   endmodule // aes_core
//
// The point of the fixture is an override '.RESVAL(RESVAL)' whose value has
// the same name as the parameter it overrides. The instantiation is written
// in prim_subreg_shadow, and a module is a scope (23.9), so the name inside
// the parentheses refers to prim_subreg_shadow's own RESVAL, which in turn receives a
// packed structure imported from a package. The regression this file exists
// to catch is HLC binding the inner RESVAL to prim_subreg's parameter (a
// self-reference), or substituting the default '0 instead of the passed
// value.
//
// Values: CTRL_RESET = '{manual_operation: '0} = 32'h0. ctrl_shadowed_reg's
// RESVAL = CTRL_RESET = 0, and committed_reg's RESVAL = 0. A packed structure
// is integral (6.11.1), so it is assignment compatible with logic [31:0]
// (6.22.3).
//
// What is checked, and why:
//   Package aes_pkg
//     - ctrl_reg_t is a packed structure (7.2.1) with exactly 1 member,
//       manual_operation, of type logic [31:0]
//     - CTRL_RESET is local because it is in a package (6.20.1), of type
//       ctrl_reg_t, and its value is an assignment pattern with one
//       member-keyed item manual_operation: '0 (10.9.2)
//       whose key is bound to the structure's member
//   Modules prim_subreg and prim_subreg_shadow
//     - each '()' declares one null port (37.14 detail 10, "module M();")
//     - each declares exactly 1 parameter, RESVAL, overridable (6.20.4), of
//       type logic [31:0], defaulting to '0
//     - committed_reg instantiates prim_subreg and overrides RESVAL with the
//       RefObj RESVAL bound to prim_subreg_shadow's own Parameter
//   Module aes_core
//     - has no port
//     - records exactly 1 import, the wildcard import of aes_pkg (26.3)
//     - ctrl_q is a variable of the imported type ctrl_reg_t
//     - ctrl_shadowed_reg instantiates prim_subreg_shadow and overrides
//       RESVAL with the RefObj CTRL_RESET, bound through the import to
//       aes_pkg's Parameter
//   Elaboration
//     - both elaborated RESVAL parameters hold 0
//     - aes_core is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced values are checked only when the
// design is elaborated, on Design::getTopModules(); the unreduced values and
// bindings are asserted on the definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list '()' with no port at all, although 37.14 detail
// 10 names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
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
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamNoSubstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamNoSubst.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("aes_pkg", m_design->getAllPackages());
  }

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

  static const hldb::Typedef *getCtrlRegT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("ctrl_reg_t", pkg->getTypedefs());
  }

  // Whether 'rts' names ctrl_reg_t.
  static bool namesCtrlRegT(const hldb::RefTypespec *rts) {
    const hldb::Typedef *const td = getCtrlRegT();
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

  // The single override the unelaborated instance 'inst' of module 'module'
  // writes, after verifying it instantiates 'target' and names RESVAL.
  static const hldb::Any *getResvalOverride(std::string_view module, std::string_view inst, std::string_view target) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    const hldb::RefInstance *const ri = hldb::findByName<hldb::RefInstance>(inst, m->getRefInstances());
    if (ri == nullptr || ri->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = ri->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr || mt->getModule() != getModule(target)) return nullptr;
    if (mt->getParamAssigns() == nullptr || mt->getParamAssigns()->size() != 1u) return nullptr;
    const hldb::RefObj *const lhs = mt->getParamAssigns()->at(0)->getLhs<hldb::RefObj>();
    if (lhs == nullptr || lhs->getName() != "RESVAL") return nullptr;
    return mt->getParamAssigns()->at(0)->getRhs();
  }

  // Verifies module 'name' declares exactly 1 overridable logic [31:0]
  // RESVAL defaulting to '0.
  static void ExpectResvalParameter(std::string_view name) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getParameters(), nullptr);
    ASSERT_EQ(m->getParameters()->size(), 1u) << name;
    const hldb::Parameter *const p = getParam(m, "RESVAL");
    ASSERT_NE(p, nullptr) << name;
    EXPECT_FALSE(p->getLocalParam()) << "6.20.4: " << name << "'s RESVAL is a parameter port";
    ASSERT_NE(p->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << name << "'s RESVAL is declared 'logic [31:0]'";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    ASSERT_NE(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>(), nullptr);
    EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "31");
    ASSERT_NE(lt->getRanges()->at(0)->getRightExpr<hldb::Constant>(), nullptr);
    EXPECT_EQ(lt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
    const hldb::ParamAssign *const pa = getParamAssign(m, "RESVAL");
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
    EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "'0") << "5.7.1: the unbased unsized literal '0";
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
// package aes_pkg
// ---------------------------------------------------------------------------

TEST_F(ParamNoSubstTest, CtrlRegTIsPackedStructOfManualOperation) {
  const hldb::Typedef *const td = getCtrlRegT();
  ASSERT_NE(td, nullptr) << "typedef 'ctrl_reg_t' not found in aes_pkg";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: ctrl_reg_t is declared 'packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 1u);
  const hldb::TypespecMember *const m = st->getStruct()->getMembers()->at(0);
  EXPECT_EQ(m->getName(), "manual_operation");
  ASSERT_NE(m->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'manual_operation' is declared 'logic[31:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ASSERT_NE(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "31");
}

TEST_F(ParamNoSubstTest, CtrlResetIsLocalPatternOfZero) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'aes_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(pkg, "CTRL_RESET");
  ASSERT_NE(p, nullptr) << "parameter 'CTRL_RESET' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.1: a param_assignment in a package becomes a localparam";
  EXPECT_TRUE(namesCtrlRegT(p->getTypespec())) << "'CTRL_RESET' is declared 'ctrl_reg_t'";
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "CTRL_RESET");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an assignment pattern";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 1u);
  const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(0));
  ASSERT_NE(item, nullptr) << "the item is keyed by a member name";
  EXPECT_EQ(getKey(item), "manual_operation");
  ASSERT_NE(item->getTag<hldb::RefObj>(), nullptr);
  const hldb::Typedef *const td = getCtrlRegT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_EQ(item->getTag<hldb::RefObj>()->getActual(),
            hldb::findByName<hldb::TypespecMember>("manual_operation", st->getStruct()->getMembers()))
      << "10.9.2: the key names ctrl_reg_t's member manual_operation";
  ASSERT_NE(item->getPattern<hldb::Constant>(), nullptr);
  EXPECT_EQ(item->getPattern<hldb::Constant>()->getDecompile(), "'0");
}

// ---------------------------------------------------------------------------
// modules prim_subreg and prim_subreg_shadow
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamNoSubstTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"prim_subreg", "prim_subreg_shadow"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: " << name << "'s '()' declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: " << name << "'s '()' declares exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamNoSubstTest, SubregsDeclareOverridableResvalDefaultingToZero) {
  ExpectResvalParameter("prim_subreg");
  ExpectResvalParameter("prim_subreg_shadow");
}

TEST_F(ParamNoSubstTest, ShadowPassesItsOwnResvalToCommittedReg) {
  const hldb::RefObj *const resval =
      any_cast<hldb::RefObj>(getResvalOverride("prim_subreg_shadow", "committed_reg", "prim_subreg"));
  ASSERT_NE(resval, nullptr) << "committed_reg instantiates prim_subreg with '.RESVAL(RESVAL)'";
  EXPECT_EQ(resval->getName(), "RESVAL");
  ASSERT_NE(getParam(getModule("prim_subreg_shadow"), "RESVAL"), nullptr);
  EXPECT_EQ(resval->getActual(), getParam(getModule("prim_subreg_shadow"), "RESVAL"))
      << "23.9: the override is written in prim_subreg_shadow's scope, so 'RESVAL' is its own parameter";
}

// ---------------------------------------------------------------------------
// module aes_core
// ---------------------------------------------------------------------------

TEST_F(ParamNoSubstTest, AesCoreHasNoPort) {
  const hldb::Module *const m = getModule("aes_core");
  ASSERT_NE(m, nullptr) << "module 'aes_core' not found";
  EXPECT_TRUE(m->getPorts() == nullptr || m->getPorts()->empty())
      << "A.1.2: 'module aes_core;' writes no list_of_port_declarations";
}

TEST_F(ParamNoSubstTest, AesCoreImportsAllOfAesPkg) {
  const hldb::Module *const m = getModule("aes_core");
  ASSERT_NE(m, nullptr);
  std::vector<const hldb::ImportTypespec *> imports;
  if (m->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *m->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "aes_core writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "aes_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
}

TEST_F(ParamNoSubstTest, CtrlQIsImportedCtrlRegT) {
  const hldb::Module *const m = getModule("aes_core");
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("ctrl_q", m->getVariables());
  ASSERT_NE(v, nullptr) << "variable 'ctrl_q' not found";
  EXPECT_TRUE(namesCtrlRegT(v->getTypespec())) << "26.3: 'ctrl_reg_t' is imported from aes_pkg";
}

TEST_F(ParamNoSubstTest, AesCorePassesImportedCtrlReset) {
  const hldb::RefObj *const reset =
      any_cast<hldb::RefObj>(getResvalOverride("aes_core", "ctrl_shadowed_reg", "prim_subreg_shadow"));
  ASSERT_NE(reset, nullptr) << "ctrl_shadowed_reg instantiates prim_subreg_shadow with '.RESVAL(CTRL_RESET)'";
  EXPECT_EQ(reset->getName(), "CTRL_RESET");
  ASSERT_NE(getParam(getPkg(), "CTRL_RESET"), nullptr);
  EXPECT_EQ(reset->getActual(), getParam(getPkg(), "CTRL_RESET")) << "26.3: 'CTRL_RESET' is imported from aes_pkg";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamNoSubstTest, ResvalIsZeroAtBothLevels) {
  if (m_design->getElaborated()) {
    const hldb::Module *const core = hldb::findByDefName<hldb::Module>("aes_core", m_design->getTopModules());
    ASSERT_NE(core, nullptr) << "the elaborated aes_core not found";
    const hldb::Module *const shadow = hldb::findByName<hldb::Module>("ctrl_shadowed_reg", core->getModules());
    ASSERT_NE(shadow, nullptr) << "the instance ctrl_shadowed_reg is elaborated";
    ExpectElabParam(shadow, "RESVAL", 0u, "23.10.2.2: CTRL_RESET = '{manual_operation: '0} = 0");
    const hldb::Module *const committed = hldb::findByName<hldb::Module>("committed_reg", shadow->getModules());
    ASSERT_NE(committed, nullptr) << "the instance committed_reg is elaborated";
    ExpectElabParam(committed, "RESVAL", 0u, "23.10.2.2: prim_subreg_shadow's RESVAL = 0");
  }
}

TEST_F(ParamNoSubstTest, AesCoreIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: both prim modules are instantiated";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "aes_core");
  }
}

TEST_F(ParamNoSubstTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "6.22.3: a packed structure is assignment compatible with logic [31:0]";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
