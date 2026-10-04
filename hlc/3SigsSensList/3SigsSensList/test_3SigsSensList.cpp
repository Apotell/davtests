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

// Tests for dut.sv (tags: 3SigsSensList)
//   module dut (
//   input wire clk,
//   input wire rst,
//   input wire start
//   );
//   always @ (posedge clk or posedge rst or posedge start)
//   begin
//   	if(rst | start)
//   	begin
//   	  outputLine <= 0;
//   	end
//   	else
//   	begin
//   	 yScaleAmountNext <= 0;
//   	end
//   end
//   endmodule
//
// A regular "always" process (not always_comb/always_ff) with a 3-signal
// sensitivity list combined with "or". "outputLine" and "yScaleAmountNext"
// are never declared anywhere in the module -- they are referenced only as
// assignment targets, so their RefObj nodes resolve to no symbol at all
// (getActual() is null) rather than an implicitly-declared net.
//
// Checked:
//   - module "dut" has exactly 3 ports/nets (clk, rst, start), all scalar
//     input wires
//   - module has exactly 1 process, and it is an Always (not Initial,
//     AlwaysComb, etc) whose vpiAlwaysType is the plain "always" (1)
//   - the Always's statement is an EventControl whose condition is an
//     "event or" of 3 "posedge" operations, over clk, rst, start in that
//     order
//   - the EventControl's statement is a Begin (since the source has an
//     explicit "begin ... end") containing a single IfElse
//   - the IfElse's condition is "rst | start" (bitwise or)
//   - the IfElse's true branch is a Begin with one nonblocking Assignment
//     of constant 0 to "outputLine"; the false branch is a Begin with one
//     nonblocking Assignment of constant 0 to "yScaleAmountNext"
//   - "outputLine" and "yScaleAmountNext" are undeclared: their RefObj
//     resolves to no symbol (getActual() is null)
//   - compiler reports zero errors
//
// NOT CHECKED: runtime behavior (what value outputLine/yScaleAmountNext
// would hold) cannot be observed -- HLC is a compiler/elaborator with no
// simulation capability, so no execution ever happens for this test to
// check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SigsSensListTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "3SigsSensList.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Always *getAlwaysProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Always>(mod->getProcesses()->at(0));
  }

  static const hldb::EventControl *getEventControl() {
    const hldb::Always *const always = getAlwaysProcess();
    if (always == nullptr) {
      return nullptr;
    }
    return always->getStmt<hldb::EventControl>();
  }

  static const hldb::IfElse *getIfElse() {
    const hldb::EventControl *const eventControl = getEventControl();
    if (eventControl == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const begin = eventControl->getStmt<hldb::Begin>();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::IfElse>(begin->getStmts()->at(0));
  }

  static const hldb::Assignment *getBranchAssignment(const hldb::Any *branchStmt) {
    const hldb::Begin *const begin = any_cast<hldb::Begin>(branchStmt);
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  }
};

// --- module / ports -------------------------------------------------------

TEST_F(SigsSensListTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(SigsSensListTest, ModuleHasThreeScalarInputWirePorts) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  EXPECT_EQ(mod->getPorts()->size(), 3u);

  for (std::string_view name : {"clk", "rst", "start"}) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, mod->getNets());
    ASSERT_NE(net, nullptr) << "Net '" << name << "' is null";
    EXPECT_EQ(net->getNetType(), vpiWire);
    EXPECT_TRUE(net->getScalar());

    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, mod->getPorts());
    ASSERT_NE(port, nullptr) << "Port '" << name << "' is null";
    EXPECT_EQ(port->getDirection(), vpiInput);
  }
}

// --- always process / sensitivity list -------------------------------------

TEST_F(SigsSensListTest, ModuleHasOneAlwaysProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  const hldb::Always *const always = getAlwaysProcess();
  ASSERT_NE(always, nullptr) << "process should be an Always, not e.g. an Initial";
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(SigsSensListTest, SensitivityListIsEventOrOfThreePosedges) {
  const hldb::EventControl *const eventControl = getEventControl();
  ASSERT_NE(eventControl, nullptr) << "Always's statement should be an EventControl directly";

  const hldb::Operation *const eventOr = eventControl->getCondition<hldb::Operation>();
  ASSERT_NE(eventOr, nullptr);
  EXPECT_EQ(eventOr->getOpType(), vpiEventOrOp);
  ASSERT_NE(eventOr->getOperands(), nullptr);
  ASSERT_EQ(eventOr->getOperands()->size(), 3u);

  static constexpr std::string_view kExpectedSignals[3] = {"clk", "rst", "start"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Operation *const posedge = any_cast<hldb::Operation>(eventOr->getOperands()->at(i));
    ASSERT_NE(posedge, nullptr) << "operand " << i << " should be a posedge Operation";
    EXPECT_EQ(posedge->getOpType(), vpiPosedgeOp);
    ASSERT_NE(posedge->getOperands(), nullptr);
    ASSERT_EQ(posedge->getOperands()->size(), 1u);
    const hldb::RefObj *const signal = any_cast<hldb::RefObj>(posedge->getOperands()->at(0));
    ASSERT_NE(signal, nullptr);
    EXPECT_EQ(signal->getName(), kExpectedSignals[i]);
  }
}

// --- if (rst | start) begin ... end else begin ... end --------------------

TEST_F(SigsSensListTest, IfConditionIsRstOrStart) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr) << "EventControl's statement should be a Begin with a single IfElse statement";

  const hldb::Operation *const condition = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getOpType(), vpiBitOrOp);
  ASSERT_NE(condition->getOperands(), nullptr);
  ASSERT_EQ(condition->getOperands()->size(), 2u);

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(condition->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "rst");

  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(condition->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "start");
}

TEST_F(SigsSensListTest, ThenBranchAssignsZeroToUndeclaredOutputLine) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);

  const hldb::Assignment *const assign = getBranchAssignment(ifElse->getStmt());
  ASSERT_NE(assign, nullptr) << "then-branch should be a Begin with one Assignment";
  EXPECT_FALSE(assign->getBlocking()) << "'<=' is a nonblocking assignment";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "outputLine");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'outputLine' is never declared -- it must not resolve to any symbol";

  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(SigsSensListTest, ElseBranchAssignsZeroToUndeclaredYScaleAmountNext) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);

  const hldb::Assignment *const assign = getBranchAssignment(ifElse->getElseStmt());
  ASSERT_NE(assign, nullptr) << "else-branch should be a Begin with one Assignment";
  EXPECT_FALSE(assign->getBlocking()) << "'<=' is a nonblocking assignment";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "yScaleAmountNext");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'yScaleAmountNext' is never declared -- it must not resolve to any symbol";

  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(SigsSensListTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
