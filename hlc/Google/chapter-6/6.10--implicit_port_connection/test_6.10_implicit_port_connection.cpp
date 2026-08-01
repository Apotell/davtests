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

// Validates the UHDM graph for a design with two modules where 'c' is used
// as an implicit net in the top module:
//   module top:  wire a=1, b=0, d;  test mod(a, b, c);  assign d = c;
//   module test: input a, b; output c;  assign c = a | b;
// Per IEEE 1800-2023 Sec 6.10, the undeclared identifier 'c' (used both as a
// port connection and as the RHS of a continuous assignment) is implicitly
// declared as a scalar net of the default net type (wire). This is legal
// SystemVerilog; HLC compiles this with 0 errors and, at this
// (non-elaborated) stage, does not materialize a Net object for 'c' -- only
// RefObj references with no vpiActual back-pointer are recorded.
//
// Checked:
//   - design has exactly 2 modules (top, test)
//   - top has 3 explicit nets (a=1, b=0, d); 'c' is implicit and absent from vpiNet
//   - top has 1 ContAssign: lhs=d, rhs=c (RefObj with no vpiActual -- implicit)
//   - top has 1 RefInstance named "mod" with 3 port connections
//   - RefInstance "mod" typespec is a ModuleTypespec pointing to test (via getName())
//   - port connection for 'c' on the RefInstance has no vpiActual (implicit net)
//   - test has 3 nets (a, b, c) and 3 ports (a:input, b:input, c:output)
//   - test ContAssign RHS is a vpiBitOrOp with operands a and b
//   - ModuleTypespec::getModule() is null for this pattern (pinned known limitation;
//     getName() == "test" is used instead to verify the submodule reference)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ImplicitPortConnection : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.10--implicit_port_connection.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Design level
// ----
TEST_F(ImplicitPortConnection, TwoModulesExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u) << "expected top and test";
}

// ----
// top -- net declarations (a, b, d explicit; c implicit)
// ----
TEST_F(ImplicitPortConnection, TopModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(ImplicitPortConnection, TopHasThreeNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u) << "expected nets a, b, d -- 'c' is implicit and absent from vpiNet";
}

TEST_F(ImplicitPortConnection, TopANetHasInitialValueOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr) << "net 'a' not found in top";
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'a' has no initial value";
  EXPECT_EQ(init->getDecompile(), "1");
}

TEST_F(ImplicitPortConnection, TopBNetHasInitialValueZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found in top";
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'b' has no initial value";
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(ImplicitPortConnection, TopDNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("d", top->getNets()), nullptr) << "net 'd' not found in top";
}

TEST_F(ImplicitPortConnection, TopCNetNotDeclared) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr)
      << "'c' should not appear in vpiNet of top -- implicit net not yet bound/materialized";
}

// ----
// top -- assign d = c (RHS 'c' has no vpiActual)
// ----
TEST_F(ImplicitPortConnection, TopContAssignLhsIsD) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);

  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "d");
}

TEST_F(ImplicitPortConnection, TopContAssignRhsIsCWithNoActual) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a RefObj";
  EXPECT_EQ(rhs->getName(), "c");
  EXPECT_EQ(rhs->getActual(), nullptr) << "'c' is implicit -- RHS RefObj should have no vpiActual";
}

// ----
// top -- module instantiation: test mod(a, b, c)
// ----
TEST_F(ImplicitPortConnection, TopHasOneRefInstance) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr) << "top has no ref instances";
  EXPECT_EQ(top->getRefInstances()->size(), 1u);
}

TEST_F(ImplicitPortConnection, TopRefInstanceIsNamedMod) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "mod");
}

TEST_F(ImplicitPortConnection, TopRefInstanceHasThreePorts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getPorts(), nullptr);
  EXPECT_EQ(inst->getPorts()->size(), 3u);
}

TEST_F(ImplicitPortConnection, TopPortCConnectionHasNoActual) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getPorts(), nullptr);
  ASSERT_EQ(inst->getPorts()->size(), 3u);

  // Third port connection is 'c' -- implicit, so highConn RefObj has no vpiActual
  const hldb::Port *const port_c = any_cast<hldb::Port>(inst->getPorts()->at(2));
  ASSERT_NE(port_c, nullptr);
  const hldb::RefObj *const hc = port_c->getHighConn<hldb::RefObj>();
  ASSERT_NE(hc, nullptr) << "port c highConn is not a RefObj";
  EXPECT_EQ(hc->getName(), "c");
  EXPECT_EQ(hc->getActual(), nullptr) << "implicit 'c' port connection should have no vpiActual";
}

TEST_F(ImplicitPortConnection, TopRefInstanceModPointsToWorkAtTest) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getTypespec(), nullptr) << "RefInstance 'mod' has no typespec";

  const hldb::ModuleTypespec *const ts = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(ts, nullptr) << "RefInstance 'mod' typespec is not a ModuleTypespec";

  EXPECT_EQ(ts->getName(), "test") << "RefInstance 'mod' in top should point to test";
}

TEST_F(ImplicitPortConnection, TopRefInstanceModTypespecGetModuleIsNull) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getTypespec(), nullptr);

  const hldb::ModuleTypespec *const ts = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(ts, nullptr);

  EXPECT_EQ(ts->getModule(), nullptr)
      << "known HLC limitation: ModuleTypespec::getModule() is not resolved for this pattern; "
      << "getName() is the only reliable way to identify the submodule";
}

// ----
// test -- module definition with ports and assign c = a | b
// ----
TEST_F(ImplicitPortConnection, TestModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("test", m_design->getAllModules()), nullptr);
}

TEST_F(ImplicitPortConnection, TestHasThreeNets) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getNets(), nullptr);
  EXPECT_EQ(test->getNets()->size(), 3u) << "expected nets a, b, c in test";
}

TEST_F(ImplicitPortConnection, TestHasThreePorts) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);
  EXPECT_EQ(test->getPorts()->size(), 3u);
}

TEST_F(ImplicitPortConnection, TestPortAIsInput) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);

  const hldb::Port *const pa = hldb::findByName<hldb::Port>("a", test->getPorts());
  ASSERT_NE(pa, nullptr) << "port 'a' not found in test";
  EXPECT_EQ(pa->getDirection(), vpiInput);
}

TEST_F(ImplicitPortConnection, TestPortBIsInput) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);

  const hldb::Port *const pb = hldb::findByName<hldb::Port>("b", test->getPorts());
  ASSERT_NE(pb, nullptr) << "port 'b' not found in test";
  EXPECT_EQ(pb->getDirection(), vpiInput);
}

TEST_F(ImplicitPortConnection, TestPortCIsOutput) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);

  const hldb::Port *const pc = hldb::findByName<hldb::Port>("c", test->getPorts());
  ASSERT_NE(pc, nullptr) << "port 'c' not found in test";
  EXPECT_EQ(pc->getDirection(), vpiOutput);
}

TEST_F(ImplicitPortConnection, TestContAssignRhsIsBitOr) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getContAssigns(), nullptr);
  ASSERT_EQ(test->getContAssigns()->size(), 1u);

  const hldb::Operation *const rhs = test->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "test ContAssign RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitOrOp) << "expected vpiBitOrOp (29)";
}

TEST_F(ImplicitPortConnection, TestBitOrOperandsAreAAndB) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getContAssigns(), nullptr);

  const hldb::Operation *const rhs = test->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::RefObj *const op0 = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  const hldb::RefObj *const op1 = any_cast<hldb::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(op0, nullptr) << "first operand is not a RefObj";
  ASSERT_NE(op1, nullptr) << "second operand is not a RefObj";
  EXPECT_EQ(op0->getName(), "a");
  EXPECT_EQ(op1->getName(), "b");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
