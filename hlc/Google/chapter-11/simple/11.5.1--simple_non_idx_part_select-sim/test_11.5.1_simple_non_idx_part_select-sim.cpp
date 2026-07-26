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

// Tests for 11.5.1--simple_non_idx_part_select-sim.sv (tags: 11.5.1)
//   module top(input [7:0] a, output [1:0] b);
//     assign b = a[7:6];
//   endmodule
//
// Checked:
//   - design has module top with exactly 2 nets: "a" [7:0] (input),
//     "b" [1:0] (output), both vpiNetType wire
//   - module has exactly 1 continuous assignment: lhs RefObj "b", rhs
//     PartSelect "a[7:6]": vpiPrefix RefObj "a" resolving Net "a", vpiRange
//     left Constant "7" right Constant "6"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//   - no processes
//
// Not checked:
//   - this file is annotated "(without result verification)" and has no
//     $display assertions, so there is no runtime value to check even in
//     principle.

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
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SimpleNonIdxPartSelectSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.5.1--simple_non_idx_part_select-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ---------------------------------------------------------

TEST_F(SimpleNonIdxPartSelectSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SimpleNonIdxPartSelectSimTest, ModuleHasTwoNetsAllWire) {
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

// --- continuous assignment: non-indexed (constant-range) part-select -------

TEST_F(SimpleNonIdxPartSelectSimTest, ContAssignIsASevenToSixPartSelect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getLhs<hldb::RefObj>()->getName(), "b");
  const hldb::PartSelect *const sel = ca->getRhs<hldb::PartSelect>();
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getName(), "a[7:6]");
  EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "a");
  ASSERT_NE(sel->getRange(), nullptr);
  EXPECT_EQ(sel->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(sel->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "6");
}

// --- design-level typespecs / compiler diagnostics -----------------------

TEST_F(SimpleNonIdxPartSelectSimTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(SimpleNonIdxPartSelectSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(SimpleNonIdxPartSelectSimTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
