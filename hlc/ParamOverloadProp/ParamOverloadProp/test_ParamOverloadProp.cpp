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

// Validates the HLDB model built for tests/ParamOverloadProp/dut.sv:
//
//   module top;
//      prim_flop #(
//         .Width(22)
//      ) u_prim_flop();
//   endmodule
//
//   module prim_flop;
//      parameter int Width = 1;
//      parameter logic [Width-1:0] ResetValue = Width;
//   endmodule
//
// The point of the fixture is a parameter whose declared type and value both
// depend on another parameter that the instance overrides. 6.20.1: "a
// parameter can depend on earlier parameters". ResetValue's type is
// logic [Width-1:0], so its range is computed from Width, and u_prim_flop
// overrides Width with 22 (23.10.2.2). The regression this file exists to
// catch is HLC sizing ResetValue with the default Width = 1, which would
// truncate the value 22 (5'b10110) to its lowest bit, 0.
//
// Values: in u_prim_flop, Width = 22, so ResetValue is logic [21:0] and holds
// 22.
//
// What is checked, and why:
//   Module prim_flop
//     - has no port
//     - exactly 2 parameters, Width and ResetValue, both overridable
//       (prim_flop has no parameter port list, 6.20.1)
//     - Width is an int defaulting to 1
//     - ResetValue is logic [Width-1:0], whose left bound is Operation
//       vpiSubOp over the RefObj Width, bound to prim_flop's Parameter, and
//       the Constant 1, and it defaults to the RefObj Width
//   Module top
//     - has no port, and holds exactly 1 instance, u_prim_flop of prim_flop,
//       whose only override is Width = 22 (23.10.2.2)
//   Elaboration
//     - u_prim_flop's Width is 22 and its ResetValue is 22
//     - u_prim_flop's ResetValue has the range [21:0]
//     - top is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the values and the range after the override are
// checked only when the design is elaborated, on Design::getTopModules();
// the unreduced declarations and the override are asserted on the
// definitions in every run.
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
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {

class ParamOverloadPropTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamOverloadProp.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
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

  static const hldb::Module *getElabFlop() {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Module>("u_prim_flop", top->getModules());
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

TEST_F(ParamOverloadPropTest, ModulesHaveNoPort) {
  for (std::string_view name : {"top", "prim_flop"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    EXPECT_TRUE(m->getPorts() == nullptr || m->getPorts()->empty())
        << "A.1.2: " << name << "'s header writes no list_of_port_declarations";
  }
}

// ---------------------------------------------------------------------------
// module prim_flop
// ---------------------------------------------------------------------------

TEST_F(ParamOverloadPropTest, PrimFlopDeclaresTwoOverridableParameters) {
  const hldb::Module *const m = getModule("prim_flop");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 2u);
  for (std::string_view name : {"Width", "ResetValue"}) {
    const hldb::Parameter *const p = getParam(m, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: prim_flop has no parameter port list, so '" << name
                                     << "' is overridable";
  }
}

TEST_F(ParamOverloadPropTest, WidthIsIntDefaultingToOne) {
  const hldb::Module *const m = getModule("prim_flop");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const width = getParam(m, "Width");
  ASSERT_NE(width, nullptr);
  ASSERT_NE(width->getTypespec(), nullptr);
  EXPECT_NE(width->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'Width' is declared 'int'";
  const hldb::ParamAssign *const pa = getParamAssign(m, "Width");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "1");
}

TEST_F(ParamOverloadPropTest, ResetValueIsLogicWidthMinusOneToZero) {
  const hldb::Module *const m = getModule("prim_flop");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const rv = getParam(m, "ResetValue");
  ASSERT_NE(rv, nullptr);
  ASSERT_NE(rv->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = rv->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'ResetValue' is declared 'logic [Width-1:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Operation *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "unreduced, the left bound is 'Width-1'";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const width = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(width, nullptr);
  EXPECT_EQ(width->getName(), "Width");
  EXPECT_EQ(width->getActual(), getParam(m, "Width")) << "'Width' is prim_flop's own parameter";
  const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(ParamOverloadPropTest, ResetValueDefaultsToWidth) {
  const hldb::Module *const m = getModule("prim_flop");
  ASSERT_NE(m, nullptr);
  const hldb::ParamAssign *const pa = getParamAssign(m, "ResetValue");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const width = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(width, nullptr) << "unreduced, the default is the name 'Width'";
  EXPECT_EQ(width->getName(), "Width");
  EXPECT_EQ(width->getActual(), getParam(m, "Width")) << "'Width' is prim_flop's own parameter";
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamOverloadPropTest, UPrimFlopOverridesWidthWithTwentyTwo) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_prim_flop", top->getRefInstances());
  ASSERT_NE(inst, nullptr) << "instance 'u_prim_flop' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("prim_flop"));
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = mt->getParamAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "Width");
  ASSERT_NE(mt->getParamAssigns()->at(0)->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(mt->getParamAssigns()->at(0)->getRhs<hldb::Constant>()->getDecompile(), "22");
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamOverloadPropTest, ElaboratedValuesAreTwentyTwo) {
  if (m_design->getElaborated()) {
    const hldb::Module *const flop = getElabFlop();
    ASSERT_NE(flop, nullptr) << "the instance u_prim_flop is elaborated";
    ExpectElabParam(flop, "Width", 22u, "23.10.2.2: '.Width(22)'");
    ExpectElabParam(flop, "ResetValue", 22u, "6.20.2: ResetValue = Width = 22 fits in logic [21:0]");
  }
}

TEST_F(ParamOverloadPropTest, ElaboratedResetValueIsTwentyOneToZero) {
  if (m_design->getElaborated()) {
    const hldb::Module *const flop = getElabFlop();
    ASSERT_NE(flop, nullptr);
    const hldb::Parameter *const rv = getParam(flop, "ResetValue");
    ASSERT_NE(rv, nullptr);
    ASSERT_NE(rv->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = rv->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << "the left bound should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(left, &v)) << "unparsable constant '" << left->getDecompile() << "'";
    EXPECT_EQ(v, 21u) << "6.20.1: the range uses the overridden Width, 22 - 1";
  }
}

TEST_F(ParamOverloadPropTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: prim_flop is instantiated by top";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamOverloadPropTest, NoFatalSyntaxOrErrorDiagnostics) {
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
