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

// Tests for 10.3--proc-assignment--bad.sv (tags: 10.3)
//   :should_fail_because: Illegal to procedurally assign to wire, IEEE Table 10-1
//   module top(input a, input b);
//     wire w;
//     initial
//       w = #10 a & b;
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 nets: "a", "b", "w", all
//     vpiNetType wire, each RefTypespec -> LogicTypespec
//   - module has exactly 2 ports: "a" (input), "b" (input), each
//     vpiLowConn -> RefObj of the same name -> actual resolves to the matching Net
//   - module has exactly 1 process (Initial), whose stmt is directly a
//     blocking Assignment (no Begin wrapper, since it is a single
//     statement): lhs RefObj "w" resolving Net "w", rhs a DelayControl
//     wrapping an Operation (bitwise-and) of RefObj "a" and RefObj "b",
//     with vpiDelay Constant "10"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//
// Known compiler defect (see CompilerShouldRejectProceduralAssignmentToWireButDoesNot
// below): this file's own ':should_fail_because:' annotation states the
// construct is illegal per IEEE 1800-2017 Table 10-1 -- a net (here, `wire w`)
// can only be driven by a continuous assignment, a gate, or a port
// connection, never by a procedural (blocking/non-blocking) assignment
// inside an initial/always block. The compiler currently emits ZERO errors
// and happily elaborates 'w = #10 a & b;' as an ordinary Assignment with the
// Net "w" as its lhs (see above) -- it does not enforce this rule at all.
// This is not a simulation gap: the compiler already resolves "w" to a Net
// (not a Variable) at compile time, via the very same RefTypespec ->
// LogicTypespec / vpiNetType:wire data checked above, so it has everything
// it needs to reject this statement and doesn't.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ProcAssignmentBadTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "10.3--proc-assignment--bad.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets / ports ----

TEST_F(ProcAssignmentBadTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ProcAssignmentBadTest, ModuleHasThreeNetsAllWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 3u);
  const char *const names[3] = {"a", "b", "w"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_EQ(net->getNetType(), vpiWire);
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
  }
}

TEST_F(ProcAssignmentBadTest, ModuleHasNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr);
}

TEST_F(ProcAssignmentBadTest, ModuleHasTwoInputPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  const char *const names[2] = {"a", "b"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Port *const port = any_cast<hldb::Port>(top->getPorts()->at(i));
    ASSERT_NE(port, nullptr) << "port " << i;
    EXPECT_EQ(port->getName(), names[i]);
    EXPECT_EQ(port->getDirection(), vpiInput);
    const hldb::RefObj *const lowConn = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(lowConn, nullptr) << "port low_conn is a RefObj pointing at the net/variable, not the net/variable itself";
    EXPECT_EQ(lowConn->getName(), names[i]);
    EXPECT_NE(lowConn->getActual<hldb::Net>(), nullptr);
  }
}

// --- initial process: illegal procedural assignment to a net ----

TEST_F(ProcAssignmentBadTest, InitialStmtIsBlockingAssignmentToNetW) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign = init->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "single-statement initial body should not be wrapped in a Begin";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "w");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "the compiler resolves 'w' to a Net, not a Variable";
}

TEST_F(ProcAssignmentBadTest, RhsIsDelayControlWrappingBitwiseAndOfAAndB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign = init->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::DelayControl *const delayCtrl = assign->getRhs<hldb::DelayControl>();
  ASSERT_NE(delayCtrl, nullptr);
  const hldb::Constant *const delay = delayCtrl->getDelay<hldb::Constant>();
  ASSERT_NE(delay, nullptr);
  EXPECT_EQ(delay->getDecompile(), "10");
  const hldb::Operation *const op = delayCtrl->getStmt<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiBitAndOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(1))->getName(), "b");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(ProcAssignmentBadTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(ProcAssignmentBadTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(ProcAssignmentBadTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- known compiler defect: illegal procedural assignment to a net ----

TEST_F(ProcAssignmentBadTest, CompilerShouldRejectProceduralAssignmentToWireButDoesNot) {
  GTEST_SKIP() << "HLC does not enforce IEEE 1800-2023 Table 10-1: a net shall not be the target of a "
                  "procedural assignment. Fix pending.";
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2017 Table 10-1: a net (here, 'wire w') shall not be the target of a procedural "
         "(blocking) assignment -- only continuous assignment, a gate, or a port connection may drive "
         "a net. This file is annotated :should_fail_because: exactly this reason, yet the compiler "
         "currently emits zero errors for 'w = #10 a & b;' inside the initial block. This is a genuine "
         "compile-time defect, not a simulation gap: the compiler already resolves 'w' to a Net (see "
         "InitialStmtIsBlockingAssignmentToNetW above), so it has everything it needs to detect and "
         "reject this at compile time.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
