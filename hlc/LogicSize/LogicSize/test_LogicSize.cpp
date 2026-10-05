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

// Tests for tests/LogicSize/dut.sv:
//
//   module dut(output logic a);
//      parameter logic P = 0;
//      assign a = P;
//   endmodule // dut
//
//   module top(output logic o);
//      parameter logic X = 0;
//      dut #(
//         .P(~X)
//      ) u_dut(
//         .a(o)
//      );
//   endmodule // top
//
// LogicSize.hlc compiles at "-d db -d ast" (no "-d inst"), so this file
// checks the unelaborated design: the module definitions, their parameter
// declarations, and the instantiation of 'dut' inside 'top' with its
// parameter override and port connection as written.
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 6.20.2 "Value parameters": 'parameter logic P = 0'
//   declares a value parameter whose type is the explicitly given 'logic'
//   -- a 1-bit (scalar) 4-state type (Sec 6.11, Table 6-8 / Sec 6.11.2).
// IEEE 1800-2023 Sec 23.2.2.3: 'output logic a' uses the explicit
//   data_type syntax with no net type, so the port kind defaults to a
//   variable (not a net).
// IEEE 1800-2023 Sec 10.3: 'assign a = P;' is a continuous assignment whose
//   LHS references 'a' and whose RHS references parameter 'P'.
// IEEE 1800-2023 Sec 23.3.2 / 23.10: 'dut #(.P(~X)) u_dut(.a(o))' is a
//   module instantiation named 'u_dut' of 'dut', overriding 'P' by name
//   (Sec 23.10.2.2) with the expression '~X' -- a bitwise negation
//   (Sec 11.4.8) of a reference to top's parameter 'X' -- and connecting
//   port 'a' by name (Sec 23.3.2.2) to top's 'o'.
//
// Not checked: the elaborated value of 'u_dut.P' (~1'b0 == 1'b1 for a
// 1-bit logic) since this compile does not elaborate; the bit width of the
// unsized literal '0' (Sec 5.7.1 only requires "at least 32 bits").

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
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LogicSizeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LogicSize.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *findParam(const hldb::Module *m, std::string_view name) {
    if (m == nullptr || m->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *m->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  static const hldb::RefInstance *getUDut() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_dut", top->getRefInstances());
  }
};

// ---------------------------------------------------------------------------
// Existence
// ---------------------------------------------------------------------------

TEST_F(LogicSizeTest, ModulesDutAndTopExist) {
  EXPECT_NE(getModule("dut"), nullptr) << "module 'dut' not found";
  EXPECT_NE(getModule("top"), nullptr) << "module 'top' not found";
}

// ---------------------------------------------------------------------------
// 'parameter logic P = 0' / 'parameter logic X = 0' -- Sec 6.20.2
// ---------------------------------------------------------------------------

TEST_F(LogicSizeTest, DutParamPIsScalarLogic) {
  const hldb::Parameter *const p = findParam(getModule("dut"), "P");
  ASSERT_NE(p, nullptr) << "'parameter logic P' not found in 'dut'";
  EXPECT_FALSE(p->getLocalParam()) << "'P' is a plain (overridable) parameter";
  const hldb::RefTypespec *const rts = p->getTypespec();
  ASSERT_NE(rts, nullptr);
  ASSERT_NE(rts->getActual(), nullptr);
  ASSERT_EQ(rts->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
  const hldb::LogicTypespec *const lts = rts->getActual<hldb::LogicTypespec>();
  EXPECT_TRUE(lts->getRanges() == nullptr || lts->getRanges()->empty()) << "'logic' with no packed dimension is 1-bit";
  EXPECT_FALSE(lts->getSigned()) << "'logic' is unsigned by default (Sec 6.11.3)";
}

TEST_F(LogicSizeTest, TopParamXIsScalarLogic) {
  const hldb::Parameter *const x = findParam(getModule("top"), "X");
  ASSERT_NE(x, nullptr) << "'parameter logic X' not found in 'top'";
  const hldb::RefTypespec *const rts = x->getTypespec();
  ASSERT_NE(rts, nullptr);
  ASSERT_NE(rts->getActual(), nullptr);
  ASSERT_EQ(rts->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
  const hldb::LogicTypespec *const lts = rts->getActual<hldb::LogicTypespec>();
  EXPECT_TRUE(lts->getRanges() == nullptr || lts->getRanges()->empty());
}

TEST_F(LogicSizeTest, DutParamPDefaultIsConstantZero) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("P", dut->getParamAssigns());
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'P' not found";
  ASSERT_NE(pa->getRhs(), nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'P = 0': default value must be a Constant";
  EXPECT_EQ(rhs->getDecompile(), std::string_view("0"));
}

// ---------------------------------------------------------------------------
// 'output logic a' / 'output logic o' -- Sec 23.2.2.3: variables, not nets
// ---------------------------------------------------------------------------

TEST_F(LogicSizeTest, DutPortAIsOutputVariable) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  const hldb::Port *const port = hldb::findByName<hldb::Port>("a", dut->getPorts());
  ASSERT_NE(port, nullptr) << "port 'a' not found";
  EXPECT_EQ(port->getDirection(), vpiOutput);
  EXPECT_NE(hldb::findByName<hldb::Variable>("a", dut->getVariables()), nullptr)
      << "'output logic a' must be a variable per Sec 23.2.2.3";
  EXPECT_EQ(hldb::findByName<hldb::Net>("a", dut->getNets()), nullptr);
}

TEST_F(LogicSizeTest, TopPortOIsOutputVariable) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::Port *const port = hldb::findByName<hldb::Port>("o", top->getPorts());
  ASSERT_NE(port, nullptr) << "port 'o' not found";
  EXPECT_EQ(port->getDirection(), vpiOutput);
  EXPECT_NE(hldb::findByName<hldb::Variable>("o", top->getVariables()), nullptr)
      << "'output logic o' must be a variable per Sec 23.2.2.3";
  EXPECT_EQ(hldb::findByName<hldb::Net>("o", top->getNets()), nullptr);
}

// ---------------------------------------------------------------------------
// 'assign a = P;' -- Sec 10.3
// ---------------------------------------------------------------------------

TEST_F(LogicSizeTest, DutContAssignAEqualsP) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getContAssigns(), nullptr);
  ASSERT_EQ(dut->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = dut->getContAssigns()->at(0);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("a"));
  ASSERT_NE(lhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Variable);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("P"));
  ASSERT_NE(rhs->getActual(), nullptr);
  EXPECT_EQ(rhs->getActual()->getAnyType(), hldb::AnyType::Parameter);
}

// ---------------------------------------------------------------------------
// 'dut #(.P(~X)) u_dut(.a(o));' -- Sec 23.3.2, 23.10
// ---------------------------------------------------------------------------

TEST_F(LogicSizeTest, TopInstantiatesDutAsUDut) {
  const hldb::RefInstance *const inst = getUDut();
  ASSERT_NE(inst, nullptr) << "instance 'u_dut' not found in 'top'";
  const hldb::RefTypespec *const rts = inst->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::ModuleTypespec *const mts = rts->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mts, nullptr) << "'u_dut' should reference module definition 'dut'";
  EXPECT_EQ(mts->getDefName(), std::string_view("dut"));
  EXPECT_EQ(mts->getModule(), getModule("dut"));
}

TEST_F(LogicSizeTest, UDutOverridesPByNameWithBitNegOfX) {
  const hldb::RefInstance *const inst = getUDut();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mts = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mts, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("P", mts->getParamAssigns());
  ASSERT_NE(pa, nullptr) << "override '.P(~X)' not found";
  EXPECT_TRUE(pa->getConnByName()) << "'.P(...)' is a named parameter assignment (Sec 23.10.2.2)";
  EXPECT_TRUE(pa->getOverridden());
  const hldb::Operation *const op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'~X' should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiBitNegOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const hldb::RefObj *const xref = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(xref, nullptr);
  EXPECT_EQ(xref->getName(), std::string_view("X"));
  ASSERT_NE(xref->getActual(), nullptr);
  EXPECT_EQ(xref->getActual(), findParam(getModule("top"), "X")) << "'X' must bind to top's parameter 'X'";
}

TEST_F(LogicSizeTest, UDutConnectsPortAToO) {
  const hldb::RefInstance *const inst = getUDut();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getPorts(), nullptr);
  ASSERT_EQ(inst->getPorts()->size(), 1u);
  const hldb::Port *const port = any_cast<hldb::Port>(inst->getPorts()->at(0));
  ASSERT_NE(port, nullptr);
  const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getName(), std::string_view("a"));
  const hldb::RefObj *const high = port->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr);
  EXPECT_EQ(high->getName(), std::string_view("o"));
  ASSERT_NE(high->getActual(), nullptr);
  EXPECT_EQ(high->getActual()->getAnyType(), hldb::AnyType::Variable);
}

// ---------------------------------------------------------------------------
// Diagnostics -- the source is legal
// ---------------------------------------------------------------------------

TEST_F(LogicSizeTest, CompilerReportsZeroErrors) {
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
