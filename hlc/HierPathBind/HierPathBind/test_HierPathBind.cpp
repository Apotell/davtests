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

// Tests for HierPathBind/dut.sv (tags: HierPathBind)
//   module dut (output logic o);
//      assign o = 1'b1;
//   endmodule
//
//   module top (output logic o);
//      typedef struct packed { logic ping_p; } alert_rx_t;
//      alert_rx_t  alert_rx_i;
//      dut d(.o(alert_rx_i.ping_p));
//      assign o = alert_rx_i.ping_p;
//   endmodule
//
// Checked (per IEEE 1800-2023 Sec 23.6 -- Hierarchical names, Sec 23.3.2.3 --
// Named port connections):
//   - "alert_rx_i.ping_p" is a dotted hierarchical-name-shaped reference to a
//     member of the packed struct variable "alert_rx_i", modeled the same
//     way as any other hierarchical path: a RefObj with getPathElems() ==
//     ["alert_rx_i", "ping_p"], resolving via getActual() to a TypespecMember
//     for "ping_p"
//   - it resolves identically whether used as a named port-connection actual
//     (".o(alert_rx_i.ping_p)", bound to instance "d"'s port "o" via
//     Ports::getHighConn()) or as the rhs of a continuous assignment
//   - "alert_rx_i" itself is a Variable (never a net, per Sec 6.8) of struct
//     type

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/ports.h>
#include <hldb/ref_obj.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class HierPathBindTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathBind.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static void checkHierPathToPingP(const hldb::RefObj *const rhs) {
    ASSERT_NE(rhs, nullptr) << "expected a RefObj hierarchical path";
    EXPECT_EQ(rhs->getName(), std::string_view{"alert_rx_i.ping_p"});
    ASSERT_NE(rhs->getPathElems(), nullptr);
    ASSERT_EQ(rhs->getPathElems()->size(), 2u);
    EXPECT_EQ(rhs->getPathElems()->at(0)->getName(), std::string_view{"alert_rx_i"});
    EXPECT_EQ(rhs->getPathElems()->at(1)->getName(), std::string_view{"ping_p"});

    ASSERT_NE(rhs->getActual(), nullptr);
    const hldb::TypespecMember *const member = rhs->getActual<hldb::TypespecMember>();
    ASSERT_NE(member, nullptr) << "'alert_rx_i.ping_p' should resolve to the struct member 'ping_p'";
    EXPECT_EQ(member->getName(), std::string_view{"ping_p"});
  }
};

TEST_F(HierPathBindTest, ModulesExist) {
  EXPECT_NE(getTop(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Module>("dut", m_design->getAllModules()), nullptr);
}

TEST_F(HierPathBindTest, AlertRxIIsAStructVariableNotNet) {
  // Per IEEE 1800-2023 Sec 6.7/6.8: with no net-type keyword, "alert_rx_i" is
  // a variable regardless of `default_nettype.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const alertRxI = hldb::findByName<hldb::Variable>("alert_rx_i", top->getVariables());
  ASSERT_NE(alertRxI, nullptr);
}

TEST_F(HierPathBindTest, ContAssignRhsIsHierPathToPingP) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::ContAssign *ca = nullptr;
  for (const hldb::ContAssign *const c : *top->getContAssigns()) {
    const hldb::RefObj *const lhs = c->getLhs<hldb::RefObj>();
    if ((lhs != nullptr) && (lhs->getName() == "o")) {
      ca = c;
      break;
    }
  }
  ASSERT_NE(ca, nullptr) << "continuous assignment 'o = alert_rx_i.ping_p' not found";
  ASSERT_NE(ca->getRhs(), nullptr);
  checkHierPathToPingP(ca->getRhs<hldb::RefObj>());
}

TEST_F(HierPathBindTest, NamedPortConnectionHighConnIsHierPathToPingP) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getModules(), nullptr);
  const hldb::Module *const dInst = hldb::findByName<hldb::Module>("d", top->getModules());
  ASSERT_NE(dInst, nullptr) << "instance 'd' not found under 'top'";
  ASSERT_NE(dInst->getPorts(), nullptr);
  const hldb::Ports *const oPort = hldb::findByName<hldb::Ports>("o", dInst->getPorts());
  ASSERT_NE(oPort, nullptr);
  ASSERT_NE(oPort->getHighConn(), nullptr);
  checkHierPathToPingP(oPort->getHighConn<hldb::RefObj>());
}

TEST_F(HierPathBindTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
