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

// Tests for dut.sv (tags: JKFlipflop)
//   module JKFlipflop(J,K,clk,reset,q);
//     input J,K,clk,reset;
//     output q;
//     wire w;
//     assign w=(J&~q)|(~K&q);
//     D_Flipflop dff(w,clk,reset,q);
//   endmodule
//
//   module D_Flipflop(Din,clk,reset,q);
//     input Din,clk,reset;
//     output reg q;
//     always@(posedge clk)
//     begin
//     if(reset)
//     q=1'b0;
//     else
//     q=Din;
//     end
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - both modules are present in the design (23.2)
//   - non-ANSI port lists (23.2.2.1, 23.2.2.3): the first port has no
//     direction/kind/type so all ports are non-ANSI; port order and
//     directions come from the later "input"/"output" declarations
//   - JKFlipflop: J/K/clk/reset/q have no net/var keyword and no data type,
//     so they are implicit nets of the default net type (wire) (23.2.2.3,
//     22.8); "wire w" is an explicit wire net (6.7)
//   - "assign w=(J&~q)|(~K&q);" is a continuous assignment (10.3.2) whose
//     RHS is BitOr(BitAnd(J, ~q), BitAnd(~K, q)) (11.4.8, 11.3.2 precedence
//     with the explicit parentheses)
//   - "D_Flipflop dff(w,clk,reset,q);" is a module instance of D_Flipflop
//     named "dff" connected by ordered list (23.3.2.1): the i-th actual
//     connects to the i-th formal (Din, clk, reset, q)
//   - D_Flipflop: "output reg q" declares q as a variable (reg is a
//     variable data type, 6.11.2 / 23.2.2.3), not a net
//   - the always procedure (9.2.2) is "always" with an event control
//     @(posedge clk) (9.4.2) whose body is a begin-end holding an if-else
//     (12.4) with two blocking assignments (10.4.1); "1'b0" is a sized
//     binary literal of width 1 (5.7.1)
//
// What is NOT checked and why:
//   - elaborated / top-module views: this .hlc does not elaborate, so
//     getTopModules() and instance hierarchy are not populated by design.
//   - the "-ast" warning (CM5010) about an ignored command line argument:
//     tool CLI behavior, not something IEEE 1800 specifies.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class JKFlipflopTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "JKFlipflop.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Operation *asOp(const hldb::Any *any) { return any_cast<hldb::Operation>(any); }
  static const hldb::RefObj *asRef(const hldb::Any *any) { return any_cast<hldb::RefObj>(any); }

  static void checkPorts(const hldb::Module *mod, const std::vector<std::string_view> &names,
                         const std::vector<int32_t> &dirs) {
    ASSERT_NE(mod->getPorts(), nullptr);
    ASSERT_EQ(mod->getPorts()->size(), names.size());
    for (size_t i = 0; i < names.size(); ++i) {
      const hldb::Port *const p = mod->getPorts()->at(i);
      ASSERT_NE(p, nullptr);
      EXPECT_EQ(p->getName(), names[i]) << "port index " << i;
      EXPECT_EQ(p->getDirection(), dirs[i]) << "port " << names[i];
    }
  }
};

// ===========================================================================
// Modules
// ===========================================================================

TEST_F(JKFlipflopTest, BothModulesExist) {
  EXPECT_NE(getModule("JKFlipflop"), nullptr);
  EXPECT_NE(getModule("D_Flipflop"), nullptr);
}

// ===========================================================================
// JKFlipflop ports and nets (23.2.2.3)
// ===========================================================================

TEST_F(JKFlipflopTest, JKFlipflopPortsInOrderWithDirections) {
  const hldb::Module *const jk = getModule("JKFlipflop");
  ASSERT_NE(jk, nullptr);
  checkPorts(jk, {"J", "K", "clk", "reset", "q"}, {vpiInput, vpiInput, vpiInput, vpiInput, vpiOutput});
}

TEST_F(JKFlipflopTest, JKFlipflopPortsAreImplicitWireNets) {
  const hldb::Module *const jk = getModule("JKFlipflop");
  ASSERT_NE(jk, nullptr);
  for (std::string_view name : {"J", "K", "clk", "reset", "q"}) {
    const hldb::Net *const n = hldb::findByName<hldb::Net>(name, jk->getNets());
    ASSERT_NE(n, nullptr) << "23.2.2.3: port '" << name << "' with no kind/type is a net of default net type";
    EXPECT_EQ(n->getNetType(), vpiWire) << name;
    EXPECT_TRUE(n->getScalar()) << name << " has no packed dimension";
  }
}

TEST_F(JKFlipflopTest, WireWIsExplicitWireNet) {
  const hldb::Module *const jk = getModule("JKFlipflop");
  ASSERT_NE(jk, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("w", jk->getNets());
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getNetType(), vpiWire);
  EXPECT_TRUE(w->getScalar());
  EXPECT_EQ(hldb::findByName<hldb::Variable>("w", jk->getVariables()), nullptr) << "'wire w' is a net, not a variable";
}

// ===========================================================================
// assign w=(J&~q)|(~K&q);  (10.3.2, 11.4.8)
// ===========================================================================

TEST_F(JKFlipflopTest, ContAssignShape) {
  const hldb::Module *const jk = getModule("JKFlipflop");
  ASSERT_NE(jk, nullptr);
  ASSERT_NE(jk->getContAssigns(), nullptr);
  ASSERT_EQ(jk->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = jk->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);

  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "w");
  ASSERT_NE(lhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Net);

  const hldb::Operation *const orOp = ca->getRhs<hldb::Operation>();
  ASSERT_NE(orOp, nullptr);
  EXPECT_EQ(orOp->getOpType(), vpiBitOrOp);
  ASSERT_NE(orOp->getOperands(), nullptr);
  ASSERT_EQ(orOp->getOperands()->size(), 2u);

  // (J & ~q)
  const hldb::Operation *const and1 = asOp(orOp->getOperands()->at(0));
  ASSERT_NE(and1, nullptr);
  EXPECT_EQ(and1->getOpType(), vpiBitAndOp);
  ASSERT_NE(and1->getOperands(), nullptr);
  ASSERT_EQ(and1->getOperands()->size(), 2u);
  const hldb::RefObj *const j = asRef(and1->getOperands()->at(0));
  ASSERT_NE(j, nullptr);
  EXPECT_EQ(j->getName(), "J");
  const hldb::Operation *const notQ = asOp(and1->getOperands()->at(1));
  ASSERT_NE(notQ, nullptr);
  EXPECT_EQ(notQ->getOpType(), vpiBitNegOp);
  ASSERT_NE(notQ->getOperands(), nullptr);
  ASSERT_EQ(notQ->getOperands()->size(), 1u);
  const hldb::RefObj *const q1 = asRef(notQ->getOperands()->at(0));
  ASSERT_NE(q1, nullptr);
  EXPECT_EQ(q1->getName(), "q");

  // (~K & q)
  const hldb::Operation *const and2 = asOp(orOp->getOperands()->at(1));
  ASSERT_NE(and2, nullptr);
  EXPECT_EQ(and2->getOpType(), vpiBitAndOp);
  ASSERT_NE(and2->getOperands(), nullptr);
  ASSERT_EQ(and2->getOperands()->size(), 2u);
  const hldb::Operation *const notK = asOp(and2->getOperands()->at(0));
  ASSERT_NE(notK, nullptr);
  EXPECT_EQ(notK->getOpType(), vpiBitNegOp);
  ASSERT_NE(notK->getOperands(), nullptr);
  ASSERT_EQ(notK->getOperands()->size(), 1u);
  const hldb::RefObj *const k = asRef(notK->getOperands()->at(0));
  ASSERT_NE(k, nullptr);
  EXPECT_EQ(k->getName(), "K");
  const hldb::RefObj *const q2 = asRef(and2->getOperands()->at(1));
  ASSERT_NE(q2, nullptr);
  EXPECT_EQ(q2->getName(), "q");
}

// ===========================================================================
// D_Flipflop dff(w,clk,reset,q);  (23.3.2.1 ordered list)
// ===========================================================================

TEST_F(JKFlipflopTest, InstanceDffOfDFlipflop) {
  const hldb::Module *const jk = getModule("JKFlipflop");
  ASSERT_NE(jk, nullptr);
  ASSERT_NE(jk->getRefInstances(), nullptr);
  ASSERT_EQ(jk->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const dff = jk->getRefInstances()->at(0);
  ASSERT_NE(dff, nullptr);
  EXPECT_EQ(dff->getName(), "dff");
  ASSERT_NE(dff->getTypespec(), nullptr);
  EXPECT_EQ(dff->getTypespec()->getName(), "D_Flipflop");
}

TEST_F(JKFlipflopTest, InstanceDffOrderedConnections) {
  const hldb::Module *const jk = getModule("JKFlipflop");
  ASSERT_NE(jk, nullptr);
  ASSERT_NE(jk->getRefInstances(), nullptr);
  ASSERT_EQ(jk->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const dff = jk->getRefInstances()->at(0);
  ASSERT_NE(dff, nullptr);
  ASSERT_NE(dff->getPorts(), nullptr);
  ASSERT_EQ(dff->getPorts()->size(), 4u);
  const std::vector<std::string_view> formals = {"Din", "clk", "reset", "q"};
  const std::vector<std::string_view> actuals = {"w", "clk", "reset", "q"};
  for (size_t i = 0; i < 4; ++i) {
    const hldb::Port *const p = any_cast<hldb::Port>(dff->getPorts()->at(i));
    ASSERT_NE(p, nullptr) << "connection " << i;
    const hldb::RefObj *const hi = p->getHighConn<hldb::RefObj>();
    ASSERT_NE(hi, nullptr) << "connection " << i;
    EXPECT_EQ(hi->getName(), actuals[i]);
    const hldb::RefObj *const lo = p->getLowConn<hldb::RefObj>();
    ASSERT_NE(lo, nullptr) << "connection " << i;
    EXPECT_EQ(lo->getName(), formals[i]) << "23.3.2.1: i-th actual binds the i-th formal";
  }
}

// ===========================================================================
// D_Flipflop ports: "output reg q" is a variable
// ===========================================================================

TEST_F(JKFlipflopTest, DFlipflopPortsInOrderWithDirections) {
  const hldb::Module *const d = getModule("D_Flipflop");
  ASSERT_NE(d, nullptr);
  checkPorts(d, {"Din", "clk", "reset", "q"}, {vpiInput, vpiInput, vpiInput, vpiOutput});
}

TEST_F(JKFlipflopTest, DFlipflopInputsAreWireNets) {
  const hldb::Module *const d = getModule("D_Flipflop");
  ASSERT_NE(d, nullptr);
  for (std::string_view name : {"Din", "clk", "reset"}) {
    const hldb::Net *const n = hldb::findByName<hldb::Net>(name, d->getNets());
    ASSERT_NE(n, nullptr) << name;
    EXPECT_EQ(n->getNetType(), vpiWire) << name;
  }
}

TEST_F(JKFlipflopTest, DFlipflopOutputRegQIsVariable) {
  const hldb::Module *const d = getModule("D_Flipflop");
  ASSERT_NE(d, nullptr);
  const hldb::Variable *const q = hldb::findByName<hldb::Variable>("q", d->getVariables());
  ASSERT_NE(q, nullptr) << "'output reg q' declares a variable";
  EXPECT_TRUE(q->getScalar());
  EXPECT_EQ(hldb::findByName<hldb::Net>("q", d->getNets()), nullptr) << "'output reg q' must not be a net";

  const hldb::Port *const p = hldb::findByName<hldb::Port>("q", d->getPorts());
  ASSERT_NE(p, nullptr);
  const hldb::RefObj *const lo = p->getLowConn<hldb::RefObj>();
  ASSERT_NE(lo, nullptr);
  ASSERT_NE(lo->getActual(), nullptr);
  EXPECT_EQ(lo->getActual()->getAnyType(), hldb::AnyType::Variable);
}

// ===========================================================================
// always@(posedge clk) begin if(reset) q=1'b0; else q=Din; end
// ===========================================================================

TEST_F(JKFlipflopTest, AlwaysWithPosedgeClk) {
  const hldb::Module *const d = getModule("D_Flipflop");
  ASSERT_NE(d, nullptr);
  ASSERT_NE(d->getProcesses(), nullptr);
  ASSERT_EQ(d->getProcesses()->size(), 1u);
  const hldb::Always *const alw = any_cast<hldb::Always>(d->getProcesses()->at(0));
  ASSERT_NE(alw, nullptr);
  EXPECT_EQ(alw->getAlwaysType(), vpiAlways);

  const hldb::EventControl *const ec = alw->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const pos = ec->getCondition<hldb::Operation>();
  ASSERT_NE(pos, nullptr);
  EXPECT_EQ(pos->getOpType(), vpiPosedgeOp);
  ASSERT_NE(pos->getOperands(), nullptr);
  ASSERT_EQ(pos->getOperands()->size(), 1u);
  const hldb::RefObj *const clk = asRef(pos->getOperands()->at(0));
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getName(), "clk");
}

TEST_F(JKFlipflopTest, AlwaysBodyIfElseBlockingAssignments) {
  const hldb::Module *const d = getModule("D_Flipflop");
  ASSERT_NE(d, nullptr);
  ASSERT_NE(d->getProcesses(), nullptr);
  ASSERT_EQ(d->getProcesses()->size(), 1u);
  const hldb::Always *const alw = any_cast<hldb::Always>(d->getProcesses()->at(0));
  ASSERT_NE(alw, nullptr);
  const hldb::EventControl *const ec = alw->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Begin *const blk = ec->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const hldb::IfElse *const ie = any_cast<hldb::IfElse>(blk->getStmts()->at(0));
  ASSERT_NE(ie, nullptr);

  const hldb::RefObj *const cond = ie->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "reset");

  const hldb::Assignment *const thenA = ie->getStmt<hldb::Assignment>();
  ASSERT_NE(thenA, nullptr);
  EXPECT_TRUE(thenA->getBlocking());
  const hldb::RefObj *const thenL = thenA->getLhs<hldb::RefObj>();
  ASSERT_NE(thenL, nullptr);
  EXPECT_EQ(thenL->getName(), "q");
  const hldb::Constant *const zero = thenA->getRhs<hldb::Constant>();
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getConstType(), vpiBinaryConst);
  EXPECT_EQ(zero->getSize(), 1);

  const hldb::Assignment *const elseA = ie->getElseStmt<hldb::Assignment>();
  ASSERT_NE(elseA, nullptr);
  EXPECT_TRUE(elseA->getBlocking());
  const hldb::RefObj *const elseL = elseA->getLhs<hldb::RefObj>();
  ASSERT_NE(elseL, nullptr);
  EXPECT_EQ(elseL->getName(), "q");
  const hldb::RefObj *const din = elseA->getRhs<hldb::RefObj>();
  ASSERT_NE(din, nullptr);
  EXPECT_EQ(din->getName(), "Din");
}

TEST_F(JKFlipflopTest, AllIdentifiersBind) {
  for (std::string_view name : {"J", "K", "q", "w", "clk", "reset", "Din"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr) << "'" << name << "' is declared";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
