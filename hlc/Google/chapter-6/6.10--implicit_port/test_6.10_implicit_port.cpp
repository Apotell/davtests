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

// Validates the UHDM graph for a module with two input ports and one internal
// wire used in a continuous assignment:
//   module top(input [3:0] a, input [3:0] b);
//     wire [3:0] c;
//     assign c = a | b;
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 3 nets: 'a', 'b' (from ports) and 'c' (internal wire)
//   - module has exactly 2 ports: 'a' and 'b', both vpiInput direction
//   - 'c' is NOT a port (internal wire only)
//   - port 'a' and 'b' lowConn RefObj each resolve to the corresponding Net
//   - 1 ContAssign: LHS RefObj "c" has vpiActual (formally declared), RHS = vpiBitOrOp(a, b)
//   - net 'c' has no initial value
//   - work@top has no processes
//
// Not checked:
//   - net type of 'a', 'b', 'c' (vpiLogic from wire declarations)
//   - 'a' and 'b' have no initial values

#include <hlc/Common/Session.h>
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

class ImplicitPort : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.10--implicit_port.hlc"});

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

TEST_F(ImplicitPort, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Nets — a, b (from ports) and c (internal wire) are all formally declared
// ---------------------------------------------------------------------------
TEST_F(ImplicitPort, ThreeNetsExist) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u) << "expected nets a, b, and c";
}

TEST_F(ImplicitPort, ANetExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr)
      << "net 'a' not found";
}

TEST_F(ImplicitPort, BNetExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("b", top->getNets()), nullptr)
      << "net 'b' not found";
}

TEST_F(ImplicitPort, CNetExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr)
      << "net 'c' not found — it is a formally declared internal wire";
}

// ---------------------------------------------------------------------------
// Ports — only a and b; c is an internal wire, not a port
// ---------------------------------------------------------------------------
TEST_F(ImplicitPort, TwoPortsExist) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  EXPECT_EQ(top->getPorts()->size(), 2u)
      << "only a and b are ports; c is an internal wire";
}

TEST_F(ImplicitPort, PortAIsInput) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Port *const pa =
      hldb::findByName<hldb::Port>("a", top->getPorts());
  ASSERT_NE(pa, nullptr) << "port 'a' not found";
  EXPECT_EQ(pa->getDirection(), vpiInput);
}

TEST_F(ImplicitPort, PortBIsInput) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Port *const pb =
      hldb::findByName<hldb::Port>("b", top->getPorts());
  ASSERT_NE(pb, nullptr) << "port 'b' not found";
  EXPECT_EQ(pb->getDirection(), vpiInput);
}

TEST_F(ImplicitPort, CIsNotAPort) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Port>("c", top->getPorts()), nullptr)
      << "'c' should not appear in vpiPort — it is an internal wire only";
}

TEST_F(ImplicitPort, PortALowConnResolvesToNetA) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Port *const pa =
      hldb::findByName<hldb::Port>("a", top->getPorts());
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lc = pa->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'a' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'a' lowConn does not resolve to a Net";
  EXPECT_EQ(net->getName(), "a");
}

TEST_F(ImplicitPort, PortBLowConnResolvesToNetB) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Port *const pb =
      hldb::findByName<hldb::Port>("b", top->getPorts());
  ASSERT_NE(pb, nullptr);
  const hldb::RefObj *const lc = pb->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'b' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'b' lowConn does not resolve to a Net";
  EXPECT_EQ(net->getName(), "b");
}

// ---------------------------------------------------------------------------
// Continuous assignment — assign c = a | b
// ---------------------------------------------------------------------------
TEST_F(ImplicitPort, ContAssignExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(ImplicitPort, ContAssignLhsIsCWithActual) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "c");
  EXPECT_NE(lhs->getActual(), nullptr)
      << "'c' is formally declared — LHS RefObj should have a vpiActual";
}

TEST_F(ImplicitPort, ContAssignRhsIsBitOr) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const rhs =
      top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitOrOp)
      << "expected vpiBitOrOp (29)";
}

TEST_F(ImplicitPort, BitOrOperandsAreAAndB) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const rhs =
      top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::RefObj *const op0 =
      any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  const hldb::RefObj *const op1 =
      any_cast<hldb::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(op0, nullptr) << "first operand is not a RefObj";
  ASSERT_NE(op1, nullptr) << "second operand is not a RefObj";
  EXPECT_EQ(op0->getName(), "a");
  EXPECT_EQ(op1->getName(), "b");
}

TEST_F(ImplicitPort, CNetHasNoInitialValue) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue<hldb::Any>(), nullptr)
      << "internal wire 'c' has no initializer — only gets a value from the assign";
}

TEST_F(ImplicitPort, NoProcesses) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
