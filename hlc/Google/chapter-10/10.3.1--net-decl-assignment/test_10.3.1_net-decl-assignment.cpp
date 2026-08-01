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

// Tests for 10.3.1--net-decl-assignment.sv (tags: 10.3.1)
//   module top(input a, input b);
//     wire w = a & b;
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 nets: "a", "b", "w", all
//     vpiNetType wire, each RefTypespec -> LogicTypespec
//   - module has exactly 2 ports: "a" (input), "b" (input)
//   - net "w"'s initializer is captured directly on the Net itself via
//     getValue() (NOT as a separate ContAssign item) -- an Operation
//     (bitwise-and) with 2 RefObj operands resolving Net "a" and Net "b"
//   - module has no continuous assignments (getContAssigns() is null): the
//     net-declaration-assignment sugar 'wire w = a & b;' elaborates purely
//     as Net::getValue(), unlike a separate 'assign w = a & b;' statement
//     (compare chapter-10/10.3.2--cont-assignment.sv, which does produce a
//     ContAssign)
//   - design-level typespecs (1): ModuleTypespec only -- no IntTypespec,
//     since the initializer has no literal Constant operand
//   - compiler emits zero errors
//   - no processes

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
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

class NetDeclAssignmentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "10.3.1--net-decl-assignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets / ports ----

TEST_F(NetDeclAssignmentTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(NetDeclAssignmentTest, ModuleHasThreeNetsAllWire) {
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

TEST_F(NetDeclAssignmentTest, ModuleHasNoVariables) {
  // Per IEEE 1800-2023 Sec 6.7/6.8: "a", "b", "w" all use the explicit net
  // keywords (ports default to net for input/inout; "wire w" is explicit),
  // so none of them should also appear in the Variables collection.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr);
}

TEST_F(NetDeclAssignmentTest, ModuleHasTwoInputPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
}

TEST_F(NetDeclAssignmentTest, NetWValueIsBitwiseAndOfAAndB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("w", top->getNets());
  ASSERT_NE(w, nullptr);
  const hldb::Operation *const op = w->getValue<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "net-declaration-assignment initializer should be captured on Net::getValue()";
  EXPECT_EQ(op->getOpType(), vpiBitAndOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const lhsOperand = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhsOperand, nullptr);
  EXPECT_EQ(lhsOperand->getName(), "a");
  EXPECT_NE(lhsOperand->getActual<hldb::Net>(), nullptr);
  const hldb::RefObj *const rhsOperand = any_cast<hldb::RefObj>(op->getOperands()->at(1));
  ASSERT_NE(rhsOperand, nullptr);
  EXPECT_EQ(rhsOperand->getName(), "b");
  EXPECT_NE(rhsOperand->getActual<hldb::Net>(), nullptr);
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(NetDeclAssignmentTest, DesignHasOneTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 1u);
}

TEST_F(NetDeclAssignmentTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(NetDeclAssignmentTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(NetDeclAssignmentTest, NoContAssignsAndNoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
