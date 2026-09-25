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

// Validates the UHDM graph for an enumeration constant used inside an
// assignment-pattern parameter default value, propagated as a parameter
// override through two levels of module instantiation, and finally cast
// to 'int' -- a chain that requires elaboration-time (constant)
// evaluation of the enum constant, per IEEE 1800-2023 Sec 6.20.4
// ("Parameterized modules") and Sec 6.19 ("Enumerations").
//
//   module prim_subreg;
//     parameter logic [4:0] RESVAL = '0;
//     int a = int'(RESVAL);
//   endmodule
//
//   module prim_subreg_shadow;
//     typedef struct packed { logic [2:0] a; logic [1:0] b; } struct_t;
//     typedef enum logic [2:0] { ENUM_ITEM = 3'b000 } enum_t;
//     parameter struct_t RESVAL = '{ a: ENUM_ITEM, b: '1 };
//     prim_subreg #(.RESVAL(RESVAL)) staged_reg ();
//   endmodule
//
//   module top;
//     typedef struct packed { logic [1:0] a; logic [2:0] b; } struct_t;
//     typedef enum logic [1:0] { ENUM_ITEM = 2'b11 } enum_t;
//     parameter struct_t CTRL_RESET = '{ a: ENUM_ITEM, b: '0 };
//     prim_subreg_shadow #(.RESVAL(CTRL_RESET)) u_ctrl_reg_shadowed ();
//   endmodule
//
// What to check and why:
//   - Sec 6.19.1: each 'typedef enum ... {ENUM_ITEM = ...} enum_t;'
//     declares a named enumeration type with one constant "ENUM_ITEM".
//   - Sec 5.10 ("Assignment pattern expressions"): "'{a: ENUM_ITEM, b: '1}"
//     is a structure assignment pattern whose "a" key is bound to a
//     reference to the enum constant "ENUM_ITEM" -- represented in UHDM
//     as an Operation (vpiAssignmentPatternOp) whose operands are
//     TaggedPattern entries; the "a" entry's pattern is a RefObj that
//     must resolve to the module-local EnumConst "ENUM_ITEM" (not a
//     Constant literal -- this is the enum-constant-in-elaboration-context
//     case under test).
//   - Sec 23.3 ("Parameter value assignment"): '#(.RESVAL(RESVAL))' and
//     '#(.RESVAL(CTRL_RESET))' are by-name parameter overrides that carry
//     the assignment-pattern value (and transitively the enum constant it
//     references) down through the instantiation hierarchy.
//   - Sec 6.24.1 ("Cast operator"): "int'(RESVAL)" casts the (ultimately
//     enum-constant-derived) packed value to 'int', requiring the whole
//     chain to be evaluable at elaboration time.
//
// Checked:
//   - all three modules ("prim_subreg", "prim_subreg_shadow", "top") exist
//   - "top" owns TypedefTypespec "enum_t" -> EnumTypespec (base logic[1:0])
//     with 1 const "ENUM_ITEM" = 2'b11 (vpiBinaryConst)
//   - "top" declares parameter "CTRL_RESET" (not a localparam), whose
//     default ParamAssign RHS is an Operation (vpiAssignmentPatternOp)
//     with 2 TaggedPattern operands: "a" -> RefObj "ENUM_ITEM" resolving
//     to the EnumConst above, "b" -> present (non-null) pattern
//   - "top" instantiates "prim_subreg_shadow" as "u_ctrl_reg_shadowed"
//     with a by-name, overriding ParamAssign for "RESVAL" whose RHS is a
//     RefObj "CTRL_RESET" resolving to the Parameter above
//   - "prim_subreg_shadow" owns its own, distinct TypedefTypespec
//     "enum_t" -> EnumTypespec (base logic[2:0]) with 1 const
//     "ENUM_ITEM" = 3'b000
//   - "prim_subreg_shadow" declares parameter "RESVAL" whose default
//     ParamAssign RHS is an assignment-pattern Operation whose "a" entry
//     is a RefObj "ENUM_ITEM" resolving to ITS OWN local EnumConst (not
//     the one in "top" -- each module scope has an independently
//     elaborated enum constant of the same name)
//   - "prim_subreg_shadow" instantiates "prim_subreg" as "staged_reg"
//     with a by-name, overriding ParamAssign for "RESVAL" whose RHS is a
//     RefObj "RESVAL" resolving to ITS OWN Parameter "RESVAL"
//   - "prim_subreg" declares parameter "RESVAL" (logic[4:0], default '0)
//   - "prim_subreg" declares variable "a" (int) whose initial value is a
//     cast Operation (vpiCastOp) to 'int' over a RefObj "RESVAL"
//     resolving to the Parameter above
//   - compiler reports zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumConstElabTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EnumConstElab.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  template <typename ScopeT>
  static const hldb::ParamAssign *findParamAssign(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(scope));
  }

  static const hldb::Enum *getModuleEnum(const hldb::Module *mod) {
    if (mod == nullptr) return nullptr;
    const hldb::TypedefTypespec *const tt = hldb::findByName<hldb::TypedefTypespec>("enum_t", mod->getTypespecs());
    if (tt == nullptr || tt->getTypedef() == nullptr || tt->getTypedef()->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const enumTs = tt->getTypedef()->getAlias()->getActual<hldb::EnumTypespec>();
    if (enumTs == nullptr) return nullptr;
    return enumTs->getEnum();
  }
};

// ---------------------------------------------------------------------------
// Existence
// ---------------------------------------------------------------------------

TEST_F(EnumConstElabTest, AllThreeModulesExist) {
  EXPECT_NE(getModule("prim_subreg"), nullptr) << "module 'prim_subreg' not found";
  EXPECT_NE(getModule("prim_subreg_shadow"), nullptr) << "module 'prim_subreg_shadow' not found";
  EXPECT_NE(getModule("top"), nullptr) << "module 'top' not found";
}

// ---------------------------------------------------------------------------
// top::enum_t -- ENUM_ITEM = 2'b11
// ---------------------------------------------------------------------------

TEST_F(EnumConstElabTest, TopEnumHasOneConstEnumItem) {
  const hldb::Enum *const e = getModuleEnum(getModule("top"));
  ASSERT_NE(e, nullptr);
  const hldb::RefTypespec *const base = e->getBaseTypespec();
  ASSERT_NE(base, nullptr);
  EXPECT_NE(base->getActual<hldb::LogicTypespec>(), nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 1u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view("ENUM_ITEM"));
}

TEST_F(EnumConstElabTest, TopEnumItemValueIsBinary2b11) {
  const hldb::Enum *const e = getModuleEnum(getModule("top"));
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 1u);
  const hldb::Constant *const val = e->getEnumConsts()->at(0)->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiBinaryConst);
  EXPECT_EQ(val->getDecompile(), std::string_view("2'b11"));
}

// ---------------------------------------------------------------------------
// top::CTRL_RESET = '{a: ENUM_ITEM, b: '0} -- assignment pattern using the
// enum constant declared just above
// ---------------------------------------------------------------------------

TEST_F(EnumConstElabTest, TopHasCtrlResetParameterNotLocalParam) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  const hldb::Parameter *ctrlReset = nullptr;
  for (const hldb::Any *const p : *top->getParameters()) {
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
    if (param != nullptr && param->getName() == "CTRL_RESET") {
      ctrlReset = param;
      break;
    }
  }
  ASSERT_NE(ctrlReset, nullptr) << "'parameter struct_t CTRL_RESET' not found on module 'top'";
  EXPECT_FALSE(ctrlReset->getLocalParam());
}

TEST_F(EnumConstElabTest, TopCtrlResetDefaultIsAssignmentPatternWithTwoOperands) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(top, "CTRL_RESET");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'CTRL_RESET' not found";
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'{a: ENUM_ITEM, b: '0}' RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 2u);
}

TEST_F(EnumConstElabTest, TopCtrlResetMemberAReferencesEnumItem) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(top, "CTRL_RESET");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(rhs->getOperands()->at(0));
  ASSERT_NE(tp, nullptr) << "'a: ENUM_ITEM' must be represented as a TaggedPattern";
  const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr);
  EXPECT_EQ(tag->getName(), std::string_view("a"));

  const hldb::RefObj *const pattern = tp->getPattern<hldb::RefObj>();
  ASSERT_NE(pattern, nullptr) << "'ENUM_ITEM' pattern value must be a RefObj to the enum constant";
  EXPECT_EQ(pattern->getName(), std::string_view("ENUM_ITEM"));
  EXPECT_NE(pattern->getActual<hldb::EnumConst>(), nullptr)
      << "'ENUM_ITEM' inside the assignment pattern must resolve to the module-local EnumConst -- this is "
         "the enum-constant elaboration case under test (Sec 6.20.4)";
}

TEST_F(EnumConstElabTest, TopCtrlResetMemberBIsPresent) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(top, "CTRL_RESET");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(rhs->getOperands()->at(1));
  ASSERT_NE(tp, nullptr) << "'b: '0' must be represented as a TaggedPattern";
  const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr);
  EXPECT_EQ(tag->getName(), std::string_view("b"));
  EXPECT_NE(tp->getPattern(), nullptr) << "'b's fill pattern ''0' must be captured";
}

// ---------------------------------------------------------------------------
// top instantiates prim_subreg_shadow, overriding RESVAL with CTRL_RESET
// ---------------------------------------------------------------------------

TEST_F(EnumConstElabTest, TopInstantiatesPrimSubregShadow) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  const hldb::RefInstance *const inst =
      hldb::findByName<hldb::RefInstance>("u_ctrl_reg_shadowed", top->getRefInstances());
  ASSERT_NE(inst, nullptr) << "'prim_subreg_shadow #(...) u_ctrl_reg_shadowed ()' not found in 'top'";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), std::string_view("prim_subreg_shadow"));
}

TEST_F(EnumConstElabTest, UCtrlRegShadowedOverridesResvalWithCtrlReset) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst =
      hldb::findByName<hldb::RefInstance>("u_ctrl_reg_shadowed", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(inst, "RESVAL");
  ASSERT_NE(pa, nullptr) << "'.RESVAL(CTRL_RESET)' override not found";
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'.RESVAL(CTRL_RESET)' RHS must be a RefObj";
  EXPECT_EQ(rhs->getName(), std::string_view("CTRL_RESET"));
  EXPECT_NE(rhs->getActual<hldb::Parameter>(), nullptr);
}

// ---------------------------------------------------------------------------
// prim_subreg_shadow::enum_t -- its OWN ENUM_ITEM = 3'b000
// ---------------------------------------------------------------------------

TEST_F(EnumConstElabTest, ShadowEnumHasOwnConstEnumItem) {
  const hldb::Enum *const e = getModuleEnum(getModule("prim_subreg_shadow"));
  ASSERT_NE(e, nullptr);
  const hldb::RefTypespec *const base = e->getBaseTypespec();
  ASSERT_NE(base, nullptr);
  EXPECT_NE(base->getActual<hldb::LogicTypespec>(), nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 1u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view("ENUM_ITEM"));
  const hldb::Constant *const val = e->getEnumConsts()->at(0)->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiBinaryConst);
  EXPECT_EQ(val->getDecompile(), std::string_view("3'b000"));
}

TEST_F(EnumConstElabTest, ShadowResvalMemberAReferencesItsOwnEnumItem) {
  const hldb::Module *const shadow = getModule("prim_subreg_shadow");
  ASSERT_NE(shadow, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(shadow, "RESVAL");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'prim_subreg_shadow::RESVAL' not found";
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(rhs->getOperands()->at(0));
  ASSERT_NE(tp, nullptr);
  const hldb::RefObj *const pattern = tp->getPattern<hldb::RefObj>();
  ASSERT_NE(pattern, nullptr) << "'ENUM_ITEM' inside 'prim_subreg_shadow::RESVAL' default must be a RefObj";
  EXPECT_EQ(pattern->getName(), std::string_view("ENUM_ITEM"));
  const hldb::EnumConst *const actual = pattern->getActual<hldb::EnumConst>();
  ASSERT_NE(actual, nullptr) << "must resolve to prim_subreg_shadow's OWN local EnumConst, not top's";
}

// ---------------------------------------------------------------------------
// prim_subreg_shadow instantiates prim_subreg, overriding RESVAL with its
// own local RESVAL parameter
// ---------------------------------------------------------------------------

TEST_F(EnumConstElabTest, ShadowInstantiatesPrimSubreg) {
  const hldb::Module *const shadow = getModule("prim_subreg_shadow");
  ASSERT_NE(shadow, nullptr);
  ASSERT_NE(shadow->getRefInstances(), nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("staged_reg", shadow->getRefInstances());
  ASSERT_NE(inst, nullptr) << "'prim_subreg #(...) staged_reg ()' not found in 'prim_subreg_shadow'";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), std::string_view("prim_subreg"));
}

TEST_F(EnumConstElabTest, StagedRegOverridesResvalWithShadowsResval) {
  const hldb::Module *const shadow = getModule("prim_subreg_shadow");
  ASSERT_NE(shadow, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("staged_reg", shadow->getRefInstances());
  ASSERT_NE(inst, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(inst, "RESVAL");
  ASSERT_NE(pa, nullptr) << "'.RESVAL(RESVAL)' override not found";
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("RESVAL"));
  EXPECT_NE(rhs->getActual<hldb::Parameter>(), nullptr);
}

// ---------------------------------------------------------------------------
// prim_subreg: parameter RESVAL, variable a = int'(RESVAL)
// ---------------------------------------------------------------------------

TEST_F(EnumConstElabTest, PrimSubregHasResvalParameter) {
  const hldb::Module *const mod = getModule("prim_subreg");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParameters(), nullptr);
  const hldb::Parameter *resval = nullptr;
  for (const hldb::Any *const p : *mod->getParameters()) {
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
    if (param != nullptr && param->getName() == "RESVAL") {
      resval = param;
      break;
    }
  }
  ASSERT_NE(resval, nullptr) << "'parameter logic [4:0] RESVAL' not found on module 'prim_subreg'";
  EXPECT_FALSE(resval->getLocalParam());
}

TEST_F(EnumConstElabTest, PrimSubregVariableAIsIntCastOfResval) {
  const hldb::Module *const mod = getModule("prim_subreg");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", mod->getVariables());
  ASSERT_NE(a, nullptr) << "'int a = int'(RESVAL);' variable 'a' not found";
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr) << "Sec 6.24.1: 'int'(RESVAL)' initial value must be a cast Operation";
  EXPECT_EQ(castOp->getOpType(), vpiCastOp);
  ASSERT_NE(castOp->getOperands(), nullptr);
  ASSERT_EQ(castOp->getOperands()->size(), 1u);

  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(castOp->getOperands()->at(0));
  ASSERT_NE(operand, nullptr) << "cast operand must be a RefObj to 'RESVAL'";
  EXPECT_EQ(operand->getName(), std::string_view("RESVAL"));
  EXPECT_NE(operand->getActual<hldb::Parameter>(), nullptr)
      << "'RESVAL' inside 'int'(RESVAL)' must resolve to the Parameter -- the tail of the elaboration chain "
         "that began with 'top::CTRL_RESET's ENUM_ITEM reference";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(EnumConstElabTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
