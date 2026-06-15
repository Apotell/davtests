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

// Tests for assignment.sv (tags: 7.9.9 7.8)
//   module top ();
//     string words [ int ];
//     string w [ int ];
//     initial begin
//       words[0]="hello"; words[1]="happy"; words[2]="world";
//       $display("...hello...happy...world...", words[0], words[1], words[2]);
//       w = words;
//       w[1] = "sad";
//       $display("...hello...happy...world...", words[0], words[1], words[2]);
//       $display("...hello...sad...world...",   w[0],     w[1],     w[2]);
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 2 nets: 'words' and 'w' (both assoc ArrayTypespec, idx=int, elem=string)
//   - 1 Initial process; Begin with 8 stmts
//   - stmts[0-2]: words[0]="hello", words[1]="happy", words[2]="world" (blocking BitSelect)
//   - stmt[3]: $display(4 args) — format includes hello/happy/world
//   - stmt[4]: whole-array copy w=words (blocking, lhs=RefObj "w", rhs=RefObj "words")
//   - stmt[5]: w[1]="sad" (BitSelect assignment)
//   - stmt[6]: $display(4 args) — format shows words unchanged
//   - stmt[7]: $display(4 args) — format shows w with "sad"; w[0] BitSelect verified
//   - work@top has no continuous assignments
//
// Not checked:
//   - copy-by-value semantics of whole-array assignment (runtime-only)
//   - stmt[7] w[1], w[2] argument BitSelects (only w[0] checked)

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
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/string_typespec.h>
#include <uhdm/sys_func_call.h>

namespace SURELOG {

class Assignment : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "assignment.hlc"});

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

// --- module and nets -------------------------------------------------------

TEST_F(Assignment, ModuleExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(Assignment, ModuleHasTwoNets) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(Assignment, NetsAreWordsAndW) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "words");
  EXPECT_EQ(top->getNets()->at(1)->getName(), "w");
}

TEST_F(Assignment, WordsNetIsAssocStringArray) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::ArrayTypespec *const at =
      net->getTypespec<uhdm::RefTypespec>()->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
  EXPECT_NE(at->getIndexTypespec()->getActual<uhdm::IntTypespec>(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<uhdm::StringTypespec>(), nullptr);
}

TEST_F(Assignment, WNetIsAssocStringArray) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const net = top->getNets()->at(1);
  ASSERT_NE(net, nullptr);
  const uhdm::ArrayTypespec *const at =
      net->getTypespec<uhdm::RefTypespec>()->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);
  EXPECT_NE(at->getIndexTypespec()->getActual<uhdm::IntTypespec>(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<uhdm::StringTypespec>(), nullptr);
}

// --- initial process (8 statements) ---------------------------------------

TEST_F(Assignment, InitialBodyIsBeginWith8Stmts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 8u);
}

TEST_F(Assignment, Words0AssignedHello) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::BitSelect *const bs = assign->getLhs<uhdm::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<uhdm::RefObj>()->getName(), "words");
  EXPECT_EQ(bs->getIndex<uhdm::Constant>()->getDecompile(), "0");
  EXPECT_EQ(assign->getRhs<uhdm::Constant>()->getDecompile(), "\"hello\"");
}

TEST_F(Assignment, Words1AssignedHappy) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<uhdm::BitSelect>()->getIndex<uhdm::Constant>()->getDecompile(), "1");
  EXPECT_EQ(assign->getRhs<uhdm::Constant>()->getDecompile(), "\"happy\"");
}

TEST_F(Assignment, Words2AssignedWorld) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<uhdm::BitSelect>()->getIndex<uhdm::Constant>()->getDecompile(), "2");
  EXPECT_EQ(assign->getRhs<uhdm::Constant>()->getDecompile(), "\"world\"");
}

TEST_F(Assignment, FourthStmtIsDisplayWordsHHW) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::SysFuncCall *const sc =
      any_cast<uhdm::SysFuncCall>(blk->getStmts()->at(3));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  EXPECT_EQ(sc->getArguments()->size(), 4u);
  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(),
            "\":assert: (('%s' == 'hello') and ('%s' == 'happy') and ('%s' == 'world'))\"");
}

TEST_F(Assignment, FifthStmtIsWholeArrayCopyWEqualsWords) {
  // w = words  — whole-array copy: lhs is RefObj "w", rhs is RefObj "words"
  // (no BitSelect on either side)
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(4));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::RefObj *const lhs = assign->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "w");
  const uhdm::RefObj *const rhs = assign->getRhs<uhdm::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "words");
}

TEST_F(Assignment, SixthStmtIsW1EqualsSad) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(5));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::BitSelect *const bs = assign->getLhs<uhdm::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<uhdm::RefObj>()->getName(), "w");
  EXPECT_EQ(bs->getIndex<uhdm::Constant>()->getDecompile(), "1");
  EXPECT_EQ(assign->getRhs<uhdm::Constant>()->getDecompile(), "\"sad\"");
}

TEST_F(Assignment, SeventhStmtIsDisplayWordsUnchanged) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::SysFuncCall *const sc =
      any_cast<uhdm::SysFuncCall>(blk->getStmts()->at(6));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  EXPECT_EQ(sc->getArguments()->size(), 4u);
  // fmt still asserts words holds original values (copy was by value)
  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(),
            "\":assert: (('%s' == 'hello') and ('%s' == 'happy') and ('%s' == 'world'))\"");
}

TEST_F(Assignment, EighthStmtIsDisplayWWithSad) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0))->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::SysFuncCall *const sc =
      any_cast<uhdm::SysFuncCall>(blk->getStmts()->at(7));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  EXPECT_EQ(sc->getArguments()->size(), 4u);
  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(),
            "\":assert: (('%s' == 'hello') and ('%s' == 'sad') and ('%s' == 'world'))\"");
  // second arg is w[0], third is w[1], fourth is w[2]
  const uhdm::BitSelect *const w0 =
      any_cast<uhdm::BitSelect>(sc->getArguments()->at(1));
  ASSERT_NE(w0, nullptr);
  EXPECT_EQ(w0->getPrefix<uhdm::RefObj>()->getName(), "w");
  EXPECT_EQ(w0->getIndex<uhdm::Constant>()->getDecompile(), "0");
}

// --- structural completeness -----------------------------------------------

TEST_F(Assignment, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace SURELOG
