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
//   - design has module top
//   - module has exactly 1 variable: 'arraya' (assoc ArrayTypespec, idx=IntTypespec, elem=StringTypespec)
//   - module has 1 task 'fun' with 1 IODecl 'arrayb' (input, assoc ArrayTypespec)
//   - task body: Begin with 2 stmts (arrayb[1]="d", $display 4 args)
//   - 1 Initial process; Begin with 6 stmts
//   - stmts[0-2]: arraya[0]="a", arraya[1]="b", arraya[2]="c" (blocking BitSelect)
//   - stmt[3]: $display(4 args) with arraya[0,1,2]
//   - stmt[4]: FuncCall "fun" with 1 arg RefObj "arraya"
//   - stmt[5]: $display(4 args) with arraya[0,1,2] (same format as stmt[3])
//   - top has no continuous assignments
//
// Also checked:
//   - 'arraya' variable has no initial value
//   - FuncCall arg RefObj "arraya" resolves to the variable node via getActual()
//   - IODecl 'arrayb' idx/elem typespec details (idx=IntTypespec, elem=StringTypespec)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/task.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Arguments : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "arguments.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module and variable (string arraya[int]) ----

TEST_F(Arguments, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(Arguments, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(Arguments, VariableNameIsArraya) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "arraya");
}

TEST_F(Arguments, VariableHasAssociativeArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(Arguments, AssocArrayKeyTypeIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Arguments, AssocArrayValueTypeIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

// --- task fun (string arrayb[int]) ----

TEST_F(Arguments, ModuleHasOneTaskFunc) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
}

TEST_F(Arguments, TaskIsFunNamed) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->getName(), "fun");
}

TEST_F(Arguments, TaskHasOneIODecl) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  EXPECT_EQ(task->getIODecls()->size(), 1u);
}

TEST_F(Arguments, IODeclNameIsArrayb) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  EXPECT_EQ(task->getIODecls()->at(0)->getName(), "arrayb");
}

TEST_F(Arguments, IODeclDirectionIsInput) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  EXPECT_EQ(task->getIODecls()->at(0)->getDirection(), vpiInput);
}

TEST_F(Arguments, IODeclTypespecIsAssocArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  const hldb::IODecl *const decl = task->getIODecls()->at(0);
  ASSERT_NE(decl, nullptr);
  const hldb::RefTypespec *const rt = decl->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(Arguments, IODeclKeyTypeIsInt) {
  // string arrayb[int] -- the IODecl's associative array index type is IntTypespec
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  const hldb::IODecl *const decl = task->getIODecls()->at(0);
  ASSERT_NE(decl, nullptr);
  const hldb::ArrayTypespec *const at = decl->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Arguments, IODeclValueTypeIsString) {
  // string arrayb[int] -- the IODecl's associative array element type is StringTypespec
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  const hldb::IODecl *const decl = task->getIODecls()->at(0);
  ASSERT_NE(decl, nullptr);
  const hldb::ArrayTypespec *const at = decl->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(Arguments, TaskBodyIsBeginWith2Stmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  const hldb::Begin *const blk = task->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(Arguments, TaskFirstStmtIsArraybAt1EqualD) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  const hldb::Begin *const blk = task->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::BitSelect *const bs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<hldb::RefObj>()->getName(), "arrayb");
  EXPECT_EQ(bs->getIndex<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(assign->getRhs<hldb::Constant>()->getDecompile(), "\"d\"");
}

TEST_F(Arguments, TaskSecondStmtIsDisplayWithAssertADC) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const task = any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  ASSERT_NE(task, nullptr);
  const hldb::Begin *const blk = task->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const sc = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(1));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  EXPECT_EQ(sc->getArguments()->size(), 4u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (('%s' == 'a') and ('%s' == 'd') and ('%s' == 'c'))\"");
}

// --- initial begin (6 statements) ----

TEST_F(Arguments, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(Arguments, InitialBodyIsBeginWith6Stmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 6u);
}

TEST_F(Arguments, InitialArraya0EqualsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::BitSelect *const bs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<hldb::RefObj>()->getName(), "arraya");
  EXPECT_EQ(bs->getIndex<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(assign->getRhs<hldb::Constant>()->getDecompile(), "\"a\"");
}

TEST_F(Arguments, InitialArraya1EqualsB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::BitSelect *const bs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<hldb::RefObj>()->getName(), "arraya");
  EXPECT_EQ(bs->getIndex<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(assign->getRhs<hldb::Constant>()->getDecompile(), "\"b\"");
}

TEST_F(Arguments, InitialArraya2EqualsC) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::BitSelect *const bs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getPrefix<hldb::RefObj>()->getName(), "arraya");
  EXPECT_EQ(bs->getIndex<hldb::Constant>()->getDecompile(), "2");
  EXPECT_EQ(assign->getRhs<hldb::Constant>()->getDecompile(), "\"c\"");
}

TEST_F(Arguments, InitialFourthStmtIsDisplayABC) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const sc = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(3));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  EXPECT_EQ(sc->getArguments()->size(), 4u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (('%s' == 'a') and ('%s' == 'b') and ('%s' == 'c'))\"");
}

TEST_F(Arguments, InitialFifthStmtIsTFCallWithArraya) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::TFCall *const fc = any_cast<hldb::TFCall>(blk->getStmts()->at(4));
  ASSERT_NE(fc, nullptr);
  EXPECT_EQ(fc->getName(), "fun");
  ASSERT_NE(fc->getArguments(), nullptr);
  EXPECT_EQ(fc->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(fc->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "arraya");
}

TEST_F(Arguments, TFCallArgResolvesToArrayaVariable) {
  // fun(arraya) -- the RefObj argument's getActual() must resolve to the
  // same Variable node as the top-level 'arraya' declaration.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const arraya = top->getVariables()->at(0);
  ASSERT_NE(arraya, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::TFCall *const fc = any_cast<hldb::TFCall>(blk->getStmts()->at(4));
  ASSERT_NE(fc, nullptr);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(fc->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getActual<hldb::Variable>(), arraya);
}

TEST_F(Arguments, InitialSixthStmtIsDisplayABC) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const sc = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(5));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  EXPECT_EQ(sc->getArguments()->size(), 4u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (('%s' == 'a') and ('%s' == 'b') and ('%s' == 'c'))\"");
}

// --- structural completeness ----

TEST_F(Arguments, ArrayaVariableHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getValue<hldb::Any>(), nullptr)
      << "string arraya[int] is declared without an initializer";
}

TEST_F(Arguments, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
