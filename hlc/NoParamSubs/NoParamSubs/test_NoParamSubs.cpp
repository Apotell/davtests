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

// Tests for tests/NoParamSubs/dut.sv (compiled with
// --disable-feature=parametersubstitution, i.e. parameter references must be
// kept as references rather than replaced by their values):
//
//   module dut(output logic [1:0] a);
//      parameter logic [1:0] P = 0;
//      assign a = P;
//   endmodule // dut
//
//   module top(output logic [1:0] o);
//      parameter logic [1:0] X = '{0, 1};
//      dut #(
//         .P(~X)
//      ) u_dut(
//         .a(o)
//      );
//   endmodule // top
//
// What to check and why (IEEE 1800-2023):
//   - Sec 23.2.2.3: "output logic [1:0] a" is an ANSI output port with an
//     explicit data type, so "the port kind shall default to variable".
//     Same for top's "o".
//   - Sec 6.20.2: "parameter logic [1:0] P = 0" -- typed and ranged, so P is
//     logic [1:0] (unsigned); P and X are overridable parameters (neither
//     module has a parameter_port_list, Sec 6.20.1).
//   - Sec 10.9.1: "'{0, 1}" is a positional assignment pattern with two
//     elements, matching the two elements of the [1:0] packed dimension.
//   - Sec 23.10.2.2: ".P(~X)" is a parameter value assignment by name to
//     parameter P of the instantiated module dut; the value expression is
//     the bitwise negation (Sec 11.4.8, vpiBitNegOp) of X, X resolving to
//     top's parameter X.
//   - Sec 23.3.2.2: ".a(o)" connects instance port a to top's o by name.
//   - "assign a = P;" (Sec 10.3.2) keeps P as a reference to the parameter
//     (parameter substitution disabled).
//
// What is NOT checked and why:
//   - The elaborated value of u_dut.P (2'b10): the .hlc does not request
//     elaboration.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NoParamSubsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NoParamSubs.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(std::string_view mod, std::string_view name) {
    const hldb::Module *const m = getModule(mod);
    if (m == nullptr || m->getParameters() == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, m->getParameters());
  }

  static const hldb::ParamAssign *getOnlyParamAssign(std::string_view mod) {
    const hldb::Module *const m = getModule(mod);
    if (m == nullptr || m->getParamAssigns() == nullptr || m->getParamAssigns()->size() != 1) return nullptr;
    return m->getParamAssigns()->at(0);
  }

  static const hldb::RefInstance *getUDut() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr || top->getRefInstances() == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_dut", top->getRefInstances());
  }

  static void checkLogic1To0(const hldb::RefTypespec *rt) {
    ASSERT_NE(rt, nullptr);
    const hldb::LogicTypespec *const lt = rt->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    EXPECT_FALSE(lt->getSigned());
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Constant *const l = any_cast<hldb::Constant>(lt->getRanges()->at(0)->getLeftExpr());
    const hldb::Constant *const r = any_cast<hldb::Constant>(lt->getRanges()->at(0)->getRightExpr());
    ASSERT_NE(l, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(l->getDecompile(), std::string_view("1"));
    EXPECT_EQ(r->getDecompile(), std::string_view("0"));
  }

  static void checkOutputVarPort(std::string_view mod, std::string_view name) {
    const hldb::Module *const m = getModule(mod);
    ASSERT_NE(m, nullptr);
    ASSERT_NE(m->getPorts(), nullptr);
    const hldb::Port *const p = hldb::findByName<hldb::Port>(name, m->getPorts());
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getDirection(), vpiOutput);
    checkLogic1To0(p->getTypespec());
    ASSERT_NE(m->getVariables(), nullptr) << "Sec 23.2.2.3: explicit data type output -> variable";
    EXPECT_NE(hldb::findByName<hldb::Variable>(name, m->getVariables()), nullptr);
    if (m->getNets() != nullptr) {
      EXPECT_EQ(hldb::findByName<hldb::Net>(name, m->getNets()), nullptr);
    }
  }
};

TEST_F(NoParamSubsTest, ModulesExist) {
  EXPECT_NE(getModule("dut"), nullptr);
  EXPECT_NE(getModule("top"), nullptr);
}

TEST_F(NoParamSubsTest, DutPortAIsOutputLogicVariable) { checkOutputVarPort("dut", "a"); }

TEST_F(NoParamSubsTest, TopPortOIsOutputLogicVariable) { checkOutputVarPort("top", "o"); }

// ---------------------------------------------------------------------------
// parameter logic [1:0] P = 0;  /  parameter logic [1:0] X = '{0, 1};
// ---------------------------------------------------------------------------

TEST_F(NoParamSubsTest, PIsOverridableLogic1To0) {
  const hldb::Parameter *const p = getParam("dut", "P");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam());
  checkLogic1To0(p->getTypespec());
}

TEST_F(NoParamSubsTest, PDefaultIsZero) {
  const hldb::ParamAssign *const pa = getOnlyParamAssign("dut");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), std::string_view("0"));
}

TEST_F(NoParamSubsTest, XIsOverridableLogic1To0) {
  const hldb::Parameter *const x = getParam("top", "X");
  ASSERT_NE(x, nullptr);
  EXPECT_FALSE(x->getLocalParam());
  checkLogic1To0(x->getTypespec());
}

TEST_F(NoParamSubsTest, XDefaultIsTwoElementAssignmentPattern) {
  const hldb::ParamAssign *const pa = getOnlyParamAssign("top");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const op = any_cast<hldb::Operation>(pa->getRhs());
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::Constant *const e0 = any_cast<hldb::Constant>(op->getOperands()->at(0));
  const hldb::Constant *const e1 = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(e0, nullptr);
  ASSERT_NE(e1, nullptr);
  EXPECT_EQ(e0->getDecompile(), std::string_view("0"));
  EXPECT_EQ(e1->getDecompile(), std::string_view("1"));
}

// ---------------------------------------------------------------------------
// assign a = P;
// ---------------------------------------------------------------------------

TEST_F(NoParamSubsTest, AssignAFromParameterReference) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getContAssigns(), nullptr);
  ASSERT_EQ(dut->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = dut->getContAssigns()->at(0);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(ca->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("a"));
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(ca->getRhs());
  ASSERT_NE(rhs, nullptr) << "with parameter substitution disabled, 'P' stays a reference";
  EXPECT_EQ(rhs->getName(), std::string_view("P"));
  ASSERT_NE(rhs->getActual(), nullptr);
  EXPECT_EQ(rhs->getActual(), getParam("dut", "P"));
}

// ---------------------------------------------------------------------------
// dut #(.P(~X)) u_dut(.a(o));
// ---------------------------------------------------------------------------

TEST_F(NoParamSubsTest, TopInstantiatesDutAsUDut) {
  const hldb::RefInstance *const ri = getUDut();
  ASSERT_NE(ri, nullptr);
  ASSERT_NE(ri->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mts = ri->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mts, nullptr);
  EXPECT_EQ(mts->getModule(), getModule("dut"));
}

TEST_F(NoParamSubsTest, OverrideOfPByNameIsBitNegOfX) {
  const hldb::RefInstance *const ri = getUDut();
  ASSERT_NE(ri, nullptr);
  ASSERT_NE(ri->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mts = ri->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mts, nullptr);
  ASSERT_NE(mts->getParamAssigns(), nullptr);
  ASSERT_EQ(mts->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = mts->getParamAssigns()->at(0);
  EXPECT_TRUE(pa->getConnByName()) << "Sec 23.10.2.2: '.P(...)' is assignment by name";
  EXPECT_TRUE(pa->getOverridden());
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(pa->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("P"));
  EXPECT_EQ(lhs->getActual(), getParam("dut", "P")) << "name must be that of the instantiated module's parameter";
  const hldb::Operation *const neg = any_cast<hldb::Operation>(pa->getRhs());
  ASSERT_NE(neg, nullptr);
  EXPECT_EQ(neg->getOpType(), vpiBitNegOp);
  ASSERT_NE(neg->getOperands(), nullptr);
  ASSERT_EQ(neg->getOperands()->size(), 1u);
  const hldb::RefObj *const x = any_cast<hldb::RefObj>(neg->getOperands()->at(0));
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getName(), std::string_view("X"));
  EXPECT_EQ(x->getActual(), getParam("top", "X"));
}

TEST_F(NoParamSubsTest, PortAConnectedToO) {
  const hldb::RefInstance *const ri = getUDut();
  ASSERT_NE(ri, nullptr);
  ASSERT_NE(ri->getPorts(), nullptr);
  ASSERT_EQ(ri->getPorts()->size(), 1u);
  const hldb::Port *const p = any_cast<hldb::Port>(ri->getPorts()->at(0));
  ASSERT_NE(p, nullptr);
  const hldb::RefObj *const low = any_cast<hldb::RefObj>(p->getLowConn());
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getName(), std::string_view("a"));
  const hldb::RefObj *const high = any_cast<hldb::RefObj>(p->getHighConn());
  ASSERT_NE(high, nullptr);
  EXPECT_EQ(high->getName(), std::string_view("o"));
  ASSERT_NE(high->getActual(), nullptr);
  EXPECT_EQ(high->getActual()->getAnyType(), hldb::AnyType::Variable);
}

TEST_F(NoParamSubsTest, NoBindingErrors) {
  for (const char *const sym : {"P", "X", "a", "o", "dut"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, sym), nullptr) << sym;
  }
  EXPECT_EQ(findError(ErrorDefinition::ELAB_UNKNOWN_PARAMETER_OVERRIDE, "P"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
