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

// Validates the HLDB model built for tests/ParamOverload2/dut.sv:
//
//   package keymgr_pkg;
//      typedef logic [63:0][5:0] lfsr_perm_t;
//      parameter lfsr_perm_t RndCnstLfsrPermDefault = {
//         128'h16108c9f9008aa37e5118d1ec1df64a7,
//         256'h24f3f1b73537f42d38383ee8f897286df81d49ab54b6bbbb666cbd1a16c41252
//      };
//   endpackage // keymgr_pkg
//
//   module keymgr;
//      import keymgr_pkg::*;
//      parameter lfsr_perm_t RndCnstLfsrPerm = RndCnstLfsrPermDefault;
//   endmodule // keymgr
//
//   module top;
//      keymgr #(
//         .RndCnstLfsrPerm(384'b1)
//      ) u_keymgr ();
//   endmodule // top
//
// The point of the fixture is a parameter wider than 64 bits: lfsr_perm_t is
// 64 * 6 = 384 bits (7.4.1), its default is a concatenation of a 128-bit and
// a 256-bit literal, also 384 bits (11.4.12), and the only instance
// overrides it with a 384-bit literal. The regression this file exists to
// catch is HLC truncating a wide constant to a machine word, or letting the
// default win over the override.
//
// Values: the default concatenation is 384 bits wide. u_keymgr's
// RndCnstLfsrPerm = 384'b1 = 1.
//
// What is checked, and why:
//   Package keymgr_pkg
//     - lfsr_perm_t is a logic with the two packed dimensions [63:0] and
//       [5:0] (7.4.1)
//     - exactly 1 parameter, RndCnstLfsrPermDefault, local because it is in a
//       package (6.20.1), of type lfsr_perm_t, whose value is Operation
//       vpiConcatOp over the 128-bit and the 256-bit literal, in that order
//   Module keymgr
//     - has no port
//     - records exactly 1 import, the wildcard import of keymgr_pkg (26.3)
//     - exactly 1 parameter, RndCnstLfsrPerm, overridable (keymgr has no
//       parameter port list, 6.20.1), of the imported type lfsr_perm_t,
//       defaulting to the RefObj RndCnstLfsrPermDefault bound through the
//       import to the package's Parameter
//   Module top
//     - has no port, and holds exactly 1 instance, u_keymgr of keymgr, whose
//       only override is RndCnstLfsrPerm = 384'b1, and which connects no port
//   Elaboration
//     - on the elaborated package, the default is reduced to a 384-bit
//       Constant (11.4.12)
//     - u_keymgr's RndCnstLfsrPerm is 1
//     - top is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced values are checked only when the
// design is elaborated, on Design::getTopPackages() and
// Design::getTopModules(); the unreduced values and bindings are asserted on
// the definitions in every run.
//
// What is NOT checked, and why:
//   - The full 384-bit reduced value of the default does not fit in a machine
//     integer, so only its width is asserted; the literal text of both
//     concatenated items is asserted on the unreduced value.
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
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamOverload2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamOverload2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("keymgr_pkg", m_design->getAllPackages());
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

  static const hldb::Typedef *getPermT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("lfsr_perm_t", pkg->getTypedefs());
  }

  // Whether 'rts' names lfsr_perm_t.
  static bool namesPermT(const hldb::RefTypespec *rts) {
    const hldb::Typedef *const td = getPermT();
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr || rts->getActual() == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = rts->getActual<hldb::TypedefTypespec>();
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
           (rts->getActual() == td->getAlias()->getActual());
  }

  static const hldb::RefInstance *getUKeymgr() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_keymgr", top->getRefInstances());
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
};

// ---------------------------------------------------------------------------
// package keymgr_pkg
// ---------------------------------------------------------------------------

TEST_F(ParamOverload2Test, LfsrPermTIsLogic63To0By5To0) {
  const hldb::Typedef *const td = getPermT();
  ASSERT_NE(td, nullptr) << "typedef 'lfsr_perm_t' not found in keymgr_pkg";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::LogicTypespec *const lt = td->getAlias()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "lfsr_perm_t is a logic";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 2u) << "7.4.1: two packed dimensions";
  const std::string_view lefts[] = {"63", "5"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Constant *const left = lt->getRanges()->at(i)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << "dimension " << i;
    EXPECT_EQ(left->getDecompile(), lefts[i]) << "dimension " << i;
    const hldb::Constant *const right = lt->getRanges()->at(i)->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr) << "dimension " << i;
    EXPECT_EQ(right->getDecompile(), "0") << "dimension " << i;
  }
}

TEST_F(ParamOverload2Test, DefaultIsLocalLfsrPermT) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'keymgr_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(pkg, "RndCnstLfsrPermDefault");
  ASSERT_NE(p, nullptr) << "parameter 'RndCnstLfsrPermDefault' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.1: a param_assignment in a package becomes a localparam";
  EXPECT_TRUE(namesPermT(p->getTypespec())) << "'RndCnstLfsrPermDefault' is declared 'lfsr_perm_t'";
}

TEST_F(ParamOverload2Test, DefaultIsConcatOf128And256BitLiterals) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "RndCnstLfsrPermDefault");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const concat = pa->getRhs<hldb::Operation>();
  ASSERT_NE(concat, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "11.4.12";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u);
  const hldb::Constant *const hi = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(hi->getDecompile(), "128'h16108c9f9008aa37e5118d1ec1df64a7");
  const hldb::Constant *const lo = any_cast<hldb::Constant>(concat->getOperands()->at(1));
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(lo->getDecompile(), "256'h24f3f1b73537f42d38383ee8f897286df81d49ab54b6bbbb666cbd1a16c41252");
}

// ---------------------------------------------------------------------------
// module keymgr
// ---------------------------------------------------------------------------

TEST_F(ParamOverload2Test, KeymgrAndTopHaveNoPort) {
  for (std::string_view name : {"keymgr", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    EXPECT_TRUE(m->getPorts() == nullptr || m->getPorts()->empty())
        << "A.1.2: " << name << "'s header writes no list_of_port_declarations";
  }
}

TEST_F(ParamOverload2Test, KeymgrImportsAllOfKeymgrPkg) {
  const hldb::Module *const m = getModule("keymgr");
  ASSERT_NE(m, nullptr);
  std::vector<const hldb::ImportTypespec *> imports;
  if (m->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *m->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "keymgr writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "keymgr_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
}

TEST_F(ParamOverload2Test, RndCnstLfsrPermDefaultsToPackageValue) {
  const hldb::Module *const m = getModule("keymgr");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(m, "RndCnstLfsrPerm");
  ASSERT_NE(p, nullptr) << "parameter 'RndCnstLfsrPerm' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: keymgr has no parameter port list, so the parameter is overridable";
  EXPECT_TRUE(namesPermT(p->getTypespec())) << "26.3: 'lfsr_perm_t' is imported from keymgr_pkg";
  const hldb::ParamAssign *const pa = getParamAssign(m, "RndCnstLfsrPerm");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const def = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(def, nullptr) << "unreduced, the default is the name 'RndCnstLfsrPermDefault'";
  EXPECT_EQ(def->getName(), "RndCnstLfsrPermDefault");
  ASSERT_NE(getParam(getPkg(), "RndCnstLfsrPermDefault"), nullptr);
  EXPECT_EQ(def->getActual(), getParam(getPkg(), "RndCnstLfsrPermDefault")) << "26.3: imported from keymgr_pkg";
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamOverload2Test, UKeymgrOverridesWith384BitOne) {
  const hldb::RefInstance *const inst = getUKeymgr();
  ASSERT_NE(inst, nullptr) << "instance 'u_keymgr' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("keymgr"));
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = mt->getParamAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "RndCnstLfsrPerm");
  const hldb::Constant *const c = mt->getParamAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "384'b1");
}

TEST_F(ParamOverload2Test, UKeymgrConnectsNoPort) {
  const hldb::RefInstance *const inst = getUKeymgr();
  ASSERT_NE(inst, nullptr);
  EXPECT_TRUE(inst->getArguments() == nullptr || inst->getArguments()->empty())
      << "'u_keymgr ()' writes an empty connection list";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamOverload2Test, DefaultReducesTo384Bits) {
  if (m_design->getElaborated()) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>("keymgr_pkg", m_design->getTopPackages());
    ASSERT_NE(pkg, nullptr) << "the elaborated keymgr_pkg not found";
    const hldb::ParamAssign *const pa = getParamAssign(pkg, "RndCnstLfsrPermDefault");
    ASSERT_NE(pa, nullptr);
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: the default should be reduced to a Constant";
    EXPECT_EQ(c->getSize(), 384) << "11.4.12: 128 + 256 bits";
  }
}

TEST_F(ParamOverload2Test, UKeymgrReceivesOne) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    const hldb::Module *const keymgr = hldb::findByName<hldb::Module>("u_keymgr", top->getModules());
    ASSERT_NE(keymgr, nullptr) << "the instance u_keymgr is elaborated";
    const hldb::ParamAssign *const pa = getParamAssign(keymgr, "RndCnstLfsrPerm");
    ASSERT_NE(pa, nullptr);
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: the parameter should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, 1u) << "23.10.2.2: the override 384'b1 replaces the default";
  }
}

TEST_F(ParamOverload2Test, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: keymgr is instantiated by top";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamOverload2Test, NoFatalSyntaxOrErrorDiagnostics) {
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
