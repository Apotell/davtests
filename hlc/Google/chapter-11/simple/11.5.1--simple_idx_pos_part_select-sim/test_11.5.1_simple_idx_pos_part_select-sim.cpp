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

// Tests for 11.5.1--simple_idx_pos_part_select-sim.sv (tags: 11.5.1)
//   module top(input [15:0] a, output [3:0] b);
//     assign b = a[0+:4];
//   endmodule
//
// Checked:
//   - design has module top with exactly 2 nets: "a" [15:0] (input),
//     "b" [3:0] (output), both vpiNetType wire. Per IEEE 1800-2023 Sec
//     6.7/23.2.2.3: an input port always defaults to a net, and an
//     output port with no explicit data type also defaults to a net, so
//     both being nets here is correct; module has no variables
//     (getVariables() is null)
//   - module has exactly 1 continuous assignment: lhs RefObj "b", rhs
//     IndexedPartSelect "a[0+:4]": vpiPrefix RefObj "a" resolving Net "a",
//     vpiIndexedPartSelectType=pos indexed, vpiBaseExpr Constant "0",
//     vpiWidthExpr Constant "4"
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
#include <hldb/indexed_part_select.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SimpleIdxPosPartSelectSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.5.1--simple_idx_pos_part_select-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----

TEST_F(SimpleIdxPosPartSelectSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SimpleIdxPosPartSelectSimTest, ModuleHasTwoNetsAllWire) {
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

TEST_F(SimpleIdxPosPartSelectSimTest, ModuleHasNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr) << "both ports default to nets per IEEE 1800-2023 "
                                              "Sec 6.7/23.2.2.3, so the module should have no "
                                              "variables";
}

// --- continuous assignment: indexed positive part-select ----

TEST_F(SimpleIdxPosPartSelectSimTest, ContAssignIsAPosIndexedZeroPlusFour) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getLhs<hldb::RefObj>()->getName(), "b");
  const hldb::IndexedPartSelect *const sel = ca->getRhs<hldb::IndexedPartSelect>();
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getName(), "a[0+:4]");
  EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "a");
  EXPECT_EQ(sel->getIndexedPartSelectType(), vpiPosIndexed);
  EXPECT_EQ(sel->getBaseExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(sel->getWidthExpr<hldb::Constant>()->getDecompile(), "4");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(SimpleIdxPosPartSelectSimTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(SimpleIdxPosPartSelectSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(SimpleIdxPosPartSelectSimTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
