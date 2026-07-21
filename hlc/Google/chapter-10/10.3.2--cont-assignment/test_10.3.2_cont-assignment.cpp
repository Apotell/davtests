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

// Tests for 10.3.2--cont-assignment.sv (tags: 10.3.2)
//   module top(input a, input b);
//     wire w;
//     assign w = a & b;
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 3 nets: "a", "b", "w", all
//     vpiNetType wire, each RefTypespec -> LogicTypespec
//   - module has exactly 2 ports: "a" (input), "b" (input)
//   - module has exactly 1 continuous assignment: lhs RefObj "w" resolving
//     Net "w", rhs Operation (bitwise-and) with 2 RefObj operands ("a",
//     "b"), no delay -- unlike 10.3.1--net-decl-assignment.sv, declaring
//     "wire w;" separately from "assign w = a & b;" produces a real
//     ContAssign item instead of populating Net::getValue()
//   - design-level typespecs (1): ModuleTypespec only
//   - compiler emits zero errors
//   - no processes

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
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

class ContAssignmentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "10.3.2--cont-assignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / nets / ports -----------------------------------------------

TEST_F(ContAssignmentTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ContAssignmentTest, ModuleHasThreeNetsAllWire) {
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

TEST_F(ContAssignmentTest, NetWHasNoOwnInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("w", top->getNets());
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getValue(), nullptr)
      << "unlike 10.3.1--net-decl-assignment.sv, a separately-declared 'wire w;' plus 'assign w = ...' "
         "should NOT populate Net::getValue()";
}

// --- continuous assignment -------------------------------------------------

TEST_F(ContAssignmentTest, HasOneContAssignWEqualsAAndB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "w");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
  const hldb::Operation *const op = ca->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiBitAndOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(1))->getName(), "b");
  EXPECT_EQ(ca->getDelay(), nullptr);
}

// --- design-level typespecs / compiler diagnostics -----------------------

TEST_F(ContAssignmentTest, DesignHasOneTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 1u);
}

TEST_F(ContAssignmentTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(ContAssignmentTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(ContAssignmentTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
