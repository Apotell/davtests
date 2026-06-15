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

// Tests for arguments.sv (tags: 7.9.10 7.8)
//   module top ();
//     string arraya[int];
//     task fun (string arrayb[int]);
//       arrayb[1] = "d";
//       $display("...a...d...c...", arrayb[0], arrayb[1], arrayb[2]);
//     endtask
//     initial begin
//       arraya[0]="a"; arraya[1]="b"; arraya[2]="c";
//       $display("...a...b...c...", arraya[0], arraya[1], arraya[2]);
//       fun(arraya);
//       $display("...a...b...c...", arraya[0], arraya[1], arraya[2]);
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'arraya' (assoc ArrayTypespec, idx=IntTypespec, elem=StringTypespec)
//   - module has 1 task 'fun' with 1 IODecl 'arrayb' (input, assoc ArrayTypespec)
//   - task body: Begin with 2 stmts (arrayb[1]="d", $display 4 args)
//   - 1 Initial process; Begin with 6 stmts
//   - stmts[0-2]: arraya[0]="a", arraya[1]="b", arraya[2]="c" (blocking BitSelect)
//   - stmt[3]: $display(4 args) with arraya[0,1,2]
//   - stmt[4]: FuncCall "fun" with 1 arg RefObj "arraya"
//   - stmt[5]: $display(4 args) with arraya[0,1,2] (same format as stmt[3])
//   - work@top has no continuous assignments
//
// Not checked:
//   - 'arraya' net has no initial value
//   - FuncCall arg RefObj "arraya" resolves to the net node
//   - IODecl idx/elem typespec details (only top-level assoc type checked)

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
#include <uhdm/func_call.h>
#include <uhdm/initial.h>
#include <uhdm/int_typespec.h>
#include <uhdm/io_decl.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/string_typespec.h>
#include <uhdm/sys_func_call.h>
#include <uhdm/task.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class Arguments : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "arguments.hlc"});

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

// --- module and net (string arraya[int]) ----------------------------------

TEST_F(Arguments, ModuleExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(Arguments, ModuleHasOneNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Arguments, NetNameIsArraya) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "arraya");
}

TEST_F(Arguments, NetHasAssociativeArrayTypespec) {
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

TEST_F(Arguments, AssocArrayKeyTypeIsInt) {
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

TEST_F(Arguments, AssocArrayValueTypeIsString) {
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

// --- task fun (string arrayb[int]) ----------------------------------------

TEST_F(Arguments, ModuleHasOneTaskFunc) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
}

TEST_F(Arguments, TaskIsFunNamed) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  const uhdm::Task *const task =
      any_cast<uhdm::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->getName(), "fun");
}

TEST_F(Arguments, TaskHasOneIODecl) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Task *const task =
      any_cast<uhdm::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  EXPECT_EQ(task->getIODecls()->size(), 1u);
}

TEST_F(Arguments, IODeclNameIsArrayb) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Task *const task =
      any_cast<uhdm::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  EXPECT_EQ(task->getIODecls()->at(0)->getName(), "arrayb");
}

TEST_F(Arguments, IODeclDirectionIsInput) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Task *const task =
      any_cast<uhdm::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  EXPECT_EQ(task->getIODecls()->at(0)->getDirection(), vpiInput);
}

TEST_F(Arguments, IODeclTypespecIsAssocArray) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Task *const task =
      any_cast<uhdm::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  const uhdm::IODecl *const decl = task->getIODecls()->at(0);
  ASSERT_NE(decl, nullptr);
  const uhdm::RefTypespec *const rt = decl->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::ArrayTypespec *const at = rt->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(Arguments, TaskBodyIsBeginWith2Stmts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Task *const task =
      any_cast<uhdm::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  const uhdm::Begin *const blk = task->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(Arguments, TaskFirstStmtIsArraybAt1EqualD) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Task *const task =
      any_cast<uhdm::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  const uhdm::Begin *const blk = task->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::BitSelect *const bs = assign->getLhs<uhdm::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<uhdm::RefObj>()->getName(), "arrayb");
  EXPECT_EQ(bs->getIndex<uhdm::Constant>()->getDecompile(), "1");
  EXPECT_EQ(assign->getRhs<uhdm::Constant>()->getDecompile(), "\"d\"");
}

TEST_F(Arguments, TaskSecondStmtIsDisplayWithAssertADC) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Task *const task =
      any_cast<uhdm::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  const uhdm::Begin *const blk = task->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::SysFuncCall *const sc =
      any_cast<uhdm::SysFuncCall>(blk->getStmts()->at(1));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  EXPECT_EQ(sc->getArguments()->size(), 4u);
  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(),
            "\":assert: (('%s' == 'a') and ('%s' == 'd') and ('%s' == 'c'))\"");
}

// --- initial begin (6 statements) ----------------------------------------

TEST_F(Arguments, ModuleHasOneInitialProcess) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<uhdm::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(Arguments, InitialBodyIsBeginWith6Stmts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 6u);
}

TEST_F(Arguments, InitialArraya0EqualsA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::BitSelect *const bs = assign->getLhs<uhdm::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<uhdm::RefObj>()->getName(), "arraya");
  EXPECT_EQ(bs->getIndex<uhdm::Constant>()->getDecompile(), "0");
  EXPECT_EQ(assign->getRhs<uhdm::Constant>()->getDecompile(), "\"a\"");
}

TEST_F(Arguments, InitialArraya1EqualsB) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::BitSelect *const bs = assign->getLhs<uhdm::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<uhdm::RefObj>()->getName(), "arraya");
  EXPECT_EQ(bs->getIndex<uhdm::Constant>()->getDecompile(), "1");
  EXPECT_EQ(assign->getRhs<uhdm::Constant>()->getDecompile(), "\"b\"");
}

TEST_F(Arguments, InitialArraya2EqualsC) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::BitSelect *const bs = assign->getLhs<uhdm::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<uhdm::RefObj>()->getName(), "arraya");
  EXPECT_EQ(bs->getIndex<uhdm::Constant>()->getDecompile(), "2");
  EXPECT_EQ(assign->getRhs<uhdm::Constant>()->getDecompile(), "\"c\"");
}

TEST_F(Arguments, InitialFourthStmtIsDisplayABC) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
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
            "\":assert: (('%s' == 'a') and ('%s' == 'b') and ('%s' == 'c'))\"");
}

TEST_F(Arguments, InitialFifthStmtIsFunCallWithArraya) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::FuncCall *const fc =
      any_cast<uhdm::FuncCall>(blk->getStmts()->at(4));
  ASSERT_NE(fc, nullptr);
  EXPECT_EQ(fc->getName(), "fun");
  ASSERT_NE(fc->getArguments(), nullptr);
  EXPECT_EQ(fc->getArguments()->size(), 1u);
  const uhdm::RefObj *const arg =
      any_cast<uhdm::RefObj>(fc->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "arraya");
}

TEST_F(Arguments, InitialSixthStmtIsDisplayABC) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::SysFuncCall *const sc =
      any_cast<uhdm::SysFuncCall>(blk->getStmts()->at(5));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  EXPECT_EQ(sc->getArguments()->size(), 4u);
  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(),
            "\":assert: (('%s' == 'a') and ('%s' == 'b') and ('%s' == 'c'))\"");
}

// --- structural completeness -----------------------------------------------

TEST_F(Arguments, ArrayaNetHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getValue<uhdm::Any>(), nullptr)
      << "string arraya[int] is declared without an initializer";
}

TEST_F(Arguments, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace SURELOG
