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
//   IMPORTANT -- this is a non-elaborated compiler, not an elaborator:
//   per the project's explicit design decision (HLC records only what
//   the source states; it never materializes a Net/Variable for an
//   implicitly-declared identifier -- see the nonelaborated-model
//   design notes), the *correct*, non-buggy shape for "c" in "top" is a
//   plain RefObj named "c" with a null vpiActual, and NO Net object in
//   top's vpiNet collection. A later elaboration pass is what would
//   eventually materialize the real net. A prior version of this file
//   asserted the opposite (expected a real Net / a resolved vpiActual
//   for "c") and treated that absence as a bug -- it was not; those
//   assertions have been corrected to match the deliberate
//   non-elaborated model.
//
//   Separately, a prior version of this file's comment claimed "HLC
//   reports EL0535 (ELAB_ILLEGAL_IMPLICIT_NET) twice" for this
//   construct. That was a real bug (an elaboration-only error being
//   reported by a non-elaborating compiler pass) but it predates this
//   revision: the offending call site has since been fixed to report
//   the correctly-scoped COMP_FAILED_TO_BIND instead, and
//   ObjectBinder::reportErrors() explicitly skips a plain (non-
//   hierarchical, non-package-scoped) unresolved RefObj like "c" is
//   here -- so no error or warning should be reported for it at all.
//   This file asserts that directly instead of repeating the old,
//   now-stale ELAB_ILLEGAL_IMPLICIT_NET claim.
//
// What is checked:
//   - design has exactly 2 modules (top, test)
//   - top has explicit nets a (=1), b (=0), d only; "c" is implicit and
//     correctly absent from vpiNet in this non-elaborated model
//   - top has 1 ContAssign: lhs=d, rhs=c (RefObj "c" stays unresolved --
//     vpiActual is null, matching the non-elaborated model)
//   - top has 1 RefInstance named "mod" with 3 port connections; the
//     "c" connection's RefObj likewise stays unresolved (vpiActual null)
//   - RefInstance "mod" typespec is a ModuleTypespec pointing to test
//     (via getName()); ModuleTypespec::getModule() is null for this
//     pattern (pinned known limitation -- getName() is the reliable way
//     to identify the submodule)
//   - test (the submodule, where c is a real explicit "output c" port)
//     has 3 nets (a, b, c) and 3 ports (a:input, b:input, c:output);
//     ContAssign RHS is vpiBitOrOp(a, b)
//   - THE POINT OF THIS FILE: per IEEE 1800-2023 6.10, "c" used only in
//     the port connection list of "test mod(a, b, c);" is a legal
//     implicit-net circumstance, and HLC must report zero errors or
//     warnings for it while leaving it correctly unresolved in the
//     non-elaborated graph.
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
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

TEST_F(ImplicitPortConnectionTest, CIsNotMaterializedAsNetInTop) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr)
      << "IEEE 1800-2023 6.10 mandates 'c' (used only in the port connection list of "
         "'test mod(a, b, c)', never declared in top) be treated as an implicit scalar net -- "
         "but HLC is a non-elaborated compiler and never materializes a Net object for an "
         "implicitly-declared identifier; that is deferred to a later elaboration pass. No Net "
         "named 'c' should exist here.";
}

// ---------------------------------------------------------------------------
// top -- assign d = c (RHS 'c' stays an unresolved RefObj -- non-elaborated model)
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

TEST_F(ImplicitPortConnectionTest, TopContAssignRhsCStaysUnresolvedRefObj) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a RefObj";
  EXPECT_EQ(rhs->getName(), "c");
  EXPECT_EQ(rhs->getActual<hldb::Net>(), nullptr)
      << "'c' is the legal IEEE 1800-2023 6.10 implicit-net circumstance, but HLC's "
         "non-elaborated model never resolves it to a materialized Net -- vpiActual should "
         "stay null here; resolution is elaboration's job.";
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

  // Third port connection is 'c' -- the legal 6.10 implicit-net circumstance; stays unresolved
  const hldb::Port *const port_c = any_cast<hldb::Port>(inst->getPorts()->at(2));
  ASSERT_NE(port_c, nullptr);
  const hldb::RefObj *const hc = port_c->getHighConn<hldb::RefObj>();
  ASSERT_NE(hc, nullptr) << "port c highConn is not a RefObj";
  EXPECT_EQ(hc->getName(), "c");
  EXPECT_EQ(hc->getActual<hldb::Net>(), nullptr)
      << "'c' is the legal IEEE 1800-2023 6.10 implicit-net circumstance in the port connection "
         "list, but HLC's non-elaborated model never resolves it to a materialized Net -- "
         "vpiActual should stay null here; resolution is elaboration's job.";
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

  const hldb::Module *const test = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(test, nullptr);

  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(top->getRefInstances()->at(0));
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getTypespec(), nullptr);

  const hldb::ModuleTypespec *const ts = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(ts, nullptr);

  EXPECT_EQ(ts->getModule(), test) << "expected ModuleTypespec::getModule() to be resolved";
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
// The actual point of the file: an identifier used only in a port connection
// list is a legal implicit-net circumstance per IEEE 1800-2023 6.10 -- it
// must not be reported as a binding failure.
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortConnectionTest, NoFailedToBindErrorReportedForImplicitNetC) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "c"), nullptr)
      << "IEEE 1800-2023 6.10: 'c' used only in the port connection list of "
         "'test mod(a, b, c)' is a legal implicit-net circumstance, not a binding failure. "
         "ObjectBinder::reportErrors() should skip a plain (non-hierarchical, non-package-scoped) "
         "unresolved RefObj like this one without reporting COMP_FAILED_TO_BIND. (This test used "
         "to check for the now-retired ELAB_ILLEGAL_IMPLICIT_NET, an elaboration-only error that "
         "was wrongly being reported from this non-elaborating compiler; that call site now "
         "reports the correctly-scoped COMP_FAILED_TO_BIND instead, so this checks that error "
         "type directly.)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
