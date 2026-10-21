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

// Validates the HLDB model built for tests/ParamIndex/dut.sv:
//
//   module top ();
//   parameter type TPTOP = int;
//    TPTOP DATATOP;
//     mid #(.TP0(TPTOP)) u0();
//   endmodule
//
//   module mid #(parameter type TP0 = logic) ();
//    TP0 DATA0;
//    bottom #(.TP1(TP0)) u1();
//    bottom #(.TP1(int), .SIZE(20)) u2();
//    bottom #(int, 1120, int, 1130) u3();
//    bottom #(int, 1140, int, UNDEF) u4();
//   endmodule
//
//   module bottom #(parameter type TP1 = logic, parameter SIZE=10,
//                   parameter type TP2 = int, parameter int PPP = 1150);
//       TP1 DATA1;
//       logic [SIZE:0] a;
//       TP2 DATA2;
//   endmodule
//
// The point of the fixture is parameter overriding by position (23.10.2.1)
// in a parameter list that interleaves type parameters (6.20.3) with value
// parameters, next to overriding by name (23.10.2.2) and passing a type
// parameter down two levels. 23.10.2.1: "The order of the assignments in the
// module instance parameter assignment by ordered list shall follow the order
// of declaration of the parameters within the module." The regression this
// file exists to catch is HLC indexing type and value parameters separately,
// so that 1120 lands on PPP instead of SIZE, or the type 'int' lands on a
// value parameter.
//
// Values: in u3, TP1 = int, SIZE = 1120, TP2 = int and PPP = 1130, so 'a' is
// logic [1120:0]. In u1, TP1 = TP0 = TPTOP = int and SIZE keeps 10. In u2,
// TP1 = int and SIZE = 20. u4 is illegal: its fourth value, UNDEF, is not
// declared anywhere, and 6.20.2 allows a value parameter to be set only to
// "an expression of literals, value parameters or local parameters, genvars,
// enumerated names, or a constant function of these".
//
// What is checked, and why:
//   Module top and module mid
//     - each '()' declares one null port (37.14 detail 10, "module M();")
//   Module bottom
//     - the header writes no list_of_port_declarations (A.1.2), so bottom
//       has no port
//     - declares TP1, SIZE, TP2 and PPP in that order, all overridable
//       (6.20.4), with the defaults logic, 10, int and 1150
//     - its body declares DATA1 of type TP1, 'a' of logic [SIZE:0] and DATA2
//       of type TP2, each bound to bottom's own parameter (6.20.3)
//   Module top
//     - declares the overridable type parameter TPTOP, defaulting to int,
//       and DATATOP of type TPTOP
//     - u0 instantiates mid and overrides TP0 with TPTOP
//   Module mid
//     - declares the overridable type parameter TP0, defaulting to logic,
//       and DATA0 of type TP0
//     - u1 to u4 all instantiate bottom
//     - u1 overrides TP1 with TP0; u2 overrides TP1 with int and SIZE with 20
//     - u3 writes 4 ordered values: int, 1120, int, 1130
//     - u4 writes 4 ordered values whose last, UNDEF, is bound to nothing
//     - every name in a named override is bound to the instantiated module's
//       parameter (23.10.2.2)
//   Elaboration
//     - DATATOP and u0's DATA0 are int; u1's DATA1 is int
//     - 'a' is logic [10:0] in u1, [20:0] in u2 and [1120:0] in u3
//     - u3's PPP is 1130
//     - top is the only top-level instance (23.3.1)
//   Diagnostics
//     - the undeclared UNDEF is reported as an error
//     - there is no fatal and no syntax diagnostic
//
// Reduction and elaboration: the resolved types and values are checked only
// when the design is elaborated, on Design::getTopModules(); the
// declarations, bindings and overrides are asserted on the definitions in
// every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list '()' with no port at all, although 37.14 detail
// 10 names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (type parameter names in overrides not bound), not a
// defect in this test: in '.TP0(TPTOP)' and '.TP1(TP0)', HLC binds neither
// the type parameter named before the parentheses nor the one written inside
// them, and only warns "Failed to bind". 6.20.3 declares TPTOP in top and TP0
// in mid, and 23.10.2.2 names the instantiated module's parameter, so every
// one of these names has a declaration. Value parameters in the same
// position ('.SIZE(20)') are bound. U0OverridesTp0WithTptop,
// U1OverridesTp1WithTp0 and NamedOverridesBindToTargetParameters are
// expected to fail until HLC is fixed; they are intentionally not skipped or
// relaxed.
//
// KNOWN COMPILER BUG (undeclared name only warned), not a defect in this
// test: HLC reports the undeclared UNDEF in u4's override with a warning
// only, although 6.20.2 allows a value parameter to be set only to "an
// expression of literals, value parameters or local parameters, genvars,
// enumerated names, or a constant function of these". UndeclaredUndefIsAnError
// is expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// Model convention: an override whose value is a type name may hold it as a
// RefObj rather than a RefTypespec; the tests read either and assert what it
// is bound to.
//
// What is NOT checked, and why:
//   - Any elaborated value of u4. The instance is illegal, so the standard
//     gives none of its values.
//   - Which error identifier HLC uses for UNDEF is a tool convention, so any
//     error-severity diagnostic naming UNDEF is accepted.
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
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/type_parameter.h>
#include <hldb/variable.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamIndexTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamIndex.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Whether a diagnostic of error severity names 'symbol'. Which error
  // identifier HLC picks for it is a tool convention, so any is accepted.
  bool reportsErrorNaming(std::string_view symbol) {
    for (const ErrorDefinition::ErrorMap::value_type &entry : ErrorDefinition::getErrorInfoMap()) {
      if ((entry.second.m_severity == ErrorDefinition::ERROR) && (findError(entry.first, symbol) != nullptr)) {
        return true;
      }
    }
    return false;
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  static const hldb::TypeParameter *getTypeParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::TypeParameter>(name, scope->getParameters());
  }

  // The name the LHS of 'pa' writes, whatever node holds it.
  static std::string_view getLhsName(const hldb::ParamAssign *pa) {
    if (const hldb::RefObj *const ref = pa->getLhs<hldb::RefObj>()) return ref->getName();
    if (const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>()) return p->getName();
    if (const hldb::TypeParameter *const tp = pa->getLhs<hldb::TypeParameter>()) return tp->getName();
    if (const hldb::RefTypespec *const rts = pa->getLhs<hldb::RefTypespec>()) return rts->getName();
    return {};
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      if (getLhsName(pa) == name) return pa;
    }
    return nullptr;
  }

  // The type a type-valued RHS names. Whether a type name is held as a
  // RefTypespec or as a RefObj is a model convention, so both are read.
  static const hldb::Typespec *getRhsType(const hldb::ParamAssign *pa) {
    if (pa == nullptr) return nullptr;
    if (const hldb::RefTypespec *const rts = pa->getRhs<hldb::RefTypespec>()) return rts->getActual();
    if (const hldb::RefObj *const ref = pa->getRhs<hldb::RefObj>()) return any_cast<hldb::Typespec>(ref->getActual());
    return pa->getRhs<hldb::Typespec>();
  }

  // The declaration the LHS of 'pa' is bound to.
  static const hldb::Any *getLhsActual(const hldb::ParamAssign *pa) {
    if (const hldb::RefObj *const ref = pa->getLhs<hldb::RefObj>()) return ref->getActual();
    if (const hldb::RefTypespec *const rts = pa->getLhs<hldb::RefTypespec>()) return rts->getActual();
    return pa->getLhs();
  }

  static const hldb::RefInstance *getInstance(std::string_view module, std::string_view name) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(name, m->getRefInstances());
  }

  // The overrides the unelaborated instance writes, in source order.
  static const std::vector<hldb::ParamAssign *> *getOverrides(const hldb::RefInstance *inst) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr) return nullptr;
    return mt->getParamAssigns();
  }

  static const hldb::Variable *getVariable(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, scope->getVariables());
  }

  // The elaborated instance at 'path' below the elaborated top.
  static const hldb::Module *getElabInstance(const std::vector<std::string_view> &path) {
    const hldb::Module *m = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    for (std::string_view name : path) {
      if (m == nullptr) return nullptr;
      m = hldb::findByName<hldb::Module>(name, m->getModules());
    }
    return m;
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

  // Verifies the variable 'name' of the elaborated 'scope' is an int.
  static void ExpectElabInt(const hldb::Scope *scope, std::string_view name, std::string_view why) {
    const hldb::Variable *const v = getVariable(scope, name);
    ASSERT_NE(v, nullptr) << "variable '" << name << "' not found";
    ASSERT_NE(v->getTypespec(), nullptr);
    EXPECT_NE(v->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << why;
  }

  // Verifies 'a' of the elaborated 'scope' is logic [left:0].
  static void ExpectElabARange(const hldb::Scope *scope, uint64_t left, std::string_view why) {
    const hldb::Variable *const a = getVariable(scope, "a");
    ASSERT_NE(a, nullptr) << "variable 'a' not found";
    ASSERT_NE(a->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = a->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Constant *const l = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr) << "the left bound should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(l, &v)) << "unparsable constant '" << l->getDecompile() << "'";
    EXPECT_EQ(v, left) << why;
  }
};

// ---------------------------------------------------------------------------
// Ports
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamIndexTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"top", "mid"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: " << name << "'s '()' declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: " << name << "'s '()' declares exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamIndexTest, BottomHasNoPort) {
  const hldb::Module *const bottom = getModule("bottom");
  ASSERT_NE(bottom, nullptr) << "module 'bottom' not found";
  EXPECT_TRUE(bottom->getPorts() == nullptr || bottom->getPorts()->empty())
      << "A.1.2: bottom's header writes no list_of_port_declarations";
}

// ---------------------------------------------------------------------------
// module bottom
// ---------------------------------------------------------------------------

TEST_F(ParamIndexTest, BottomDeclaresFourOverridableParameters) {
  const hldb::Module *const bottom = getModule("bottom");
  ASSERT_NE(bottom, nullptr);
  ASSERT_NE(bottom->getParameters(), nullptr);
  ASSERT_EQ(bottom->getParameters()->size(), 4u);
  const hldb::TypeParameter *const tp1 = getTypeParam(bottom, "TP1");
  ASSERT_NE(tp1, nullptr) << "6.20.3: 'TP1' is a type parameter";
  EXPECT_FALSE(tp1->getLocalParam()) << "6.20.4: TP1 is a parameter port";
  const hldb::Parameter *const size = getParam(bottom, "SIZE");
  ASSERT_NE(size, nullptr) << "'SIZE' is a value parameter";
  EXPECT_FALSE(size->getLocalParam()) << "6.20.4: SIZE is a parameter port";
  const hldb::TypeParameter *const tp2 = getTypeParam(bottom, "TP2");
  ASSERT_NE(tp2, nullptr) << "6.20.3: 'TP2' is a type parameter";
  EXPECT_FALSE(tp2->getLocalParam()) << "6.20.4: TP2 is a parameter port";
  const hldb::Parameter *const ppp = getParam(bottom, "PPP");
  ASSERT_NE(ppp, nullptr) << "'PPP' is a value parameter";
  EXPECT_FALSE(ppp->getLocalParam()) << "6.20.4: PPP is a parameter port";
  ASSERT_NE(ppp->getTypespec(), nullptr);
  EXPECT_NE(ppp->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "PPP is declared 'int'";
}

TEST_F(ParamIndexTest, BottomParameterOrderIsTp1SizeTp2Ppp) {
  const hldb::Module *const bottom = getModule("bottom");
  ASSERT_NE(bottom, nullptr);
  ASSERT_NE(bottom->getParameters(), nullptr);
  ASSERT_EQ(bottom->getParameters()->size(), 4u);
  const std::string_view expected[] = {"TP1", "SIZE", "TP2", "PPP"};
  for (size_t i = 0; i < 4; ++i) {
    const hldb::Any *const p = bottom->getParameters()->at(i);
    std::string_view name;
    if (const hldb::Parameter *const vp = any_cast<hldb::Parameter>(p)) name = vp->getName();
    if (const hldb::TypeParameter *const tp = any_cast<hldb::TypeParameter>(p)) name = tp->getName();
    EXPECT_EQ(name, expected[i]) << "23.10.2.1: position " << i << " follows the order of declaration";
  }
}

TEST_F(ParamIndexTest, BottomDefaultsAreLogicTenIntAnd1150) {
  const hldb::Module *const bottom = getModule("bottom");
  ASSERT_NE(bottom, nullptr);
  const hldb::Typespec *const tp1 = getRhsType(getParamAssign(bottom, "TP1"));
  ASSERT_NE(tp1, nullptr) << "TP1 has a default type";
  EXPECT_NE(any_cast<hldb::LogicTypespec>(tp1), nullptr) << "TP1 defaults to logic";
  const hldb::ParamAssign *const size = getParamAssign(bottom, "SIZE");
  ASSERT_NE(size, nullptr);
  ASSERT_NE(size->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(size->getRhs<hldb::Constant>()->getDecompile(), "10");
  const hldb::Typespec *const tp2 = getRhsType(getParamAssign(bottom, "TP2"));
  ASSERT_NE(tp2, nullptr) << "TP2 has a default type";
  EXPECT_NE(any_cast<hldb::IntTypespec>(tp2), nullptr) << "TP2 defaults to int";
  const hldb::ParamAssign *const ppp = getParamAssign(bottom, "PPP");
  ASSERT_NE(ppp, nullptr);
  ASSERT_NE(ppp->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(ppp->getRhs<hldb::Constant>()->getDecompile(), "1150");
}

TEST_F(ParamIndexTest, BottomVariablesUseItsOwnParameters) {
  const hldb::Module *const bottom = getModule("bottom");
  ASSERT_NE(bottom, nullptr);
  ASSERT_NE(bottom->getVariables(), nullptr);
  EXPECT_EQ(bottom->getVariables()->size(), 3u);
  const hldb::Variable *const data1 = getVariable(bottom, "DATA1");
  ASSERT_NE(data1, nullptr);
  ASSERT_NE(data1->getTypespec(), nullptr);
  ASSERT_NE(getTypeParam(bottom, "TP1"), nullptr);
  EXPECT_EQ(data1->getTypespec()->getActual(), getTypeParam(bottom, "TP1")) << "6.20.3: DATA1 is of type TP1";
  const hldb::Variable *const data2 = getVariable(bottom, "DATA2");
  ASSERT_NE(data2, nullptr);
  ASSERT_NE(data2->getTypespec(), nullptr);
  ASSERT_NE(getTypeParam(bottom, "TP2"), nullptr);
  EXPECT_EQ(data2->getTypespec()->getActual(), getTypeParam(bottom, "TP2")) << "6.20.3: DATA2 is of type TP2";
  const hldb::Variable *const a = getVariable(bottom, "a");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = a->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'a' is declared 'logic [SIZE:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::RefObj *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::RefObj>();
  ASSERT_NE(left, nullptr) << "unreduced, the left bound is the name 'SIZE'";
  EXPECT_EQ(left->getName(), "SIZE");
  EXPECT_EQ(left->getActual(), getParam(bottom, "SIZE")) << "'SIZE' is bottom's own parameter";
  const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// modules top and mid
// ---------------------------------------------------------------------------

TEST_F(ParamIndexTest, TopDeclaresTypeParameterTptopAndDatatop) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  const hldb::TypeParameter *const tptop = getTypeParam(top, "TPTOP");
  ASSERT_NE(tptop, nullptr) << "6.20.3: 'TPTOP' is a type parameter";
  EXPECT_FALSE(tptop->getLocalParam()) << "6.20.1: top has no parameter port list, so TPTOP is overridable";
  const hldb::Typespec *const def = getRhsType(getParamAssign(top, "TPTOP"));
  ASSERT_NE(def, nullptr) << "TPTOP has a default type";
  EXPECT_NE(any_cast<hldb::IntTypespec>(def), nullptr) << "TPTOP defaults to int";
  const hldb::Variable *const data = getVariable(top, "DATATOP");
  ASSERT_NE(data, nullptr);
  ASSERT_NE(data->getTypespec(), nullptr);
  EXPECT_EQ(data->getTypespec()->getActual(), tptop) << "6.20.3: DATATOP is of type TPTOP";
}

TEST_F(ParamIndexTest, MidDeclaresTypeParameterTp0AndData0) {
  const hldb::Module *const mid = getModule("mid");
  ASSERT_NE(mid, nullptr) << "module 'mid' not found";
  const hldb::TypeParameter *const tp0 = getTypeParam(mid, "TP0");
  ASSERT_NE(tp0, nullptr) << "6.20.3: 'TP0' is a type parameter";
  EXPECT_FALSE(tp0->getLocalParam()) << "6.20.4: TP0 is a parameter port";
  const hldb::Typespec *const def = getRhsType(getParamAssign(mid, "TP0"));
  ASSERT_NE(def, nullptr) << "TP0 has a default type";
  EXPECT_NE(any_cast<hldb::LogicTypespec>(def), nullptr) << "TP0 defaults to logic";
  const hldb::Variable *const data = getVariable(mid, "DATA0");
  ASSERT_NE(data, nullptr);
  ASSERT_NE(data->getTypespec(), nullptr);
  EXPECT_EQ(data->getTypespec()->getActual(), tp0) << "6.20.3: DATA0 is of type TP0";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (type
// parameter names in overrides not bound) in the file header.
TEST_F(ParamIndexTest, U0OverridesTp0WithTptop) {
  const hldb::RefInstance *const u0 = getInstance("top", "u0");
  ASSERT_NE(u0, nullptr) << "instance 'u0' not found";
  ASSERT_NE(u0->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = u0->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("mid"));
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(u0);
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 1u);
  EXPECT_EQ(getLhsName(overrides->at(0)), "TP0");
  EXPECT_EQ(getRhsType(overrides->at(0)), getTypeParam(getModule("top"), "TPTOP"))
      << "23.10.2.2: '.TP0(TPTOP)' passes top's type parameter";
}

TEST_F(ParamIndexTest, MidInstantiatesBottomFourTimes) {
  const hldb::Module *const mid = getModule("mid");
  ASSERT_NE(mid, nullptr);
  ASSERT_NE(mid->getRefInstances(), nullptr);
  EXPECT_EQ(mid->getRefInstances()->size(), 4u);
  for (std::string_view name : {"u1", "u2", "u3", "u4"}) {
    const hldb::RefInstance *const inst = getInstance("mid", name);
    ASSERT_NE(inst, nullptr) << "instance '" << name << "' not found";
    ASSERT_NE(inst->getTypespec(), nullptr);
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    ASSERT_NE(mt, nullptr);
    EXPECT_EQ(mt->getModule(), getModule("bottom")) << name << " instantiates bottom";
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (type
// parameter names in overrides not bound) in the file header.
TEST_F(ParamIndexTest, U1OverridesTp1WithTp0) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("mid", "u1"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 1u);
  EXPECT_EQ(getLhsName(overrides->at(0)), "TP1");
  EXPECT_EQ(getRhsType(overrides->at(0)), getTypeParam(getModule("mid"), "TP0"))
      << "23.10.2.2: '.TP1(TP0)' passes mid's type parameter";
}

TEST_F(ParamIndexTest, U2OverridesTp1WithIntAndSizeWithTwenty) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("mid", "u2"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 2u);
  EXPECT_EQ(getLhsName(overrides->at(0)), "TP1");
  EXPECT_NE(any_cast<hldb::IntTypespec>(getRhsType(overrides->at(0))), nullptr) << "'.TP1(int)'";
  EXPECT_EQ(getLhsName(overrides->at(1)), "SIZE");
  const hldb::Constant *const c = overrides->at(1)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "20");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (type
// parameter names in overrides not bound) in the file header.
TEST_F(ParamIndexTest, NamedOverridesBindToTargetParameters) {
  struct NamedOverride {
    std::string_view m_module;
    std::string_view m_instance;
    size_t m_index;
    std::string_view m_target;
    std::string_view m_param;
  };
  const NamedOverride overrides[] = {{"top", "u0", 0, "mid", "TP0"},
                                     {"mid", "u1", 0, "bottom", "TP1"},
                                     {"mid", "u2", 0, "bottom", "TP1"},
                                     {"mid", "u2", 1, "bottom", "SIZE"}};
  for (const NamedOverride &o : overrides) {
    SCOPED_TRACE(testing::Message() << o.m_instance << " '." << o.m_param << "(...)'");
    const std::vector<hldb::ParamAssign *> *const pas = getOverrides(getInstance(o.m_module, o.m_instance));
    ASSERT_NE(pas, nullptr);
    ASSERT_GT(pas->size(), o.m_index);
    const hldb::Module *const target = getModule(o.m_target);
    ASSERT_NE(target, nullptr);
    const hldb::Any *const declared = (getTypeParam(target, o.m_param) != nullptr)
                                          ? static_cast<const hldb::Any *>(getTypeParam(target, o.m_param))
                                          : static_cast<const hldb::Any *>(getParam(target, o.m_param));
    ASSERT_NE(declared, nullptr);
    EXPECT_EQ(getLhsActual(pas->at(o.m_index)), declared)
        << "23.10.2.2: the name in '." << o.m_param << "(...)' is " << o.m_target << "'s parameter";
  }
}

TEST_F(ParamIndexTest, U3OrderedValuesFollowDeclarationOrder) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("mid", "u3"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 4u) << "'#(int, 1120, int, 1130)' writes 4 values";
  const std::string_view names[] = {"TP1", "SIZE", "TP2", "PPP"};
  for (size_t i = 0; i < 4; ++i) {
    const std::string_view lhs = getLhsName(overrides->at(i));
    if (!lhs.empty())
      EXPECT_EQ(lhs, names[i]) << "23.10.2.1: value " << i << " goes to the parameter at position " << i;
  }
  EXPECT_NE(any_cast<hldb::IntTypespec>(getRhsType(overrides->at(0))), nullptr) << "value 0 is the type int";
  ASSERT_NE(overrides->at(1)->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(overrides->at(1)->getRhs<hldb::Constant>()->getDecompile(), "1120");
  EXPECT_NE(any_cast<hldb::IntTypespec>(getRhsType(overrides->at(2))), nullptr) << "value 2 is the type int";
  ASSERT_NE(overrides->at(3)->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(overrides->at(3)->getRhs<hldb::Constant>()->getDecompile(), "1130");
}

TEST_F(ParamIndexTest, U4LastOrderedValueIsUnboundUndef) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("mid", "u4"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 4u) << "'#(int, 1140, int, UNDEF)' writes 4 values";
  ASSERT_NE(overrides->at(1)->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(overrides->at(1)->getRhs<hldb::Constant>()->getDecompile(), "1140");
  const hldb::RefObj *const undef = overrides->at(3)->getRhs<hldb::RefObj>();
  ASSERT_NE(undef, nullptr) << "value 3 is the name 'UNDEF'";
  EXPECT_EQ(undef->getName(), "UNDEF");
  EXPECT_EQ(undef->getActual(), nullptr) << "UNDEF is not declared anywhere in the file";
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(ParamIndexTest, TypeParametersResolveToInt) {
  if (m_design->getElaborated()) {
    ExpectElabInt(getElabInstance({}), "DATATOP", "6.20.3: TPTOP = int");
    ExpectElabInt(getElabInstance({"u0"}), "DATA0", "23.10.2.2: TP0 = TPTOP = int");
    ExpectElabInt(getElabInstance({"u0", "u1"}), "DATA1", "23.10.2.2: TP1 = TP0 = int");
  }
}

TEST_F(ParamIndexTest, SizeReachesEachInstance) {
  if (m_design->getElaborated()) {
    ExpectElabARange(getElabInstance({"u0", "u1"}), 10u, "u1 keeps the default SIZE = 10");
    ExpectElabARange(getElabInstance({"u0", "u2"}), 20u, "23.10.2.2: '.SIZE(20)'");
    ExpectElabARange(getElabInstance({"u0", "u3"}), 1120u, "23.10.2.1: the second ordered value goes to SIZE");
  }
}

TEST_F(ParamIndexTest, U3PppIsFourthOrderedValue) {
  if (m_design->getElaborated()) {
    const hldb::Module *const u3 = getElabInstance({"u0", "u3"});
    ASSERT_NE(u3, nullptr) << "the instance u0.u3 is elaborated";
    const hldb::ParamAssign *const pa = getParamAssign(u3, "PPP");
    ASSERT_NE(pa, nullptr);
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr);
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, 1130u) << "23.10.2.1: the fourth ordered value goes to PPP";
  }
}

TEST_F(ParamIndexTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: mid and bottom are instantiated";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (undeclared
// name only warned) in the file header.
TEST_F(ParamIndexTest, UndeclaredUndefIsAnError) {
  EXPECT_TRUE(reportsErrorNaming("UNDEF"))
      << "6.20.2: a value parameter can only be set to an expression of literals, value "
         "parameters or local parameters, genvars, enumerated names, or a constant function of "
         "these; UNDEF is none of them";
}

TEST_F(ParamIndexTest, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
