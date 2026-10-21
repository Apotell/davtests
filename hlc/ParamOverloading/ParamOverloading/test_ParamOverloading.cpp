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

// Validates the HLDB model built for tests/ParamOverloading/dut.sv:
//
//   module prim_subreg;
//      parameter logic [4:0] RESVAL = '0;
//      int a = int'(RESVAL);
//   endmodule // prim_subreg
//
//   module prim_subreg_shadow;
//      typedef struct packed {
//         logic [2:0] a;
//         logic [1:0] b;
//      } struct_ab;
//
//      parameter struct_ab RESVAL = '{
//         a: '0,
//         b: '1
//      };
//
//      prim_subreg #(
//         .RESVAL(RESVAL)
//      ) staged_reg();
//
//   endmodule // prim_subreg_shadow
//
//   module top;
//      typedef struct packed {
//         logic [1:0] a;
//         logic [2:0] b;
//      } struct_ab;
//      parameter v1 = 1;
//      parameter v2 = 0;
//
//      parameter struct_ab CTRL_RESET = '{
//       //  a: '1,
//       //  b: '0
//       v1, v2
//      };
//
//      prim_subreg_shadow #(
//         .RESVAL(CTRL_RESET)
//      ) u_ctrl_reg_shadowed();
//   endmodule // top
//
// The point of the fixture is two modules that each declare a packed
// structure named struct_ab, with the same member names but swapped widths,
// and a value that crosses both of them on its way down. A module is a scope
// (23.9), so each struct_ab is local to its module and the two do not clash.
// Each override is assignment compatible because a packed structure is
// integral (6.11.1) and "all integral types are assignment compatible"
// (6.22.3). The regression this file exists to catch is HLC resolving a
// struct_ab reference to the other module's typedef, or reporting the two
// typedefs as a redefinition.
//
// Values: in top, CTRL_RESET = '{v1, v2} sets a (logic [1:0]) to 1 and b
// (logic [2:0]) to 0, so it is {2'b01, 3'b000} = 5'b01000 = 8.
// u_ctrl_reg_shadowed's RESVAL is prim_subreg_shadow's struct_ab holding the
// same 5 bits, 8, and staged_reg's RESVAL (logic [4:0]) is also 8.
//
// What is checked, and why:
//   Modules
//     - none of the three modules has a port
//   Module prim_subreg
//     - exactly 1 parameter, RESVAL, overridable (no parameter port list,
//       6.20.1), of type logic [4:0], defaulting to '0
//     - the variable 'a' is an int initialized with the cast int'(RESVAL)
//       (6.24.1), whose operand is bound to prim_subreg's own RESVAL
//   Module prim_subreg_shadow
//     - its own struct_ab is packed (7.2.1), with a of logic [2:0] and b of
//       logic [1:0]
//     - RESVAL is overridable, of that struct_ab, and its value keys a: '0
//       and b: '1 (10.9.2)
//     - staged_reg instantiates prim_subreg and overrides RESVAL with the
//       RefObj RESVAL bound to prim_subreg_shadow's own Parameter (23.9)
//   Module top
//     - its own struct_ab is packed, with a of logic [1:0] and b of
//       logic [2:0], and is not the typedef of prim_subreg_shadow
//     - v1 and v2 hold 1 and 0
//     - CTRL_RESET is of top's struct_ab, and its value is a positional
//       pattern (10.9.2) of the RefObjs v1 and v2, bound to top's Parameters
//     - u_ctrl_reg_shadowed instantiates prim_subreg_shadow and overrides
//       RESVAL with the RefObj CTRL_RESET, bound to top's Parameter
//   Elaboration
//     - top's CTRL_RESET, u_ctrl_reg_shadowed's RESVAL and staged_reg's
//       RESVAL all hold 8
//     - top is the only top-level instance (23.3.1)
//   Diagnostics
//     - the two struct_ab typedefs are not reported as a redefinition
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced values are checked only when the
// design is elaborated, on Design::getTopModules(); the unreduced values,
// types and bindings are asserted on the definitions in every run.
//
// What is NOT checked, and why:
//   - The value the variable 'a' holds. 6.8: "Setting the initial value of a
//     static variable as part of the variable declaration ... shall occur
//     before any initial or always procedures are started", that is, when the
//     design runs. HLC is a static compiler/elaborator and is not a
//     simulator, so this is out of scope permanently rather than a gap waiting
//     to be filled; PrimSubregAIsIntCastOfResval is the static evidence of
//     what 'a' is initialized with.
//   - The radix HLC picks for a reduced value is a tool convention, so every
//     integer form is accepted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
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

namespace hlc {

class ParamOverloadingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamOverloading.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(std::string_view module, std::string_view name) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, m->getParameters());
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

  static const hldb::Typedef *getStructAb(std::string_view module) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("struct_ab", m->getTypedefs());
  }

  // Whether 'rts' names the struct_ab of 'module'.
  static bool namesStructAb(const hldb::RefTypespec *rts, std::string_view module) {
    const hldb::Typedef *const td = getStructAb(module);
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

  // Verifies the struct_ab of 'module' is packed with a of logic [aLeft:0]
  // and b of logic [bLeft:0].
  static void ExpectStructAb(std::string_view module, std::string_view aLeft, std::string_view bLeft) {
    const hldb::Typedef *const td = getStructAb(module);
    ASSERT_NE(td, nullptr) << "typedef 'struct_ab' not found in " << module;
    ASSERT_NE(td->getAlias(), nullptr);
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    ASSERT_NE(st, nullptr);
    ASSERT_NE(st->getStruct(), nullptr);
    EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: " << module << "'s struct_ab is declared 'packed'";
    ASSERT_NE(st->getStruct()->getMembers(), nullptr);
    ASSERT_EQ(st->getStruct()->getMembers()->size(), 2u);
    const std::string_view names[] = {"a", "b"};
    const std::string_view lefts[] = {aLeft, bLeft};
    for (size_t i = 0; i < 2; ++i) {
      const hldb::TypespecMember *const m = st->getStruct()->getMembers()->at(i);
      EXPECT_EQ(m->getName(), names[i]) << module << ", member " << i;
      ASSERT_NE(m->getTypespec(), nullptr);
      const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
      ASSERT_NE(lt, nullptr) << module << "'s member " << names[i] << " is a logic";
      ASSERT_NE(lt->getRanges(), nullptr);
      ASSERT_EQ(lt->getRanges()->size(), 1u);
      const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
      ASSERT_NE(left, nullptr);
      EXPECT_EQ(left->getDecompile(), lefts[i]) << module << "'s member " << names[i];
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

TEST_F(ParamOverloadingTest, ModulesHaveNoPort) {
  for (std::string_view name : {"prim_subreg", "prim_subreg_shadow", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    EXPECT_TRUE(m->getPorts() == nullptr || m->getPorts()->empty())
        << "A.1.2: " << name << "'s header writes no list_of_port_declarations";
  }
}

// ---------------------------------------------------------------------------
// module prim_subreg
// ---------------------------------------------------------------------------

TEST_F(ParamOverloadingTest, PrimSubregDeclaresOverridableLogic4To0Resval) {
  const hldb::Module *const m = getModule("prim_subreg");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam("prim_subreg", "RESVAL");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: prim_subreg has no parameter port list, so RESVAL is overridable";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "RESVAL is declared 'logic [4:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ASSERT_NE(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "4");
  const hldb::ParamAssign *const pa = getParamAssign(m, "RESVAL");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "'0");
}

TEST_F(ParamOverloadingTest, PrimSubregAIsIntCastOfResval) {
  const hldb::Module *const m = getModule("prim_subreg");
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", m->getVariables());
  ASSERT_NE(a, nullptr) << "variable 'a' not found";
  ASSERT_NE(a->getTypespec(), nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'a' is declared 'int'";
  // Model convention: HLC keeps a variable's declaration initializer as its
  // value (vpiValue) rather than as vpiExpr, so both are read.
  const hldb::Operation *const cast =
      (a->getExpr() != nullptr) ? a->getExpr<hldb::Operation>() : any_cast<hldb::Operation>(a->getValue());
  ASSERT_NE(cast, nullptr) << "'a' is initialized with int'(RESVAL)";
  EXPECT_EQ(cast->getOpType(), vpiCastOp) << "6.24.1";
  ASSERT_NE(cast->getTypespec(), nullptr);
  EXPECT_NE(cast->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "the casting type is int";
  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 1u);
  const hldb::RefObj *const resval = any_cast<hldb::RefObj>(cast->getOperands()->at(0));
  ASSERT_NE(resval, nullptr);
  EXPECT_EQ(resval->getName(), "RESVAL");
  EXPECT_EQ(resval->getActual(), getParam("prim_subreg", "RESVAL")) << "prim_subreg's own parameter";
}

// ---------------------------------------------------------------------------
// module prim_subreg_shadow
// ---------------------------------------------------------------------------

TEST_F(ParamOverloadingTest, ShadowStructAbIsThreeThenTwoBits) { ExpectStructAb("prim_subreg_shadow", "2", "1"); }

TEST_F(ParamOverloadingTest, ShadowResvalKeysAAndB) {
  const hldb::Parameter *const p = getParam("prim_subreg_shadow", "RESVAL");
  ASSERT_NE(p, nullptr) << "prim_subreg_shadow's RESVAL not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: no parameter port list, so RESVAL is overridable";
  EXPECT_TRUE(namesStructAb(p->getTypespec(), "prim_subreg_shadow")) << "23.9: its own struct_ab";
  const hldb::ParamAssign *const pa = getParamAssign(getModule("prim_subreg_shadow"), "RESVAL");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u);
  const std::string_view keys[] = {"a", "b"};
  const std::string_view values[] = {"'0", "'1"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(i));
    ASSERT_NE(item, nullptr) << "item " << i << " is keyed by a member name";
    EXPECT_EQ(getKey(item), keys[i]);
    ASSERT_NE(item->getPattern<hldb::Constant>(), nullptr);
    EXPECT_EQ(item->getPattern<hldb::Constant>()->getDecompile(), values[i]);
  }
}

TEST_F(ParamOverloadingTest, ShadowPassesItsOwnResvalToStagedReg) {
  const hldb::RefObj *const resval =
      any_cast<hldb::RefObj>(getResvalOverride("prim_subreg_shadow", "staged_reg", "prim_subreg"));
  ASSERT_NE(resval, nullptr) << "staged_reg instantiates prim_subreg with '.RESVAL(RESVAL)'";
  EXPECT_EQ(resval->getName(), "RESVAL");
  ASSERT_NE(getParam("prim_subreg_shadow", "RESVAL"), nullptr);
  EXPECT_EQ(resval->getActual(), getParam("prim_subreg_shadow", "RESVAL"))
      << "23.9: the override is written in prim_subreg_shadow's scope";
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamOverloadingTest, TopStructAbIsTwoThenThreeBits) { ExpectStructAb("top", "1", "2"); }

TEST_F(ParamOverloadingTest, TheTwoStructAbAreDistinct) {
  ASSERT_NE(getStructAb("top"), nullptr);
  ASSERT_NE(getStructAb("prim_subreg_shadow"), nullptr);
  EXPECT_NE(getStructAb("top"), getStructAb("prim_subreg_shadow")) << "23.9: each module has its own struct_ab";
}

TEST_F(ParamOverloadingTest, V1AndV2HoldOneAndZero) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::ParamAssign *const v1 = getParamAssign(top, "v1");
  ASSERT_NE(v1, nullptr);
  ASSERT_NE(v1->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(v1->getRhs<hldb::Constant>()->getDecompile(), "1");
  const hldb::ParamAssign *const v2 = getParamAssign(top, "v2");
  ASSERT_NE(v2, nullptr);
  ASSERT_NE(v2->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(v2->getRhs<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(ParamOverloadingTest, CtrlResetIsPositionalPatternOfV1AndV2) {
  const hldb::Parameter *const p = getParam("top", "CTRL_RESET");
  ASSERT_NE(p, nullptr) << "parameter 'CTRL_RESET' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: top has no parameter port list";
  EXPECT_TRUE(namesStructAb(p->getTypespec(), "top")) << "23.9: CTRL_RESET is of top's own struct_ab";
  const hldb::ParamAssign *const pa = getParamAssign(getModule("top"), "CTRL_RESET");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2: a positional structure pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u) << "the commented-out keyed items are not part of it (5.4)";
  const std::string_view names[] = {"v1", "v2"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(pattern->getOperands()->at(i));
    ASSERT_NE(ref, nullptr) << "item " << i << " is the name '" << names[i] << "'";
    EXPECT_EQ(ref->getName(), names[i]);
    ASSERT_NE(getParam("top", names[i]), nullptr);
    EXPECT_EQ(ref->getActual(), getParam("top", names[i])) << "top's own parameter";
  }
}

TEST_F(ParamOverloadingTest, TopPassesCtrlResetToShadow) {
  const hldb::RefObj *const reset =
      any_cast<hldb::RefObj>(getResvalOverride("top", "u_ctrl_reg_shadowed", "prim_subreg_shadow"));
  ASSERT_NE(reset, nullptr) << "u_ctrl_reg_shadowed instantiates prim_subreg_shadow with '.RESVAL(CTRL_RESET)'";
  EXPECT_EQ(reset->getName(), "CTRL_RESET");
  ASSERT_NE(getParam("top", "CTRL_RESET"), nullptr);
  EXPECT_EQ(reset->getActual(), getParam("top", "CTRL_RESET")) << "top's own parameter";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamOverloadingTest, ValueEightReachesBothLevels) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    ExpectElabParam(top, "CTRL_RESET", 8u, "10.9.2: '{v1, v2} = {2'b01, 3'b000} = 5'b01000");
    const hldb::Module *const shadow = hldb::findByName<hldb::Module>("u_ctrl_reg_shadowed", top->getModules());
    ASSERT_NE(shadow, nullptr) << "the instance u_ctrl_reg_shadowed is elaborated";
    ExpectElabParam(shadow, "RESVAL", 8u, "6.22.3: the same 5 bits in prim_subreg_shadow's struct_ab");
    const hldb::Module *const staged = hldb::findByName<hldb::Module>("staged_reg", shadow->getModules());
    ASSERT_NE(staged, nullptr) << "the instance staged_reg is elaborated";
    ExpectElabParam(staged, "RESVAL", 8u, "6.22.3: the same 5 bits as logic [4:0]");
  }
}

TEST_F(ParamOverloadingTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: both prim modules are instantiated";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamOverloadingTest, StructAbIsNotReportedAsRedefined) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_TYPEDEF), nullptr)
      << "23.9: each struct_ab is declared once in its own module scope";
}

TEST_F(ParamOverloadingTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "6.22.3: all integral types are assignment compatible";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
