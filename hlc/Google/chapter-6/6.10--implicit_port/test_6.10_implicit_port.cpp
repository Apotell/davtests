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

// Tests for 6.10--implicit_port.sv (tags: 6.10)
//   module top(input [3:0] a, input [3:0] b);
//     wire [3:0] c;
//     assign c = a | b;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.10 "Implicit declarations",
// p.108, checked before any test code was written):
//   "If an identifier is used in a port expression declaration, then an
//   implicit net of default net type shall be assumed, with the vector
//   width of the port expression declaration." "a" and "b" appear only
//   in the ANSI port header ("input [3:0] a", "input [3:0] b") with no
//   separate explicit net declaration in the module body -- this is
//   exactly the circumstance the spec describes, and it is legal, not
//   an error. "c" is a normal, explicitly declared internal wire. This
//   file has no :should_fail_because: tag -- it is legal per spec.
//
//   Unlike 6.10--implicit_continuous_assignment.sv and
//   6.10--implicit_port_connection.sv (where HLC's EL0535 "Illegal
//   implicit net" false-positive is already confirmed), this file's
//   implicit nets come from port expression declarations (6.10's FIRST
//   bullet) rather than a continuous-assignment LHS or a port
//   connection list (the second and third bullets) -- a prior version
//   of this test never checked the compiler's error count at all, so it
//   is unknown whether this circumstance triggers the same bug. Adding
//   a zero-errors check here closes that gap.
//
// What is checked:
//   - module top exists, has exactly 3 nets: 'a', 'b' (implicit, from
//     port declarations) and 'c' (explicit internal wire)
//   - module has exactly 2 ports: 'a' and 'b', both vpiInput direction
//   - 'c' is NOT a port (internal wire only)
//   - port 'a' and 'b' lowConn RefObj each resolve to the corresponding
//     Net
//   - 1 ContAssign: LHS RefObj "c" has vpiActual (formally declared),
//     RHS = vpiBitOrOp(a, b)
//   - net 'c' has no initial value; 'a' and 'b' have no initial values
//   - top has no processes
//   - net type of 'a', 'b', 'c' is vpiWire (implicit wire from
//     port/internal declarations)
//   - compiler reports zero errors (this file is fully legal per 6.10's
//     first bullet -- previously never checked)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ImplicitPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.10--implicit_port.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ImplicitPortTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Nets -- a, b (from ports) and c (internal wire) are all formally declared
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortTest, ThreeNetsExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u) << "expected nets a, b, and c";
}

TEST_F(ImplicitPortTest, ANetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "net 'a' not found";
}

TEST_F(ImplicitPortTest, BNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("b", top->getNets()), nullptr) << "net 'b' not found";
}

TEST_F(ImplicitPortTest, CNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr)
      << "net 'c' not found -- it is a formally declared internal wire";
}

TEST_F(ImplicitPortTest, ANetTypeIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getNetType(), vpiWire) << "expected vpiNetType wire (1) for implicit-wire port 'a'";
}

TEST_F(ImplicitPortTest, BNetTypeIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getNetType(), vpiWire) << "expected vpiNetType wire (1) for implicit-wire port 'b'";
}

TEST_F(ImplicitPortTest, CNetTypeIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getNetType(), vpiWire) << "expected vpiNetType wire (1) for 'wire [3:0] c'";
}

TEST_F(ImplicitPortTest, ANetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr) << "port 'a' has no initializer";
}

TEST_F(ImplicitPortTest, BNetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Any>(), nullptr) << "port 'b' has no initializer";
}

// ---------------------------------------------------------------------------
// Ports -- only a and b; c is an internal wire, not a port
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortTest, TwoPortsExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  EXPECT_EQ(top->getPorts()->size(), 2u) << "only a and b are ports; c is an internal wire";
}

TEST_F(ImplicitPortTest, PortAIsInput) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Port *const pa = hldb::findByName<hldb::Port>("a", top->getPorts());
  ASSERT_NE(pa, nullptr) << "port 'a' not found";
  EXPECT_EQ(pa->getDirection(), vpiInput);
}

TEST_F(ImplicitPortTest, PortBIsInput) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Port *const pb = hldb::findByName<hldb::Port>("b", top->getPorts());
  ASSERT_NE(pb, nullptr) << "port 'b' not found";
  EXPECT_EQ(pb->getDirection(), vpiInput);
}

TEST_F(ImplicitPortTest, CIsNotAPort) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Port>("c", top->getPorts()), nullptr)
      << "'c' should not appear in vpiPort -- it is an internal wire only";
}

TEST_F(ImplicitPortTest, PortALowConnResolvesToNetA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Port *const pa = hldb::findByName<hldb::Port>("a", top->getPorts());
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lc = pa->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'a' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'a' lowConn does not resolve to a Net";
  EXPECT_EQ(net->getName(), "a");
}

TEST_F(ImplicitPortTest, PortBLowConnResolvesToNetB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Port *const pb = hldb::findByName<hldb::Port>("b", top->getPorts());
  ASSERT_NE(pb, nullptr);
  const hldb::RefObj *const lc = pb->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'b' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'b' lowConn does not resolve to a Net";
  EXPECT_EQ(net->getName(), "b");
}

// ---------------------------------------------------------------------------
// Continuous assignment -- assign c = a | b
// ---------------------------------------------------------------------------
TEST_F(ImplicitPortTest, ContAssignExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(ImplicitPortTest, ContAssignLhsIsCWithActual) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "c");
  EXPECT_NE(lhs->getActual(), nullptr) << "'c' is formally declared -- LHS RefObj should have a vpiActual";
}

TEST_F(ImplicitPortTest, ContAssignRhsIsBitOr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitOrOp) << "expected vpiBitOrOp (29)";
}

TEST_F(ImplicitPortTest, BitOrOperandsAreAAndB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
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

TEST_F(ImplicitPortTest, CNetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue<hldb::Any>(), nullptr)
      << "internal wire 'c' has no initializer -- only gets a value from the assign";
}

TEST_F(ImplicitPortTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(ImplicitPortTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0)
      << "IEEE 1800-2023 6.10: an identifier used only in a port expression declaration ('input "
         "[3:0] a') gets an implicit net of default type -- this is legal, not an error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
