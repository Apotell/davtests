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

// Tests for 6.10--implicit_port_connection.sv (tags: 6.10)
//   module top:  wire a=1, b=0, d;  test mod(a, b, c);  assign d = c;
//   module test: input a, b; output c;  assign c = a | b;
//
// What to check and why (IEEE 1800-2023 6.10 "Implicit declarations",
// p.108, checked before any test code was written):
//   "If an identifier is used in the terminal list of a primitive
//   instance or in the port connection list of a module ... instance
//   ... and that identifier has not been declared previously ... then
//   an implicit scalar net of default net type shall be assumed." "c"
//   in "test mod(a, b, c);" is never declared anywhere in "top" -- this
//   is exactly the circumstance the spec describes, and it is legal,
//   not an error. This file has no :should_fail_because: tag.
//
//   A prior version of this test's file-level comment claimed "HLC
//   reports EL0535 twice (implicit net 'c')" but never actually asserted
//   an error count anywhere in the test body -- an unverified claim
//   sitting in prose. Given the same EL0535-on-legal-implicit-net bug is
//   already confirmed in 6.6.8--interconnect.sv and
//   6.10--implicit_continuous_assignment.sv, this version adds a real
//   assertion for it, and treats 'c' having no Net node / no vpiActual
//   in "top" as documenting the SAME bug (HLC should create a real
//   implicit net for "c" in top's scope), not neutral fact.
//
// What is checked:
//   - design has exactly 2 modules (top, test)
//   - top has explicit nets a (=1), b (=0), d; 'c' should also be a
//     real (implicit) net per 6.10, but currently is not
//   - top has 1 ContAssign: lhs=d, rhs=c (RefObj -- should resolve via
//     vpiActual to the implicit net, currently does not)
//   - top has 1 RefInstance named "mod" with 3 port connections; the
//     'c' connection's RefObj should likewise resolve to the implicit
//     net, currently does not
//   - RefInstance "mod" typespec is a ModuleTypespec pointing to test
//     (via getName()); ModuleTypespec::getModule() is null for this
//     pattern (pinned known limitation -- getName() is the reliable way
//     to identify the submodule)
//   - test (the submodule, where c is a real explicit "output c" port)
//     has 3 nets (a, b, c) and 3 ports (a:input, b:input, c:output);
//     ContAssign RHS is vpiBitOrOp(a, b)
//   - THE POINT OF THIS FILE: per IEEE 1800-2023 6.10, "c" used only in
//     the port connection list of "test mod(a, b, c);" should be
//     implicitly declared as a real net in top's scope with zero
//     compiler errors -- a real, non-skipped, currently-failing
//     assertion
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
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

class ImplicitPortConnectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.10--implicit_port_connection.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Design level
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnectionTest, TwoModulesExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u) << "expected top and test";
}

// ---------------------------------------------------------------------------
// top -- net declarations (a, b, d explicit; c implicit)
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnectionTest, TopModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(ImplicitPortConnectionTest, TopHasThreeNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u) << "expected nets a, b, d -- 'c' is implicit and absent from vpiNet";
}

TEST_F(ImplicitPortConnectionTest, TopANetHasInitialValueOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr) << "net 'a' not found in top";
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'a' has no initial value";
  EXPECT_EQ(init->getDecompile(), "1");
}

TEST_F(ImplicitPortConnectionTest, TopBNetHasInitialValueZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found in top";
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'b' has no initial value";
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(ImplicitPortConnectionTest, TopDNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("d", top->getNets()), nullptr) << "net 'd' not found in top";
}

TEST_F(ImplicitPortConnectionTest, CShouldBeAnImplicitNetInTopButIsNotCreated) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr)
      << "IEEE 1800-2023 6.10: 'c' used only in the port connection list of 'test mod(a, b, c)' "
         "and never declared in top should be an implicit scalar net of default net type -- "
         "mandatory, legal behavior, not an error. HLC currently creates no Net for 'c' at all";
}

// ---------------------------------------------------------------------------
// top -- assign d = c (RHS 'c' should resolve to the implicit net)
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnectionTest, TopContAssignLhsIsD) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);

  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "d");
}

TEST_F(ImplicitPortConnectionTest, TopContAssignRhsCShouldResolveToImplicitNetButDoesNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a RefObj";
  EXPECT_EQ(rhs->getName(), "c");
  EXPECT_NE(rhs->getActual<hldb::Net>(), nullptr)
      << "'c' should resolve to the implicit Net that IEEE 1800-2023 6.10 mandates -- HLC "
         "currently leaves this RefObj unresolved (no vpiActual)";
}

// ---------------------------------------------------------------------------
// top -- module instantiation: test mod(a, b, c)
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnectionTest, TopHasOneRefInstance) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr) << "top has no ref instances";
  EXPECT_EQ(top->getRefInstances()->size(), 1u);
}

TEST_F(ImplicitPortConnectionTest, TopRefInstanceIsNamedMod) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "mod");
}

TEST_F(ImplicitPortConnectionTest, TopRefInstanceHasThreePorts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getPorts(), nullptr);
  EXPECT_EQ(inst->getPorts()->size(), 3u);
}

TEST_F(ImplicitPortConnectionTest, TopPortCConnectionHasNoActual) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);

  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getPorts(), nullptr);
  ASSERT_EQ(inst->getPorts()->size(), 3u);

  // Third port connection is 'c' -- should resolve to the implicit net IEEE 1800-2023 6.10 mandates
  const hldb::Port *const port_c = any_cast<hldb::Port>(inst->getPorts()->at(2));
  ASSERT_NE(port_c, nullptr);
  const hldb::RefObj *const hc = port_c->getHighConn<hldb::RefObj>();
  ASSERT_NE(hc, nullptr) << "port c highConn is not a RefObj";
  EXPECT_EQ(hc->getName(), "c");
  EXPECT_NE(hc->getActual<hldb::Net>(), nullptr)
      << "the 'c' port connection should resolve to the implicit net IEEE 1800-2023 6.10 "
         "mandates -- HLC currently leaves this RefObj unresolved (no vpiActual)";
}

TEST_F(ImplicitPortConnectionTest, TopRefInstanceModPointsToWorkAtTest) {
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

TEST_F(ImplicitPortConnectionTest, TopRefInstanceModTypespecGetModuleIsNull) {
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

// ---------------------------------------------------------------------------
// test -- module definition with ports and assign c = a | b
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnectionTest, TestModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("test", m_design->getAllModules()), nullptr);
}

TEST_F(ImplicitPortConnectionTest, TestHasThreeNets) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getNets(), nullptr);
  EXPECT_EQ(test->getNets()->size(), 3u) << "expected nets a, b, c in test";
}

TEST_F(ImplicitPortConnectionTest, TestHasThreePorts) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);
  EXPECT_EQ(test->getPorts()->size(), 3u);
}

TEST_F(ImplicitPortConnectionTest, TestPortAIsInput) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);

  const hldb::Port *const pa = hldb::findByName<hldb::Port>("a", test->getPorts());
  ASSERT_NE(pa, nullptr) << "port 'a' not found in test";
  EXPECT_EQ(pa->getDirection(), vpiInput);
}

TEST_F(ImplicitPortConnectionTest, TestPortBIsInput) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);

  const hldb::Port *const pb = hldb::findByName<hldb::Port>("b", test->getPorts());
  ASSERT_NE(pb, nullptr) << "port 'b' not found in test";
  EXPECT_EQ(pb->getDirection(), vpiInput);
}

TEST_F(ImplicitPortConnectionTest, TestPortCIsOutput) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getPorts(), nullptr);

  const hldb::Port *const pc = hldb::findByName<hldb::Port>("c", test->getPorts());
  ASSERT_NE(pc, nullptr) << "port 'c' not found in test";
  EXPECT_EQ(pc->getDirection(), vpiOutput);
}

TEST_F(ImplicitPortConnectionTest, TestContAssignRhsIsBitOr) {
  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getContAssigns(), nullptr);
  ASSERT_EQ(test->getContAssigns()->size(), 1u);

  const hldb::Operation *const rhs = test->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "test ContAssign RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitOrOp) << "expected vpiBitOrOp (29)";
}

TEST_F(ImplicitPortConnectionTest, TestBitOrOperandsAreAAndB) {
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

// ---------------------------------------------------------------------------
// The actual point of the file: implicit net creation from a port connection
// list is mandatory, legal SystemVerilog per IEEE 1800-2023 6.10
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnectionTest, CompilerShouldAcceptLegalImplicitNetButReportsSpuriousErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0)
      << "IEEE 1800-2023 6.10 mandates an implicit net for 'c' here, not an error -- this matches "
         "the same EL0535-on-legal-implicit-net bug already confirmed in "
         "6.6.8--interconnect.sv and 6.10--implicit_continuous_assignment.sv. A prior version of "
         "this test's file comment claimed HLC reports EL0535 twice for this file but never "
         "actually asserted it in code -- this is that assertion";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
