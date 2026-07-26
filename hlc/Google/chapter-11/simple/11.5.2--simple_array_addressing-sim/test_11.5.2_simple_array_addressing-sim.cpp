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

// Tests for 11.5.2--simple_array_addressing-sim.sv (tags: 11.5.2)
//   module top(input [7:0] a, output [7:0] b);
//     reg [7:0] mem [0:255];
//     assign b = mem[a];
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 nets: "a" [7:0] (input),
//     "b" [7:0] (output), "mem" (memory array)
//   - net "mem": RefTypespec -> ArrayTypespec static(1), unpacked range
//     [0:255], elem -> LogicTypespec [7:0] -- "reg" maps to LogicTypespec
//     (reg is not a distinct typespec kind), matching the analogous
//     finding in chapter-7/structures and chapter-7/arrays/packed/basic
//   - module has exactly 1 continuous assignment: lhs RefObj "b", rhs
//     BitSelect "mem[a]": vpiPrefix RefObj "mem" resolving Net "mem",
//     vpiIndex RefObj "a" resolving Net "a" -- a VARIABLE index (not a
//     Constant), since "a" is the runtime address into the memory
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
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SimpleArrayAddressingSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.5.2--simple_array_addressing-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ---------------------------------------------------------

TEST_F(SimpleArrayAddressingSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SimpleArrayAddressingSimTest, ModuleHasThreeNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 3u);
}

TEST_F(SimpleArrayAddressingSimTest, NetMemIsArrayZeroToTwoFiveFiveOfEightBitLogic) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const mem = hldb::findByName<hldb::Net>("mem", top->getNets());
  ASSERT_NE(mem, nullptr);
  const hldb::ArrayTypespec *const at = mem->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(at->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "255");
  const hldb::LogicTypespec *const elem = at->getElemTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(elem, nullptr);
  ASSERT_NE(elem->getRanges(), nullptr);
  ASSERT_EQ(elem->getRanges()->size(), 1u);
  EXPECT_EQ(elem->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(elem->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- continuous assignment: array addressing with a variable index ---------

TEST_F(SimpleArrayAddressingSimTest, ContAssignIsMemIndexedByVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getLhs<hldb::RefObj>()->getName(), "b");
  const hldb::BitSelect *const sel = ca->getRhs<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getName(), "mem[a]");
  EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "mem");
  EXPECT_NE(sel->getPrefix<hldb::RefObj>()->getActual<hldb::Net>(), nullptr);
  const hldb::RefObj *const index = sel->getIndex<hldb::RefObj>();
  ASSERT_NE(index, nullptr) << "'mem[a]' should have a RefObj index (variable 'a'), not a Constant";
  EXPECT_EQ(index->getName(), "a");
  EXPECT_NE(index->getActual<hldb::Net>(), nullptr);
}

// --- design-level typespecs / compiler diagnostics -----------------------

TEST_F(SimpleArrayAddressingSimTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(SimpleArrayAddressingSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(SimpleArrayAddressingSimTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
