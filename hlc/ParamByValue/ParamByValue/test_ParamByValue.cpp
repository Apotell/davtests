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

// Validates the HLDB model built for tests/ParamByValue/dut.sv:
//
//   package lc_ctrl_state_pkg;
//     parameter logic [15:0] B0 = 16'b1110011001111001;
//     parameter logic [15:0] B1 = 16'b1101111101101110;
//     typedef enum logic [31:0] {
//       LcStScrap         = {B1,  B0}
//     } lc_state_e;
//   endpackage
//
//   module generic_flop #(parameter logic [15:0] ResetValue = 0) ( );
//   endmodule
//
//   module prim_flop #( parameter logic [15:0] ResetValue = 0) ();
//       //import lc_ctrl_state_pkg::*;
//       generic_flop #(
//         .ResetValue(ResetValue)
//       ) impl_generic ();
//   endmodule
//
//   module top();
//     import lc_ctrl_state_pkg::*;
//     prim_flop #(
//       .ResetValue(32'(LcStScrap))
//     ) state_regs ();
//   endmodule
//
// The point of the fixture is a parameter value passed by value down two
// levels of instances (IEEE 1800-2023 23.10.2.2): top overrides prim_flop's
// ResetValue with a size cast (6.24.1) of an enum name whose value
// concatenates two package parameters, and prim_flop passes its own
// ResetValue on to generic_flop. ResetValue is declared 'logic [15:0]', and
// an override "shall be assignment compatible with the declared type"
// (6.20.2), so the 32-bit value is truncated to its low 16 bits (10.7). The
// regression this file exists to catch is HLC losing the value, or its
// truncation, on the way down.
//
// Values: B0 = 16'b1110011001111001 = 16'hE679, B1 = 16'b1101111101101110 =
// 16'hDF6E, so LcStScrap = {B1, B0} = 32'hDF6EE679, and both ResetValue
// parameters of the elaborated instances are 16'hE679.
//
// What is checked, and why:
//   Package lc_ctrl_state_pkg
//     - exactly 2 parameters, B0 and B1, each a local (6.20.4) logic [15:0]
//       with the binary literal written
//     - exactly 1 Typedef, lc_state_e, an enum (6.19) with base type logic
//       [31:0] and the single name LcStScrap whose value is Operation
//       vpiConcatOp (11.4.12) over RefObj 'B1' then RefObj 'B0', bound to the
//       package's Parameters
//   Modules generic_flop and prim_flop
//     - each has exactly 1 parameter, ResetValue, declared in the parameter
//       port list so not local (6.20.1), of type logic [15:0], with the
//       default Constant "0"
//     - prim_flop holds exactly 1 instance, impl_generic, of generic_flop,
//       overriding ResetValue by name with RefObj 'ResetValue' bound to
//       prim_flop's own Parameter
//   Module top
//     - records exactly 1 import, the wildcard import of lc_ctrl_state_pkg
//     - holds exactly 1 instance, state_regs, of prim_flop, overriding
//       ResetValue with Operation vpiCastOp (6.24.1) whose operand is RefObj
//       'LcStScrap' bound through the import to the package's EnumConst
//   Every '()' or '( )' port list declares one null port (37.14 detail 10,
//   "module M();")
//   Elaboration
//     - prim_flop and generic_flop are instantiated, so top is the only
//       top-level instance (23.3.1)
//     - top.state_regs has ResetValue = 16'hE679, and
//       top.state_regs.impl_generic has ResetValue = 16'hE679
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the parameter values are checked only when the
// design is elaborated, on the instances under Design::getTopModules(); the
// unreduced overrides are asserted on the definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - How HLC records the target width of the size cast '32'(...)' is a
//     model convention; the cast's operand and the truncated result are
//     asserted.
//   - How HLC records a parameter override of an unelaborated instance is a
//     model convention; it is read from the instance's module typespec.
//   - The name HLC gives a parameterized module definition: modules are
//     looked up by their definition name.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The commented-out import in prim_flop is not a design object (5.4).

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
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

// The value both elaborated ResetValue parameters hold: the low 16 bits of
// LcStScrap = 32'hDF6EE679.
constexpr uint64_t kResetValue = 0xE679;

class ParamByValueTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamByValue.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("lc_ctrl_state_pkg", m_design->getAllPackages());
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

  static const hldb::EnumConst *getLcStScrap() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("lc_state_e", pkg->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (et == nullptr || et->getEnum() == nullptr) return nullptr;
    return hldb::findByName<hldb::EnumConst>("LcStScrap", et->getEnum()->getEnumConsts());
  }

  // The override expression for 'param' that the unelaborated instance
  // 'inst' writes.
  static const hldb::Any *getOverride(const hldb::RefInstance *inst, std::string_view param) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr || mt->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *mt->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == param)) return pa->getRhs();
    }
    return nullptr;
  }

  static const hldb::RefInstance *getOnlyInstance(std::string_view moduleName) {
    const hldb::Module *const m = getModule(moduleName);
    if (m == nullptr || m->getRefInstances() == nullptr || m->getRefInstances()->size() != 1u) return nullptr;
    return m->getRefInstances()->at(0);
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
    const hldb::ParamAssign *const pa = getParamAssign(scope, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "' in the elaborated scope";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << why;
  }

  // Verifies 'scope' declares exactly 1 parameter, ResetValue, overridable
  // and of type logic [15:0], defaulting to 0.
  static void ExpectResetValueParameter(const hldb::Module *m, std::string_view what) {
    ASSERT_NE(m, nullptr) << what << " not found";
    ASSERT_NE(m->getParameters(), nullptr);
    ASSERT_EQ(m->getParameters()->size(), 1u) << what;
    const hldb::Parameter *const p = getParam(m, "ResetValue");
    ASSERT_NE(p, nullptr) << what;
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: " << what << "'s ResetValue is in the parameter port list";
    ASSERT_NE(p->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << what << ": declared 'logic [15:0]'";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->getDecompile(), "15");
    const hldb::ParamAssign *const pa = getParamAssign(m, "ResetValue");
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
    EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "0") << what;
  }
};

// ---------------------------------------------------------------------------
// package lc_ctrl_state_pkg;
// ---------------------------------------------------------------------------

TEST_F(ParamByValueTest, B0AndB1AreLocalLogic15To0Literals) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'lc_ctrl_state_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 2u);
  const char *const names[] = {"B0", "B1"};
  const char *const values[] = {"16'b1110011001111001", "16'b1101111101101110"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Parameter *const p = getParam(pkg, names[i]);
    ASSERT_NE(p, nullptr) << "parameter '" << names[i] << "' not found";
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
    const hldb::ParamAssign *const pa = getParamAssign(pkg, names[i]);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
    EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), values[i]);
  }
}

TEST_F(ParamByValueTest, LcStScrapIsConcatOfB1AndB0) {
  const hldb::EnumConst *const scrap = getLcStScrap();
  ASSERT_NE(scrap, nullptr) << "enum name 'LcStScrap' not found";
  const hldb::Operation *const concat = scrap->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr) << "unreduced, '{B1, B0}' is an Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "11.4.12: a concatenation";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u);
  const char *const names[] = {"B1", "B0"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(concat->getOperands()->at(i));
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getName(), names[i]);
    EXPECT_EQ(ref->getActual(), getParam(getPkg(), names[i]));
  }
}

// ---------------------------------------------------------------------------
// modules generic_flop, prim_flop and top
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamByValueTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"generic_flop", "prim_flop", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: the empty port list of " << name << " is a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamByValueTest, FlopsDeclareOverridableResetValue) {
  ExpectResetValueParameter(getModule("generic_flop"), "generic_flop");
  ExpectResetValueParameter(getModule("prim_flop"), "prim_flop");
}

TEST_F(ParamByValueTest, PrimFlopPassesItsResetValueToGenericFlop) {
  const hldb::RefInstance *const inst = getOnlyInstance("prim_flop");
  ASSERT_NE(inst, nullptr) << "prim_flop holds exactly one instance";
  EXPECT_EQ(inst->getName(), "impl_generic");
  const hldb::RefObj *const value = any_cast<hldb::RefObj>(getOverride(inst, "ResetValue"));
  ASSERT_NE(value, nullptr) << "23.10.2.2: '.ResetValue(ResetValue)' overrides by name";
  EXPECT_EQ(value->getName(), "ResetValue");
  EXPECT_EQ(value->getActual(), getParam(getModule("prim_flop"), "ResetValue"))
      << "the value is prim_flop's own parameter";
}

TEST_F(ParamByValueTest, TopImportsAllOfStatePkg) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  std::vector<const hldb::ImportTypespec *> imports;
  if (top->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *top->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "top writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "lc_ctrl_state_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
}

TEST_F(ParamByValueTest, TopOverridesResetValueWithCastOfLcStScrap) {
  const hldb::RefInstance *const inst = getOnlyInstance("top");
  ASSERT_NE(inst, nullptr) << "top holds exactly one instance";
  EXPECT_EQ(inst->getName(), "state_regs");
  const hldb::Operation *const cast = any_cast<hldb::Operation>(getOverride(inst, "ResetValue"));
  ASSERT_NE(cast, nullptr) << "23.10.2.2: '.ResetValue(32'(LcStScrap))' overrides with a cast";
  EXPECT_EQ(cast->getOpType(), vpiCastOp) << "6.24.1: a size cast";
  ASSERT_NE(cast->getOperands(), nullptr);
  const hldb::RefObj *scrap = nullptr;
  for (const hldb::Any *const operand : *cast->getOperands()) {
    if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(operand)) scrap = ref;
  }
  ASSERT_NE(scrap, nullptr) << "the cast's operand is 'LcStScrap'";
  EXPECT_EQ(scrap->getName(), "LcStScrap");
  ASSERT_NE(getLcStScrap(), nullptr);
  EXPECT_EQ(scrap->getActual(), getLcStScrap()) << "26.3: LcStScrap is imported from lc_ctrl_state_pkg";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamByValueTest, ResetValueReachesBothInstancesTruncatedTo16Bits) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: prim_flop and generic_flop are instantiated";
    const hldb::Module *const top = m_design->getTopModules()->at(0);
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->getDefName(), "top");
    const hldb::Module *const regs = hldb::findByName<hldb::Module>("state_regs", top->getModules());
    ASSERT_NE(regs, nullptr) << "the instance state_regs is elaborated";
    ExpectElabParam(regs, "ResetValue", kResetValue,
                    "6.20.2, 10.7: 32'(LcStScrap) = 32'hDF6EE679 is truncated to the 16-bit ResetValue, 16'hE679");
    const hldb::Module *const generic = hldb::findByName<hldb::Module>("impl_generic", regs->getModules());
    ASSERT_NE(generic, nullptr) << "the instance impl_generic is elaborated";
    ExpectElabParam(generic, "ResetValue", kResetValue, "23.10.2.2: prim_flop passes its ResetValue, 16'hE679");
  }
}

TEST_F(ParamByValueTest, NoFatalSyntaxOrErrorDiagnostics) {
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
