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

// Validates the HLDB model built for tests/PartSelectElab/dut.sv:
//
//   module test(output reg state, input clk);
//   reg [15:0] data;
//
//   always @(posedge clk) begin
//     data[7:0] <= data[7:0] + 2;
//   end
//
//   endmodule // test
//
// The fixture is a part-select (11.5.1) used both as the target and as an
// operand of a nonblocking assignment (10.4.2) inside an edge-controlled
// always procedure (9.2.2, 9.4.2).
//
// What is checked, and why:
//   - test has two ports, state (output) then clk (input), indexed 0 and 1
//     in list order (23.2.2.2, 37.14 detail 9)
//   - state and data are variables (6.8: 'reg' declares a variable); clk is
//     a net; data is logic [15:0] (6.11, 7.4.1)
//   - the only process is an 'always' (9.2.2) whose statement is an event
//     control on posedge clk (9.4.2), bound to the net clk
//   - the body is a begin-end block with one nonblocking assignment (10.4.2)
//   - both 'data[7:0]' are part-selects of the variable data with bounds 7
//     and 0 (11.5.1)
//   - the RHS is an addition (11.4.3) of data[7:0] and the literal 2
//   - no fatal, syntax or error diagnostic: the file is legal
//
// What is NOT checked, and why:
//   - The value data takes on a clock edge is a runtime property. HLC is a
//     static compiler/elaborator and not a simulator, so this is out of scope
//     permanently; RhsAddsPartSelectAndTwo is the static evidence.
//   - The size of the part-select ranges: HLC leaves Range sizes unset before
//     elaboration, and no .hlc in this suite elaborates; the bounds are
//     asserted instead.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PartSelectElabTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PartSelectElab.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTest() { return hldb::findByDefName<hldb::Module>("test", m_design->getAllModules()); }

  static const hldb::Always *getAlways() {
    const hldb::Module *const m = getTest();
    if ((m == nullptr) || (m->getProcesses() == nullptr) || (m->getProcesses()->size() != 1)) return nullptr;
    return any_cast<hldb::Always>(m->getProcesses()->at(0));
  }

  static const hldb::Assignment *getAssignment() {
    const hldb::Always *const a = getAlways();
    if (a == nullptr) return nullptr;
    const hldb::EventControl *const ec = a->getStmt<hldb::EventControl>();
    if (ec == nullptr) return nullptr;
    const hldb::Begin *const b = ec->getStmt<hldb::Begin>();
    if ((b == nullptr) || (b->getStmts() == nullptr) || (b->getStmts()->size() != 1)) return nullptr;
    return any_cast<hldb::Assignment>(b->getStmts()->at(0));
  }

  static void ExpectConstant(const hldb::Expr *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }

  // Verifies 'ps' is data[7:0].
  static void ExpectData7To0(const hldb::PartSelect *ps, std::string_view what) {
    ASSERT_NE(ps, nullptr) << "11.5.1: " << what << " is a part-select";
    const hldb::RefObj *const prefix = ps->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr) << what << ": the selected object is named";
    EXPECT_EQ(prefix->getName(), "data");
    const hldb::Variable *const data = hldb::findByName<hldb::Variable>("data", getTest()->getVariables());
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(prefix->getActual(), data) << what << ": 'data' binds to the module variable";
    ASSERT_NE(ps->getRange(), nullptr) << what << ": a part-select has a range";
    ExpectConstant(ps->getRange()->getLeftExpr(), "7", "11.5.1: the msb expression");
    ExpectConstant(ps->getRange()->getRightExpr(), "0", "11.5.1: the lsb expression");
  }
};

TEST_F(PartSelectElabTest, TestIsTheOnlyModule) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  ASSERT_EQ(m_design->getAllModules()->size(), 1u);
  EXPECT_EQ(m_design->getAllModules()->at(0)->getDefName(), "test");
}

TEST_F(PartSelectElabTest, PortsAreStateThenClk) {
  const hldb::Module *const m = getTest();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 2u) << "23.2.2.2: the list of ports names 2 ports";
  const hldb::Port *const state = m->getPorts()->at(0);
  const hldb::Port *const clk = m->getPorts()->at(1);
  EXPECT_EQ(state->getName(), "state");
  EXPECT_EQ(state->getDirection(), vpiOutput);
  EXPECT_EQ(state->getPortIndex(), 0) << "37.14 detail 9: the first port has index 0";
  EXPECT_EQ(clk->getName(), "clk");
  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_EQ(clk->getPortIndex(), 1);
}

TEST_F(PartSelectElabTest, StateAndDataAreVariablesClkIsNet) {
  const hldb::Module *const m = getTest();
  ASSERT_NE(m, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("state", m->getVariables()), nullptr)
      << "6.8: 'output reg state' declares a variable";
  EXPECT_NE(hldb::findByName<hldb::Net>("clk", m->getNets()), nullptr) << "23.2.2.3: 'input clk' is a net";
  EXPECT_EQ(hldb::findByName<hldb::Net>("data", m->getNets()), nullptr) << "6.8: 'reg' declares a variable";
}

TEST_F(PartSelectElabTest, DataIsLogic15To0) {
  const hldb::Module *const m = getTest();
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const data = hldb::findByName<hldb::Variable>("data", m->getVariables());
  ASSERT_NE(data, nullptr);
  ASSERT_NE(data->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = data->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "6.11.2: reg is a 4-state logic type";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u) << "7.4.1: one packed dimension";
  ExpectConstant(lt->getRanges()->at(0)->getLeftExpr(), "15", "7.4.1: left bound");
  ExpectConstant(lt->getRanges()->at(0)->getRightExpr(), "0", "7.4.1: right bound");
}

TEST_F(PartSelectElabTest, AlwaysOnPosedgeClk) {
  const hldb::Always *const a = getAlways();
  ASSERT_NE(a, nullptr) << "9.2.2: the module has one always procedure";
  EXPECT_EQ(a->getAlwaysType(), vpiAlways) << "9.2.2: a general-purpose 'always'";
  const hldb::EventControl *const ec = a->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr) << "9.4.2: '@(...)' is an event control";
  const hldb::Operation *const edge = any_cast<hldb::Operation>(ec->getCondition());
  ASSERT_NE(edge, nullptr);
  EXPECT_EQ(edge->getOpType(), vpiPosedgeOp) << "9.4.2: posedge";
  ASSERT_NE(edge->getOperands(), nullptr);
  ASSERT_EQ(edge->getOperands()->size(), 1u);
  const hldb::RefObj *const clk = any_cast<hldb::RefObj>(edge->getOperands()->at(0));
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getName(), "clk");
  EXPECT_EQ(clk->getActual(), hldb::findByName<hldb::Net>("clk", getTest()->getNets())) << "binds to the port net";
}

TEST_F(PartSelectElabTest, BodyIsOneNonblockingAssignment) {
  const hldb::Assignment *const as = getAssignment();
  ASSERT_NE(as, nullptr) << "9.3.1: the begin-end block holds one statement, an assignment";
  EXPECT_FALSE(as->getBlocking()) << "10.4.2: '<=' is a nonblocking assignment";
}

TEST_F(PartSelectElabTest, LhsIsData7To0) {
  const hldb::Assignment *const as = getAssignment();
  ASSERT_NE(as, nullptr);
  ExpectData7To0(as->getLhs<hldb::PartSelect>(), "the LHS");
}

TEST_F(PartSelectElabTest, RhsAddsPartSelectAndTwo) {
  const hldb::Assignment *const as = getAssignment();
  ASSERT_NE(as, nullptr);
  const hldb::Operation *const add = as->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getOpType(), vpiAddOp) << "11.4.3: binary '+'";
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  ExpectData7To0(any_cast<hldb::PartSelect>(add->getOperands()->at(0)), "the left operand");
  ExpectConstant(any_cast<hldb::Expr>(add->getOperands()->at(1)), "2", "the right operand");
}

TEST_F(PartSelectElabTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
