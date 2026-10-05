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

// Tests for tests/MaskNeg/dut.sv (tags: MaskNeg)
//
//   module dut;
//      parameter logic [1:0] RESVAL;
//   endmodule
//
//   module top;
//      dut #(
//         .RESVAL(~(2'h0))
//      ) u_dut();
//   endmodule // top
//
// MaskNeg.hlc compiles at "-d db -d ast" (no elaboration), so this file
// checks the compile-time (unelaborated) shape of the parameter
// declaration and of the named parameter override whose value is a
// bitwise negation ("mask negation") of a sized hex literal.
//
// What is checked:
//   - modules "dut" and "top" exist
//   - Sec 6.20.1/6.20.2: "dut" declares a (non-local) value parameter
//     "RESVAL" whose explicit data type is "logic [1:0]" (a LogicTypespec
//     with one packed range [1:0])
//   - Sec 6.20.1: the declaration in "dut" omits the default value, so the
//     RESVAL ParamAssign (if one is modeled) carries no RHS
//   - Sec 23.3.2/23.10.2.2: "top" contains the instance "u_dut" of module
//     "dut"; the instance typespec resolves to a ModuleTypespec for "dut"
//   - Sec 23.10.2.2 (parameter value assignment by name): the override is a
//     single ParamAssign, connected by name and marked overridden, whose LHS
//     binds to dut's Parameter "RESVAL"
//   - Sec 11.4.8 (bitwise operators, unary ~) / Sec 5.7.1 (integer literal
//     constants): the override RHS is Operation(vpiBitNegOp) with exactly
//     one operand, the 2-bit hexadecimal Constant "2'h0" (parentheses do
//     not create a node of their own)
//   - Sec 23.10: since every instantiation provides an override, no
//     missing-override or bind failure is reported for "RESVAL"
//
// What is NOT checked and why:
//   - The evaluated override value (~2'h0 in the 2-bit context of RESVAL is
//     2'b11): constant evaluation of overrides is an elaboration result and
//     this compile stops before elaboration.
//   - Sec 6.20.1 footnote 22 of Syntax 6-6: "It shall be legal to omit the
//     constant_param_expression from a param_assignment ... only within a
//     parameter_port_list." "parameter logic [1:0] RESVAL;" omits the
//     default in a module-body declaration, so it is strictly illegal.
//     ErrorDefinition.h has no diagnostic for this rule
//     (COMP_LOCALPARAM_NO_VALUE covers localparam only), so there is no
//     ErrorType to pass to findError(); this is noted here instead.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MaskNegTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MaskNeg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getResval() {
    const hldb::Module *const dut = getModule("dut");
    if (dut == nullptr || dut->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *dut->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == "RESVAL") return param;
    }
    return nullptr;
  }

  static const hldb::RefInstance *getUDut() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr || top->getRefInstances() == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_dut", top->getRefInstances());
  }

  static const hldb::ModuleTypespec *getUDutTypespec() {
    const hldb::RefInstance *const inst = getUDut();
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    return inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  }

  static const hldb::ParamAssign *getOverride() {
    const hldb::ModuleTypespec *const mt = getUDutTypespec();
    if (mt == nullptr || mt->getParamAssigns() == nullptr || mt->getParamAssigns()->size() != 1) return nullptr;
    return mt->getParamAssigns()->at(0);
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(MaskNegTest, ModulesExist) {
  EXPECT_NE(getModule("dut"), nullptr) << "module 'dut' not found";
  EXPECT_NE(getModule("top"), nullptr) << "module 'top' not found";
}

// ---------------------------------------------------------------------------
// Sec 6.20.2: parameter logic [1:0] RESVAL;
// ---------------------------------------------------------------------------

TEST_F(MaskNegTest, ResvalIsNonLocalParameter) {
  const hldb::Parameter *const resval = getResval();
  ASSERT_NE(resval, nullptr) << "'parameter logic [1:0] RESVAL' not found in 'dut'";
  EXPECT_FALSE(resval->getLocalParam()) << "declared with 'parameter', not 'localparam'";
}

TEST_F(MaskNegTest, ResvalTypeIsLogicOneDownToZero) {
  const hldb::Parameter *const resval = getResval();
  ASSERT_NE(resval, nullptr);
  ASSERT_NE(resval->getTypespec(), nullptr) << "RESVAL has an explicit data type";
  const hldb::LogicTypespec *const lt = resval->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "RESVAL's data type is 'logic [1:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u) << "exactly one packed dimension";
  const hldb::Range *const r = lt->getRanges()->at(0);
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), "1");
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(MaskNegTest, ResvalDeclarationHasNoDefaultValue) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(getResval(), nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("RESVAL", hldb::getParamAssigns(dut));
  if (pa != nullptr) {
    EXPECT_EQ(pa->getRhs(), nullptr) << "Sec 6.20.1: 'parameter logic [1:0] RESVAL;' specifies no default value";
  }
}

// ---------------------------------------------------------------------------
// Sec 23.3.2: dut #(...) u_dut();
// ---------------------------------------------------------------------------

TEST_F(MaskNegTest, TopInstantiatesDutAsUDut) {
  const hldb::RefInstance *const inst = getUDut();
  ASSERT_NE(inst, nullptr) << "instance 'u_dut' not found in 'top'";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = getUDutTypespec();
  ASSERT_NE(mt, nullptr) << "'u_dut's typespec must resolve to a ModuleTypespec";
  EXPECT_EQ(mt->getDefName(), "dut");
  EXPECT_EQ(mt->getModule(), getModule("dut")) << "'u_dut' is an instance of module 'dut'";
}

// ---------------------------------------------------------------------------
// Sec 23.10.2.2: .RESVAL(~(2'h0))
// ---------------------------------------------------------------------------

TEST_F(MaskNegTest, OverrideIsNamedAndBindsToResval) {
  const hldb::ModuleTypespec *const mt = getUDutTypespec();
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(mt->getParamAssigns(), nullptr) << "'#(.RESVAL(...))' override missing";
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u) << "exactly one parameter override";
  const hldb::ParamAssign *const pa = getOverride();
  ASSERT_NE(pa, nullptr);
  EXPECT_TRUE(pa->getConnByName()) << "'.RESVAL(...)' is a named parameter assignment";
  EXPECT_TRUE(pa->getOverridden());

  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "RESVAL");
  ASSERT_NE(getResval(), nullptr);
  EXPECT_EQ(lhs->getActual(), getResval()) << "override must bind to dut's Parameter 'RESVAL'";
}

TEST_F(MaskNegTest, OverrideValueIsBitwiseNegationOfTwoBitHexZero) {
  const hldb::ParamAssign *const pa = getOverride();
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs(), nullptr);
  const hldb::Operation *const op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'~(2'h0)' must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiBitNegOp) << "Sec 11.4.8: unary '~' is bitwise negation";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u) << "unary operator takes exactly one operand";

  const hldb::Constant *const c = any_cast<hldb::Constant>(op->getOperands()->at(0));
  ASSERT_NE(c, nullptr) << "operand '(2'h0)' must be a Constant (parentheses add no node)";
  EXPECT_EQ(c->getConstType(), vpiHexConst) << "Sec 5.7.1: 'h base is hexadecimal";
  EXPECT_EQ(c->getSize(), 2) << "Sec 5.7.1: size prefix '2' makes it a 2-bit constant";
  EXPECT_EQ(c->getDecompile(), "2'h0");
}

TEST_F(MaskNegTest, NoMissingOverrideOrBindErrorForResval) {
  EXPECT_EQ(findError(ErrorDefinition::ELAB_MISSING_PARAMETER_OVERRIDE, "RESVAL"), nullptr)
      << "Sec 23.10: the only instantiation of 'dut' does override RESVAL";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "RESVAL"), nullptr)
      << "'.RESVAL' names a parameter declared in 'dut'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
