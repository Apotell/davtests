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

// Tests for nonexistent.sv (tags: 7.8.6)
//   module top ();
//     int arr[int]; int r;
//     initial begin
//       arr[10] = 10;
//       $display(":assert: (%d == 1)", arr.size);
//       $display(":re: BEGIN:ARRAY_NONEXISTENT");
//       r = arr[9];
//       $display(":re: END");
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 2 nets: 'arr' (assoc ArrayTypespec, idx=int, elem=int) and 'r' (IntTypespec)
//   - 1 Initial process; Begin with 5 stmts
//   - stmt[0]: blocking Assignment arr[10]=10 (BitSelect lhs, Constant rhs)
//   - stmt[1]: $display(2 args) — HierPath "arr.size" as second arg
//   - stmt[2]: $display(1 arg) — Constant ":re: BEGIN:ARRAY_NONEXISTENT"
//   - stmt[3]: blocking Assignment r=arr[9] (RefObj lhs, BitSelect rhs)
//   - stmt[4]: $display(1 arg) — Constant ":re: END"
//   - work@top has no continuous assignments
//
// Not checked:
//   - HLC emits EL0535 for arr.size (implicit net) but still puts HierPath in UHDM
//   - runtime behavior (arr[9] is nonexistent, returns default int value 0)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>

namespace hlc {

class Nonexistent : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "nonexistent.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ---------------------------------------------------------------

TEST_F(Nonexistent, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- net declarations -----------------------------------------------------

TEST_F(Nonexistent, ModuleHasTwoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(Nonexistent, ArrNetHasAssociativeArrayTypespec) {
  // int arr[int] -> ArrayTypespec associative(3)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr = top->getNets()->at(0);
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->getName(), "arr");
  const hldb::RefTypespec *const rt = arr->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(Nonexistent, AssocArrayKeyTypeIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Nonexistent, AssocArrayValueTypeIsInt) {
  // element type is IntTypespec (from `int arr[int]`)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Nonexistent, RNetHasIntTypespec) {
  // int r; -> Net "r" -> RefTypespec -> IntTypespec
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const r = top->getNets()->at(1);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getName(), "r");
  const hldb::RefTypespec *const rt = r->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::IntTypespec>(), nullptr);
}

// --- initial block --------------------------------------------------------

TEST_F(Nonexistent, InitialBodyIsBeginWith5Stmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = init->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 5u);
}

TEST_F(Nonexistent, FirstStmtAssignsArr10To10) {
  // arr[10] = 10
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "arr[10]");
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "10");
}

TEST_F(Nonexistent, SecondStmtIsDisplayWithArrSize) {
  // $display(":assert: (%d == 1)", arr.size)
  // HLC emits EL0535 for "size" (treated as implicit net) but the
  // method call still appears as HierPath "arr.size" in UHDM.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(body->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 2u);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), "arr.size");
}

TEST_F(Nonexistent, ThirdStmtIsDisplayBeginMarker) {
  // $display(":re: BEGIN:ARRAY_NONEXISTENT")
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(body->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 1u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":re: BEGIN:ARRAY_NONEXISTENT");
}

TEST_F(Nonexistent, FourthStmtAssignsRFromArr9) {
  // r = arr[9] — reading a nonexistent key; lhs is RefObj, rhs is BitSelect
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(3));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "r");
  const hldb::BitSelect *const rhs = assign->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "arr[9]");
}

TEST_F(Nonexistent, FifthStmtIsDisplayEndMarker) {
  // $display(":re: END")
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(body->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 1u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":re: END");
}

// --- structural completeness -----------------------------------------------

TEST_F(Nonexistent, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
