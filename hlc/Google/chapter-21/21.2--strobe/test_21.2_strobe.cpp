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

// Regression coverage for tests/Google/chapter-21/21.2--strobe.sv
//
// The fixture is the IEEE 1800 clause 21.2 "$strobe" display-task test. Its
// whole text is:
//
//   module top();
//
//   logic clk;
//   int a;
//
//   always @(posedge clk) begin
//     $strobe(a);
//     $strobeb(a);
//     $strobeo(a);
//     $strobeh(a);
//   end
//
//   endmodule
//
// Every expectation below is derived from that source plus IEEE 1800
// semantics, never from what the compiler happens to emit:
//
//   - 'top' is the sole module the file declares. Its port list is written
//     '()', so it has zero ports, and it instantiates nothing.
//   - 'logic clk;' is a variable (not a net) declaration of the 4-state
//     'logic' type. 'logic' is unsigned (IEEE 1800 6.8) and, written with no
//     packed dimension, denotes a single bit, so it is not a vector.
//   - 'int a;' is the 2-state 32-bit *signed* integer type (IEEE 1800 6.11.1),
//     also with no packed dimensions.
//   - Both variables are declared directly in a module, so they are static,
//     not automatic (IEEE 1800 6.21).
//   - 'always @(posedge clk)' is the general-purpose always procedure, not
//     always_comb / always_ff / always_latch, so its always type is vpiAlways.
//     Its event expression is the single 'posedge' edge-qualified event on
//     'clk'.
//   - The procedure body is an unnamed sequential 'begin ... end' block
//     holding exactly four statements, in source order.
//   - '$strobe', '$strobeb', '$strobeo' and '$strobeh' are the four built-in
//     display system *tasks* of IEEE 1800 Table 21-1 (the binary, octal and
//     hexadecimal default-radix variants of $strobe). None is user defined,
//     and each is called here with exactly one argument: a reference to 'a'.
//
// The regression this file exists to catch: the four calls are structurally
// identical -- same argument, same arity, same node type -- so the system task
// *name* is the only place the database records which member of the $strobe
// family the source actually wrote. A test that checked merely "four
// SysTaskCalls with one argument each" would pass just as happily if the
// compiler collapsed $strobeb, $strobeo and $strobeh onto $strobe, or dropped
// the radix suffix. Hence the names are pinned individually and in order, and
// each argument is checked to resolve back to the very 'a' the module
// declares rather than merely to be a reference spelled "a".
//
// What is deliberately NOT checked, and why:
//   - Source line numbers and design-level typespec positions. They are
//     incidental to how the source is laid out rather than determined by
//     what it says.
//   - The actual output the four calls would produce, and the radix each one
//     renders 'a' in. Clause 21.2 also makes $strobe print at the END of the
//     current time step, after all other events for that step have settled --
//     which is a scheduling property, observable only by advancing time. HLC
//     is a static compiler/elaborator and is not a simulator: there is no
//     console, output stream or scheduler in the model, and the four
//     SysTaskCall nodes carry only their name strings and argument lists.
//     This is out of scope permanently rather than a gap waiting to be
//     filled, so no placeholder test is carried for it;
//     FourStrobeVariantsCalledInSourceOrder is the static evidence that the
//     four variants stayed distinct and in order.
//
// Gated on elaboration rather than dropped:
//   - Root-module status (getTopModules() / Module::getTopModule()). IEEE
//     1800 23.3.1 makes 'top' a root module -- nothing in the file
//     instantiates it -- and it is the only module declared, so it is the
//     design's sole root. That is an elaboration fact, not a simulation one:
//     it is something HLC has to compute, and it lives in the model once the
//     elaboration pass has run. This fixture's command file
//     (21.2--strobe.hlc) runs a non-elaborating compile flow, so the flag
//     stays unset here and asserting it unconditionally fails for a reason
//     the source does not determine. An earlier version of this file hit
//     exactly that and DELETED the assertion, which was the wrong fix: it
//     traded away a real check on elaborated flows to keep a parse-only flow
//     green. TopIsTheDesignsOnlyRootModule below restores it behind
//     m_design->getElaborated() instead, so the check runs wherever the flag
//     is meaningful and is simply inert where it is not.

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>

#include <array>
#include <string_view>
#include <vector>

namespace hlc { namespace {

// Size of a possibly-null hldb collection. hldb leaves a collection null
// rather than empty when nothing is ever added to it, so "absent" and "empty"
// are the same observation and both must read as 0.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  return (collection == nullptr) ? 0u : collection->size();
}

class StrobeTaskTest : public Test {
 protected:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.2--strobe.hlc"}); }

  static void TearDownTestSuite() { Shutdown(); }

  // 'module top' is the only module the file declares, so it is the only entry
  // the design can hold.
  static const hldb::Module *topModule() {
    EXPECT_NE(m_design, nullptr);
    if (m_design == nullptr) return nullptr;

    EXPECT_EQ(sizeOf(m_design->getAllModules()), 1u);
    if (sizeOf(m_design->getAllModules()) != 1u) return nullptr;

    return m_design->getAllModules()->front();
  }

  // The single 'always' procedure of 'top'.
  static const hldb::Always *alwaysProcedure(const hldb::Module *top) {
    EXPECT_EQ(sizeOf(top->getProcesses()), 1u);
    if (sizeOf(top->getProcesses()) != 1u) return nullptr;

    return any_cast<hldb::Always>(top->getProcesses()->front());
  }
};

TEST_F(StrobeTaskTest, TopModuleIsTop) {
  const hldb::Module *const top = topModule();
  ASSERT_NE(top, nullptr);

  // The declaration header is 'module top', so both the module's name and its
  // definition name are the declared identifier 'top'.
  EXPECT_EQ(top->getName(), "top");
  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top->getAnyType(), hldb::AnyType::Module);
  EXPECT_EQ(top->getVpiType(), static_cast<uint32_t>(vpiModule));

  // 'module top();' -- the port list is present but empty.
  EXPECT_EQ(sizeOf(top->getPorts()), 0u);

  // The body declares no nets, instantiates no modules, and has no continuous
  // assignments, generate blocks or parameters.
  EXPECT_EQ(sizeOf(top->getNets()), 0u);
  EXPECT_EQ(sizeOf(top->getModules()), 0u);
  EXPECT_EQ(sizeOf(top->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(top->getGenScopeArrays()), 0u);
  EXPECT_EQ(sizeOf(top->getParameters()), 0u);

  // 'endmodule' carries no ': top' end label.
  EXPECT_EQ(top->getEndLabel(), "");

  // 'always @(posedge clk) ... end' is the only procedure in the module.
  EXPECT_EQ(sizeOf(top->getProcesses()), 1u);
}

TEST_F(StrobeTaskTest, TopIsTheDesignsOnlyRootModule) {
  ASSERT_NE(m_design, nullptr);

  // IEEE 1800 23.3.1: a module that no other module instantiates is a root
  // (top-level) module. Nothing in this file instantiates 'top', and 'top' is
  // the only module the file declares, so it is the design's sole root module.
  //
  // Root-module status is only computed by the elaboration pass, and this
  // fixture's command file runs a non-elaborating compile flow, so the check
  // is gated: it runs wherever the flag is meaningful and stays inert where it
  // is not. Everything asserted in the other cases lives in the module
  // definition that the compile step builds, so those stay unconditional.
  if (m_design->getElaborated()) {
    ASSERT_EQ(sizeOf(m_design->getTopModules()), 1u);

    const hldb::Module *const root = m_design->getTopModules()->front();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->getName(), "top");
    EXPECT_TRUE(root->getTopModule());
  }
}

TEST_F(StrobeTaskTest, ModuleDeclaresClkAndA) {
  const hldb::Module *const top = topModule();
  ASSERT_NE(top, nullptr);

  // 'logic clk;' and 'int a;' are data declarations, so both are variables
  // rather than nets, and they are the only two.
  ASSERT_EQ(sizeOf(top->getVariables()), 2u);

  const hldb::Variable *const clk = hldb::findByName<hldb::Variable>("clk", top->getVariables());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getName(), "clk");
  EXPECT_EQ(clk->getAnyType(), hldb::AnyType::Variable);
  // Declared at module level, hence static lifetime and not a constant.
  EXPECT_FALSE(clk->getAutomatic());
  EXPECT_FALSE(clk->getConstantVariable());

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
  EXPECT_EQ(a->getAnyType(), hldb::AnyType::Variable);
  EXPECT_FALSE(a->getAutomatic());
  EXPECT_FALSE(a->getConstantVariable());
}

TEST_F(StrobeTaskTest, ClkIsAnUnsignedScalarLogic) {
  const hldb::Module *const top = topModule();
  ASSERT_NE(top, nullptr);

  const hldb::Variable *const clk = hldb::findByName<hldb::Variable>("clk", top->getVariables());
  ASSERT_NE(clk, nullptr);

  const hldb::LogicTypespec *const typespec = hldb::getTypespec<hldb::LogicTypespec>(clk);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(typespec->getAnyType(), hldb::AnyType::LogicTypespec);
  EXPECT_EQ(typespec->getVpiType(), static_cast<uint32_t>(vpiLogicTypespec));

  // IEEE 1800 6.8: 'logic' is a 4-state *unsigned* type. Written without a
  // packed dimension it is a single bit, so it is not a vector and carries no
  // ranges.
  EXPECT_FALSE(typespec->getSigned());
  EXPECT_FALSE(typespec->getVector());
  EXPECT_EQ(sizeOf(typespec->getRanges()), 0u);
}

TEST_F(StrobeTaskTest, AIsASignedInt) {
  const hldb::Module *const top = topModule();
  ASSERT_NE(top, nullptr);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);

  const hldb::IntTypespec *const typespec = hldb::getTypespec<hldb::IntTypespec>(a);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(typespec->getAnyType(), hldb::AnyType::IntTypespec);
  EXPECT_EQ(typespec->getVpiType(), static_cast<uint32_t>(vpiIntTypespec));

  // IEEE 1800 6.11.1: 'int' is the 2-state 32-bit *signed* integer type, and
  // it takes no packed dimensions.
  EXPECT_TRUE(typespec->getSigned());
  EXPECT_EQ(sizeOf(typespec->getRanges()), 0u);
}

TEST_F(StrobeTaskTest, AlwaysIsEdgeTriggeredOnPosedgeClk) {
  const hldb::Module *const top = topModule();
  ASSERT_NE(top, nullptr);

  const hldb::Always *const always = alwaysProcedure(top);
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAnyType(), hldb::AnyType::Always);
  EXPECT_EQ(always->getVpiType(), static_cast<uint32_t>(vpiAlways));

  // The source writes the general-purpose 'always', not always_comb /
  // always_ff / always_latch.
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);

  // '@(posedge clk)' is the timing control that fronts the procedure body.
  const hldb::EventControl *const eventControl = always->getStmt<hldb::EventControl>();
  ASSERT_NE(eventControl, nullptr);
  EXPECT_EQ(eventControl->getAnyType(), hldb::AnyType::EventControl);
  EXPECT_EQ(eventControl->getVpiType(), static_cast<uint32_t>(vpiEventControl));

  // A single edge-qualified event: 'posedge clk'.
  const hldb::Operation *const posedge = eventControl->getCondition<hldb::Operation>();
  ASSERT_NE(posedge, nullptr);
  EXPECT_EQ(posedge->getOpType(), vpiPosedgeOp);
  ASSERT_EQ(sizeOf(posedge->getOperands()), 1u);

  const hldb::RefObj *const clkRef = any_cast<hldb::RefObj>(posedge->getOperands()->front());
  ASSERT_NE(clkRef, nullptr);
  EXPECT_EQ(clkRef->getName(), "clk");
  EXPECT_EQ(clkRef->getAnyType(), hldb::AnyType::RefObj);

  // The event expression names the module's own 'clk' variable.
  const hldb::Variable *const clk = hldb::findByName<hldb::Variable>("clk", top->getVariables());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clkRef->getActual<hldb::Variable>(), clk);
}

TEST_F(StrobeTaskTest, BodyIsAnUnnamedBeginWithFourStatements) {
  const hldb::Module *const top = topModule();
  ASSERT_NE(top, nullptr);

  const hldb::Always *const always = alwaysProcedure(top);
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const eventControl = always->getStmt<hldb::EventControl>();
  ASSERT_NE(eventControl, nullptr);

  const hldb::Begin *const body = eventControl->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->getAnyType(), hldb::AnyType::Begin);
  EXPECT_EQ(body->getVpiType(), static_cast<uint32_t>(vpiBegin));

  // 'begin' carries no ': label', so 'end' carries no end label either.
  EXPECT_EQ(body->getEndLabel(), "");

  // The block declares nothing; it only holds the four $strobe* calls.
  EXPECT_EQ(sizeOf(body->getVariables()), 0u);
  EXPECT_EQ(sizeOf(body->getNets()), 0u);
  EXPECT_EQ(sizeOf(body->getStmts()), 4u);
}

TEST_F(StrobeTaskTest, FourStrobeVariantsCalledInSourceOrder) {
  const hldb::Module *const top = topModule();
  ASSERT_NE(top, nullptr);

  const hldb::Always *const always = alwaysProcedure(top);
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const eventControl = always->getStmt<hldb::EventControl>();
  ASSERT_NE(eventControl, nullptr);
  const hldb::Begin *const body = eventControl->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(sizeOf(body->getStmts()), 4u);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);

  // IEEE 1800 21.2: $strobe and its default-radix variants, in the order the
  // fixture writes them.
  const std::array<std::string_view, 4> expectedNames = {"$strobe", "$strobeb", "$strobeo", "$strobeh"};

  for (size_t i = 0; i < expectedNames.size(); ++i) {
    SCOPED_TRACE(expectedNames[i]);

    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(body->getStmts()->at(i));
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getAnyType(), hldb::AnyType::SysTaskCall);
    EXPECT_EQ(call->getVpiType(), static_cast<uint32_t>(vpiSysTaskCall));
    EXPECT_EQ(call->getName(), expectedNames[i]);

    // All four are built-in display tasks, so none is a user-defined systf and
    // none resolves to a user-written task declaration.
    EXPECT_FALSE(call->getUserDefn());
    EXPECT_EQ(call->getUserSystf(), nullptr);
    EXPECT_EQ(call->getTaskFunc(), nullptr);

    // Each call passes exactly one argument: the variable 'a'.
    ASSERT_EQ(sizeOf(call->getArguments()), 1u);
    const hldb::RefObj *const argument = any_cast<hldb::RefObj>(call->getArguments()->front());
    ASSERT_NE(argument, nullptr);
    EXPECT_EQ(argument->getName(), "a");
    EXPECT_EQ(argument->getAnyType(), hldb::AnyType::RefObj);
    EXPECT_EQ(argument->getActual<hldb::Variable>(), a);
  }
}

TEST_F(StrobeTaskTest, ClkAndAResolveWithoutBindingErrors) {
  // 'clk' and 'a' are both declared in 'top' and both referenced from the
  // always procedure, so neither reference may be reported as undefined.
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, "clk"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, "a"), nullptr);
}

TEST_F(StrobeTaskTest, StrobeVariantsAreRecognizedStrobedMonitoringTasks) {
  // IEEE 1800 clause 21.2 "Display system tasks" splits the family into
  // subclauses: 21.2.1 is the display and write tasks, 21.2.2 is strobed
  // monitoring ($strobe and its b/o/h radix variants), and 21.2.3 is
  // continuous monitoring ($monitor). $strobe therefore belongs to the
  // strobed-monitoring group and NOT to the display/write group, and clause
  // 21.2 defines all of them as tasks rather than functions.
  const std::array<std::string_view, 4> names = {"$strobe", "$strobeb", "$strobeo", "$strobeh"};

  for (std::string_view name : names) {
    SCOPED_TRACE(name);
    EXPECT_TRUE(hldb::isStrobedMonitoringTaskName(name)) << "21.2.2: $strobe* are the strobed monitoring tasks";
    EXPECT_FALSE(hldb::isDisplayWriteTaskName(name))
        << "21.2.1 vs 21.2.2: $strobe* must not be collapsed into the display/write group";
    EXPECT_TRUE(hldb::isSystemTaskName(name));
    EXPECT_FALSE(hldb::isSystemFuncName(name)) << "21.2: these are tasks, not functions";
  }
}

}}  // namespace hlc

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
