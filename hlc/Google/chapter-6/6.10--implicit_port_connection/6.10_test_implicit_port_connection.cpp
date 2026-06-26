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
// Surelog reports EL0535 twice (implicit net 'c') but still produces UHDM.
//
// Checked:
//   - design has exactly 2 modules (work@top, work@test)
//   - work@top has 3 explicit nets (a=1, b=0, d); 'c' is implicit and absent from vpiNet
//   - work@top has 1 ContAssign: lhs=d, rhs=c (RefObj with no vpiActual — implicit)
//   - work@top has 1 RefInstance named "mod" with 3 port connections
//   - RefInstance "mod" typespec is a ModuleTypespec pointing to work@test (via getName())
//   - port connection for 'c' on the RefInstance has no vpiActual (implicit net)
//   - work@test has 3 nets (a, b, c) and 3 ports (a:input, b:input, c:output)
//   - work@test ContAssign RHS is a vpiBitOrOp with operands a and b
//
// Not checked:
//   - ModuleTypespec::getModule() — returns null in Surelog for this pattern;
//     use getName() == "work@test" instead to verify the submodule reference

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/cont_assign.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/port.h>
#include <uhdm/module_typespec.h>
#include <uhdm/ref_instance.h>
#include <uhdm/ref_obj.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class ImplicitPortConnection : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.10--implicit_port_connection.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

// ---------------------------------------------------------------------------
// Design level
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnection, TwoModulesExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u)
      << "expected work@top and work@test";
}

// ---------------------------------------------------------------------------
// work@top — net declarations (a, b, d explicit; c implicit)
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnection, TopModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(ImplicitPortConnection, TopHasThreeNets) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u)
      << "expected nets a, b, d — 'c' is implicit and absent from vpiNet";
}

TEST_F(ImplicitPortConnection, TopANetHasInitialValueOne) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr) << "net 'a' not found in work@top";
  const uhdm::Constant *const init = a->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr) << "net 'a' has no initial value";
  EXPECT_EQ(init->getDecompile(), "1");
}

TEST_F(ImplicitPortConnection, TopBNetHasInitialValueZero) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found in work@top";
  const uhdm::Constant *const init = b->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr) << "net 'b' has no initial value";
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(ImplicitPortConnection, TopDNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(uhdm::findByName<uhdm::Net>("d", top->getNets()), nullptr)
      << "net 'd' not found in work@top";
}

TEST_F(ImplicitPortConnection, TopCNetNotDeclared) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(uhdm::findByName<uhdm::Net>("c", top->getNets()), nullptr)
      << "'c' should not appear in vpiNet of work@top — it is implicit (EL0535)";
}

// ---------------------------------------------------------------------------
// work@top — assign d = c (RHS 'c' has no vpiActual)
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnection, TopContAssignLhsIsD) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);

  const uhdm::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "d");
}

TEST_F(ImplicitPortConnection, TopContAssignRhsIsCWithNoActual) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::RefObj *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::RefObj>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a RefObj";
  EXPECT_EQ(rhs->getName(), "c");
  EXPECT_EQ(rhs->getActual(), nullptr)
      << "'c' is implicit — RHS RefObj should have no vpiActual";
}

// ---------------------------------------------------------------------------
// work@top — module instantiation: test mod(a, b, c)
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnection, TopHasOneRefInstance) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr) << "work@top has no ref instances";
  EXPECT_EQ(top->getRefInstances()->size(), 1u);
}

TEST_F(ImplicitPortConnection, TopRefInstanceIsNamedMod) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const uhdm::RefInstance *const inst =
      any_cast<uhdm::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "mod");
}

TEST_F(ImplicitPortConnection, TopRefInstanceHasThreePorts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const uhdm::RefInstance *const inst =
      any_cast<uhdm::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getPorts(), nullptr);
  EXPECT_EQ(inst->getPorts()->size(), 3u);
}

TEST_F(ImplicitPortConnection, TopPortCConnectionHasNoActual) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const uhdm::RefInstance *const inst =
      any_cast<uhdm::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getPorts(), nullptr);
  ASSERT_EQ(inst->getPorts()->size(), 3u);

  // Third port connection is 'c' — implicit, so highConn RefObj has no vpiActual
  const uhdm::Port *const port_c =
      any_cast<uhdm::Port>(inst->getPorts()->at(2));
  ASSERT_NE(port_c, nullptr);
  const uhdm::RefObj *const hc = port_c->getHighConn<uhdm::RefObj>();
  ASSERT_NE(hc, nullptr) << "port c highConn is not a RefObj";
  EXPECT_EQ(hc->getName(), "c");
  EXPECT_EQ(hc->getActual(), nullptr)
      << "implicit 'c' port connection should have no vpiActual";
}

TEST_F(ImplicitPortConnection, TopRefInstanceModPointsToWorkAtTest) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const uhdm::RefInstance *const inst =
      any_cast<uhdm::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getTypespec(), nullptr) << "RefInstance 'mod' has no typespec";

  const uhdm::ModuleTypespec *const ts =
      inst->getTypespec()->getActual<uhdm::ModuleTypespec>();
  ASSERT_NE(ts, nullptr) << "RefInstance 'mod' typespec is not a ModuleTypespec";

  EXPECT_EQ(ts->getName(), "work@test")
      << "RefInstance 'mod' in work@top should point to work@test";
}

// ---------------------------------------------------------------------------
// work@test — module definition with ports and assign c = a | b
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnection, TestModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@test", m_design->getAllModules()), nullptr);
}

TEST_F(ImplicitPortConnection, TestHasThreeNets) {
  const uhdm::Module *const test =
      uhdm::findByName<uhdm::Module>("work@test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getNets(), nullptr);
  EXPECT_EQ(test->getNets()->size(), 3u) << "expected nets a, b, c in work@test";
}

TEST_F(ImplicitPortConnection, TestHasThreePorts) {
  const uhdm::Module *const test =
      uhdm::findByName<uhdm::Module>("work@test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);
  EXPECT_EQ(test->getPorts()->size(), 3u);
}

TEST_F(ImplicitPortConnection, TestPortAIsInput) {
  const uhdm::Module *const test =
      uhdm::findByName<uhdm::Module>("work@test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);

  const uhdm::Port *const pa = uhdm::findByName<uhdm::Port>("a", test->getPorts());
  ASSERT_NE(pa, nullptr) << "port 'a' not found in work@test";
  EXPECT_EQ(pa->getDirection(), vpiInput);
}

TEST_F(ImplicitPortConnection, TestPortBIsInput) {
  const uhdm::Module *const test =
      uhdm::findByName<uhdm::Module>("work@test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);

  const uhdm::Port *const pb = uhdm::findByName<uhdm::Port>("b", test->getPorts());
  ASSERT_NE(pb, nullptr) << "port 'b' not found in work@test";
  EXPECT_EQ(pb->getDirection(), vpiInput);
}

TEST_F(ImplicitPortConnection, TestPortCIsOutput) {
  const uhdm::Module *const test =
      uhdm::findByName<uhdm::Module>("work@test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);

  const uhdm::Port *const pc = uhdm::findByName<uhdm::Port>("c", test->getPorts());
  ASSERT_NE(pc, nullptr) << "port 'c' not found in work@test";
  EXPECT_EQ(pc->getDirection(), vpiOutput);
}

TEST_F(ImplicitPortConnection, TestContAssignRhsIsBitOr) {
  const uhdm::Module *const test =
      uhdm::findByName<uhdm::Module>("work@test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getContAssigns(), nullptr);
  ASSERT_EQ(test->getContAssigns()->size(), 1u);

  const uhdm::Operation *const rhs =
      test->getContAssigns()->at(0)->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr) << "work@test ContAssign RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitOrOp)
      << "expected vpiBitOrOp (29)";
}

TEST_F(ImplicitPortConnection, TestBitOrOperandsAreAAndB) {
  const uhdm::Module *const test =
      uhdm::findByName<uhdm::Module>("work@test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getContAssigns(), nullptr);

  const uhdm::Operation *const rhs =
      test->getContAssigns()->at(0)->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const uhdm::RefObj *const op0 =
      any_cast<uhdm::RefObj>((*rhs->getOperands())[0]);
  const uhdm::RefObj *const op1 =
      any_cast<uhdm::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(op0, nullptr) << "first operand is not a RefObj";
  ASSERT_NE(op1, nullptr) << "second operand is not a RefObj";
  EXPECT_EQ(op0->getName(), "a");
  EXPECT_EQ(op1->getName(), "b");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
