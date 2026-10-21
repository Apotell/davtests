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

// Validates the HLDB model built for tests/ParamFromPackage/dut.sv:
//
//   package aes_pkg;
//      parameter logic [7:0][3:0] X = 32'habcd;
//   endpackage // aes_pkg
//
//   module aes_cipher_core import aes_pkg::*;
//      #(
//         parameter logic [7:0][3:0] P = X,
//         parameter logic Z = 0
//      );
//   endmodule // aes_cipher_core
//
//   module top;
//      aes_cipher_core #(.Z(1)) u_aes_cipher_core();
//   endmodule
//
// The point of the fixture is a parameter port whose default is a name
// imported from a package in the module header (26.4), while the instance
// overrides only the other parameter (23.10.2.2). The regression this file
// exists to catch is HLC failing to resolve 'X' through the header import, or
// letting the override of Z disturb P, so that P loses the package value.
//
// Values: X = 32'habcd. logic [7:0][3:0] is 8 * 4 = 32 bits, so the value
// fits unchanged. In u_aes_cipher_core, P keeps its default X = 32'habcd and
// Z is overridden to 1.
//
// What is checked, and why:
//   Package aes_pkg
//     - exactly 1 parameter, X, local because it is in a package (6.20.1,
//       6.20.4), of type logic [7:0][3:0] (two packed dimensions, 7.4.1),
//       holding the literal 32'habcd
//   Module aes_cipher_core
//     - records exactly 1 import, the wildcard import of aes_pkg written in
//       the header (26.4)
//     - exactly 2 parameters, P and Z, both overridable: they are in the
//       parameter port list and none follows a localparam keyword (6.20.4)
//     - P is logic [7:0][3:0] and defaults to the RefObj X, bound to aes_pkg's
//       Parameter X (26.3)
//     - Z is a 1-bit logic with no packed dimension and defaults to 0
//     - the header writes no list_of_port_declarations (A.1.2), so the
//       module has no port
//   Module top
//     - has no port, and holds exactly 1 instance, u_aes_cipher_core of
//       aes_cipher_core, whose only override is Z = 1 (23.10.2.2)
//   Elaboration
//     - u_aes_cipher_core has P = 32'habcd and Z = 1
//     - top is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the values of the elaborated instance are
// checked only when the design is elaborated, on Design::getTopModules(); the
// declarations and the override are asserted on the definitions in every
// run.
//
// What is NOT checked, and why:
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention.
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
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hlc {

class ParamFromPackageTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamFromPackage.hlc"}); }
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

  static const hldb::RefInstance *getInstance() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_aes_cipher_core", top->getRefInstances());
  }

  // The overrides the unelaborated instance writes.
  static const std::vector<hldb::ParamAssign *> *getOverrides(const hldb::RefInstance *inst) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr) return nullptr;
    return mt->getParamAssigns();
  }

  // Verifies 'rts' is a logic with the packed dimensions 'ranges', each
  // given as {left, right}.
  static void ExpectLogic(const hldb::RefTypespec *rts,
                          const std::vector<std::pair<std::string_view, std::string_view>> &ranges,
                          std::string_view what) {
    ASSERT_NE(rts, nullptr) << what << " has no type";
    const hldb::LogicTypespec *const lt = rts->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << what << " is a logic";
    if (ranges.empty()) {
      EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty()) << what << " has no packed dimension";
      return;
    }
    ASSERT_NE(lt->getRanges(), nullptr) << what;
    ASSERT_EQ(lt->getRanges()->size(), ranges.size()) << what;
    for (size_t i = 0; i < ranges.size(); ++i) {
      const hldb::Constant *const left = lt->getRanges()->at(i)->getLeftExpr<hldb::Constant>();
      ASSERT_NE(left, nullptr) << what << ", dimension " << i;
      EXPECT_EQ(left->getDecompile(), ranges[i].first) << what << ", dimension " << i;
      const hldb::Constant *const right = lt->getRanges()->at(i)->getRightExpr<hldb::Constant>();
      ASSERT_NE(right, nullptr) << what << ", dimension " << i;
      EXPECT_EQ(right->getDecompile(), ranges[i].second) << what << ", dimension " << i;
    }
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

TEST_F(ParamFromPackageTest, AesPkgDeclaresLocalPackedX) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'aes_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const x = getParam(pkg, "X");
  ASSERT_NE(x, nullptr) << "parameter 'X' not found";
  EXPECT_TRUE(x->getLocalParam()) << "6.20.1: a param_assignment in a package becomes a localparam";
  ExpectLogic(x->getTypespec(), {{"7", "0"}, {"3", "0"}}, "X");
}

TEST_F(ParamFromPackageTest, XHoldsAbcdLiteral) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "X");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "32'habcd");
}

// ---------------------------------------------------------------------------
// module aes_cipher_core
// ---------------------------------------------------------------------------

TEST_F(ParamFromPackageTest, CipherCoreHeaderImportsAllOfAesPkg) {
  const hldb::Module *const m = getModule("aes_cipher_core");
  ASSERT_NE(m, nullptr) << "module 'aes_cipher_core' not found";
  std::vector<const hldb::ImportTypespec *> imports;
  if (m->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *m->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "26.4: the header writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "aes_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
}

TEST_F(ParamFromPackageTest, CipherCoreDeclaresOverridablePAndZ) {
  const hldb::Module *const m = getModule("aes_cipher_core");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 2u);
  const hldb::Parameter *const p = getParam(m, "P");
  ASSERT_NE(p, nullptr) << "parameter 'P' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.4: 'P' is a parameter port, not a localparam";
  ExpectLogic(p->getTypespec(), {{"7", "0"}, {"3", "0"}}, "P");
  const hldb::Parameter *const z = getParam(m, "Z");
  ASSERT_NE(z, nullptr) << "parameter 'Z' not found";
  EXPECT_FALSE(z->getLocalParam()) << "6.20.4: 'Z' is a parameter port, not a localparam";
  ExpectLogic(z->getTypespec(), {}, "Z");
}

TEST_F(ParamFromPackageTest, PDefaultsToImportedX) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("aes_cipher_core"), "P");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const x = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(x, nullptr) << "unreduced, the default of 'P' is the name 'X'";
  EXPECT_EQ(x->getName(), "X");
  ASSERT_NE(getParam(getPkg(), "X"), nullptr);
  EXPECT_EQ(x->getActual(), getParam(getPkg(), "X")) << "26.3, 26.4: 'X' is imported from aes_pkg";
}

TEST_F(ParamFromPackageTest, ZDefaultsToZero) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("aes_cipher_core"), "Z");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "0");
}

TEST_F(ParamFromPackageTest, CipherCoreAndTopHaveNoPort) {
  for (std::string_view name : {"aes_cipher_core", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    EXPECT_TRUE(m->getPorts() == nullptr || m->getPorts()->empty())
        << "A.1.2: " << name << "'s header writes no list_of_port_declarations";
  }
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamFromPackageTest, TopInstantiatesCipherCore) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const inst = getInstance();
  ASSERT_NE(inst, nullptr) << "instance 'u_aes_cipher_core' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("aes_cipher_core"));
}

TEST_F(ParamFromPackageTest, InstanceOverridesOnlyZWithOne) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance());
  ASSERT_NE(overrides, nullptr) << "'#(.Z(1))' writes an override";
  ASSERT_EQ(overrides->size(), 1u) << "23.10.2.2: only Z is named";
  const hldb::RefObj *const lhs = overrides->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "Z");
  const hldb::Constant *const c = overrides->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamFromPackageTest, InstanceKeepsPackageValueForP) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    const hldb::Module *const core = hldb::findByName<hldb::Module>("u_aes_cipher_core", top->getModules());
    ASSERT_NE(core, nullptr) << "the instance u_aes_cipher_core is elaborated";
    ExpectElabParam(core, "P", 0xabcdu, "26.4: P keeps its default, aes_pkg::X = 32'habcd");
    ExpectElabParam(core, "Z", 1u, "23.10.2.2: '.Z(1)'");
  }
}

TEST_F(ParamFromPackageTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: aes_cipher_core is instantiated by top";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamFromPackageTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "26.4: an import may appear in a module header";
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
