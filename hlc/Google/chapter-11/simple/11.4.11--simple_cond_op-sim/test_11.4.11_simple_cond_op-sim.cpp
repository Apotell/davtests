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

// Tests for 11.4.11--simple_cond_op-sim.sv (tags: 11.4.11)
//   module top(input a, output b);
//     assign b = (a) ? 0 : 1;
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 2 nets: "a" (input), "b"
//     (output), both vpiNetType wire, each RefTypespec -> LogicTypespec
//   - module has exactly 1 continuous assignment: lhs RefObj "b", rhs
//     Operation (vpiOpType=condition) with 3 operands: RefObj "a", Constant
//     "0", Constant "1"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//   - no processes
//
// Not checked:
//   - this file is annotated "(without result verification)" and has no
//     $display assertions at all, so there is no runtime value to check
//     even in principle -- purely a parsing/elaboration structure test.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
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

class SimpleCondOpSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.11--simple_cond_op-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / nets / ports -----------------------------------------------

TEST_F(SimpleCondOpSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SimpleCondOpSimTest, ModuleHasTwoNetsAllWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 2u);
  const char *const names[2] = {"a", "b"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_EQ(net->getNetType(), vpiWire);
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
  }
}

TEST_F(SimpleCondOpSimTest, ModuleHasInputAAndOutputB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  const hldb::Port *const a = any_cast<hldb::Port>(top->getPorts()->at(0));
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getDirection(), vpiInput);
  const hldb::Port *const b = any_cast<hldb::Port>(top->getPorts()->at(1));
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getDirection(), vpiOutput);
}

// --- continuous assignment: conditional operator ----------------------------

TEST_F(SimpleCondOpSimTest, ContAssignIsConditionalOperator) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getLhs<hldb::RefObj>()->getName(), "b");
  const hldb::Operation *const op = ca->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConditionOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 3u);
  const hldb::RefObj *const cond = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "a");
  EXPECT_EQ(any_cast<hldb::Constant>(op->getOperands()->at(1))->getDecompile(), "0");
  EXPECT_EQ(any_cast<hldb::Constant>(op->getOperands()->at(2))->getDecompile(), "1");
}

// --- design-level typespecs / compiler diagnostics -----------------------

TEST_F(SimpleCondOpSimTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(SimpleCondOpSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(SimpleCondOpSimTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
