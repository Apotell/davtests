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
//   - Surelog emits EL0535 for arr.size (implicit net) but still puts HierPath in UHDM
//   - runtime behavior (arr[9] is nonexistent, returns default int value 0)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/assignment.h>
#include <uhdm/begin.h>
#include <uhdm/bit_select.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/hier_path.h>
#include <uhdm/initial.h>
#include <uhdm/int_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/sys_func_call.h>

namespace SURELOG {

class Nonexistent : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "nonexistent.hlc"});

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

// --- module ---------------------------------------------------------------

TEST_F(Nonexistent, ModuleExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- net declarations -----------------------------------------------------

TEST_F(Nonexistent, ModuleHasTwoNets) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(Nonexistent, ArrNetHasAssociativeArrayTypespec) {
  // int arr[int] -> ArrayTypespec associative(3)
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const arr = top->getNets()->at(0);
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->getName(), "arr");
  const uhdm::RefTypespec *const rt = arr->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::ArrayTypespec *const at = rt->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(Nonexistent, AssocArrayKeyTypeIsInt) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<uhdm::IntTypespec>(), nullptr);
}

TEST_F(Nonexistent, AssocArrayValueTypeIsInt) {
  // element type is IntTypespec (from `int arr[int]`)
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<uhdm::IntTypespec>(), nullptr);
}

TEST_F(Nonexistent, RNetHasIntTypespec) {
  // int r; -> Net "r" -> RefTypespec -> IntTypespec
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const r = top->getNets()->at(1);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getName(), "r");
  const uhdm::RefTypespec *const rt = r->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<uhdm::IntTypespec>(), nullptr);
}

// --- initial block --------------------------------------------------------

TEST_F(Nonexistent, InitialBodyIsBeginWith5Stmts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const body = init->getStmt<uhdm::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 5u);
}

TEST_F(Nonexistent, FirstStmtAssignsArr10To10) {
  // arr[10] = 10
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const body =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))
          ->getStmt<uhdm::Begin>();
  ASSERT_NE(body, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::BitSelect *const lhs = assign->getLhs<uhdm::BitSelect>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "arr[10]");
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "10");
}

TEST_F(Nonexistent, SecondStmtIsDisplayWithArrSize) {
  // $display(":assert: (%d == 1)", arr.size)
  // Surelog emits EL0535 for "size" (treated as implicit net) but the
  // method call still appears as HierPath "arr.size" in UHDM.
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const body =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))
          ->getStmt<uhdm::Begin>();
  ASSERT_NE(body, nullptr);
  const uhdm::SysFuncCall *const disp =
      any_cast<uhdm::SysFuncCall>(body->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 2u);
  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), "arr.size");
}

TEST_F(Nonexistent, ThirdStmtIsDisplayBeginMarker) {
  // $display(":re: BEGIN:ARRAY_NONEXISTENT")
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const body =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))
          ->getStmt<uhdm::Begin>();
  ASSERT_NE(body, nullptr);
  const uhdm::SysFuncCall *const disp =
      any_cast<uhdm::SysFuncCall>(body->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 1u);
  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":re: BEGIN:ARRAY_NONEXISTENT");
}

TEST_F(Nonexistent, FourthStmtAssignsRFromArr9) {
  // r = arr[9] — reading a nonexistent key; lhs is RefObj, rhs is BitSelect
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const body =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))
          ->getStmt<uhdm::Begin>();
  ASSERT_NE(body, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(body->getStmts()->at(3));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::RefObj *const lhs = assign->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "r");
  const uhdm::BitSelect *const rhs = assign->getRhs<uhdm::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "arr[9]");
}

TEST_F(Nonexistent, FifthStmtIsDisplayEndMarker) {
  // $display(":re: END")
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const body =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))
          ->getStmt<uhdm::Begin>();
  ASSERT_NE(body, nullptr);
  const uhdm::SysFuncCall *const disp =
      any_cast<uhdm::SysFuncCall>(body->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 1u);
  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":re: END");
}

// --- structural completeness -----------------------------------------------

TEST_F(Nonexistent, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace SURELOG
