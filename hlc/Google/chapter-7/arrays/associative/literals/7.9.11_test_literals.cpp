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

// Tests for literals.sv (tags: 7.9.11)
//   module top ();
//     string words[int] = '{default: "hello"};
//     initial begin
//       $display(":assert: ('%s' == 'hello')", words[1]);
//       words[1] = "world";
//       $display(":assert: (('%s' == 'hello') and ('%s' == 'world'))", words[0], words[1]);
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'words' (assoc ArrayTypespec, idx=IntTypespec, elem=StringTypespec)
//   - net vpiValue = assign-pattern Operation (vpiAssignmentPatternOp=87) with 1 TaggedPattern
//   - TaggedPattern tag = RefObj "default"; pattern = Constant value "hello"
//   - 1 Initial process; Begin with 3 stmts
//   - stmt[0]: $display(2 args) — BitSelect words[1] as second arg
//   - stmt[1]: blocking Assignment words[1]="world" (BitSelect lhs, Constant rhs)
//   - stmt[2]: $display(3 args) — words[0] and words[1] BitSelects verified
//   - work@top has no continuous assignments
//
// Not checked:
//   - runtime default-value behavior (words[x] == "hello" for any unset key)
//   - format string in stmt[2] not separately verified

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
#include <uhdm/initial.h>
#include <uhdm/int_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/string_typespec.h>
#include <uhdm/sys_func_call.h>
#include <uhdm/tagged_pattern.h>

namespace SURELOG {

class Literals : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "literals.hlc"});

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

TEST_F(Literals, ModuleExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- net "words" : string[int] with default value -------------------------

TEST_F(Literals, ModuleHasOneNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Literals, NetNameIsWords) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "words");
}

TEST_F(Literals, NetHasAssociativeArrayTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::ArrayTypespec *const at = rt->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(Literals, AssocArrayKeyTypeIsInt) {
  // `int` (2-state) is the index type for string words[int]
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

TEST_F(Literals, AssocArrayValueTypeIsString) {
  // element type is StringTypespec because `string words[int]`
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<uhdm::StringTypespec>(), nullptr);
}

// --- default value: '{default: "hello"} -----------------------------------

TEST_F(Literals, NetHasAssignPatternValue) {
  // The initializer `= '{default: "hello"}` is stored as an Operation
  // with vpiAssignmentPatternOp (87) attached to the net as vpiValue.
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::Operation *const op = net->getValue<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiAssignmentPatternOp);
}

TEST_F(Literals, DefaultPatternHasOneOperand) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Operation *const op =
      top->getNets()->at(0)->getValue<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u);
}

TEST_F(Literals, DefaultPatternTagIsDefault) {
  // Tag is a RefObj with name "default", representing the `default:` key
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Operation *const op =
      top->getNets()->at(0)->getValue<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  const uhdm::TaggedPattern *const tp =
      any_cast<uhdm::TaggedPattern>(op->getOperands()->at(0));
  ASSERT_NE(tp, nullptr);
  const uhdm::RefObj *const tag = tp->getTag<uhdm::RefObj>();
  ASSERT_NE(tag, nullptr);
  EXPECT_EQ(tag->getName(), "default");
}

TEST_F(Literals, DefaultPatternValueIsHello) {
  // Pattern is a Constant with vpiValue="hello" (the literal string "hello")
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Operation *const op =
      top->getNets()->at(0)->getValue<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  const uhdm::TaggedPattern *const tp =
      any_cast<uhdm::TaggedPattern>(op->getOperands()->at(0));
  ASSERT_NE(tp, nullptr);
  const uhdm::Constant *const pattern = tp->getPattern<uhdm::Constant>();
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->getValue(), "hello");
}

// --- initial block --------------------------------------------------------

TEST_F(Literals, InitialBodyIsBeginWith3Stmts) {
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
  EXPECT_EQ(body->getStmts()->size(), 3u);
}

TEST_F(Literals, FirstStmtIsDisplayWithDefaultHello) {
  // $display(":assert: ('%s' == 'hello')", words[1])
  // words[1] reads the default value before any assignment
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const body =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))
          ->getStmt<uhdm::Begin>();
  ASSERT_NE(body, nullptr);
  const uhdm::SysFuncCall *const disp =
      any_cast<uhdm::SysFuncCall>(body->getStmts()->at(0));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 2u);
  // second arg is BitSelect words[1]
  const uhdm::BitSelect *const bs =
      any_cast<uhdm::BitSelect>(disp->getArguments()->at(1));
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getName(), "words[1]");
}

TEST_F(Literals, SecondStmtAssignsWords1ToWorld) {
  // words[1] = "world"
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const body =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))
          ->getStmt<uhdm::Begin>();
  ASSERT_NE(body, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::BitSelect *const lhs = assign->getLhs<uhdm::BitSelect>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "words[1]");
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "world");
}

TEST_F(Literals, ThirdStmtIsDisplayWithBothValues) {
  // $display(":assert: ...", words[0], words[1]) — 3 arguments
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
  EXPECT_EQ(disp->getArguments()->size(), 3u);
  const uhdm::BitSelect *const words0 =
      any_cast<uhdm::BitSelect>(disp->getArguments()->at(1));
  ASSERT_NE(words0, nullptr);
  EXPECT_EQ(words0->getName(), "words[0]");
  const uhdm::BitSelect *const words1 =
      any_cast<uhdm::BitSelect>(disp->getArguments()->at(2));
  ASSERT_NE(words1, nullptr);
  EXPECT_EQ(words1->getName(), "words[1]");
}

// --- structural completeness -----------------------------------------------

TEST_F(Literals, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "'{default: \"hello\"} is stored as net vpiValue, not a ContAssign";
}

}  // namespace SURELOG
