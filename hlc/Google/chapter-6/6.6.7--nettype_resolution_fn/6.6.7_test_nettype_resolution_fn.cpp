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

// Tests for 6.6.7--nettype_resolution_fn.sv (tags: 6.6.7)
//   module top();
//     function automatic real real_sum(input real driver[]);
//       real_sum = 0.0;
//       foreach(driver[i]) real_sum += driver[i];
//     endfunction
//     nettype real real_net with real_sum;
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has 1 TypedefTypespec "real_net" (alias→RealTypespec, resolutionFunc→RefObj→Function "real_sum")
//   - module has 1 Function "real_sum": automatic, return=RealTypespec, 1 IODecl "driver" (input, ArrayTypespec)
//   - function body: Begin with 2 stmts: blocking Assignment (real_sum=0.0, vpiRealConst), ForeachStmt
//   - work@top has no nets, no continuous assignments
//
// Not checked:
//   - foreach body internals (real_sum += driver[i] compound assignment)
//   - IODecl ArrayTypespec element type (real)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/assignment.h>
#include <uhdm/begin.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/foreach_stmt.h>
#include <uhdm/function.h>
#include <uhdm/io_decl.h>
#include <uhdm/module.h>
#include <uhdm/real_typespec.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/typedef_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class NettypeResolutionFn : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.6.7--nettype_resolution_fn.hlc"});

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

TEST_F(NettypeResolutionFn, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// TypedefTypespec "real_net" with alias → RealTypespec
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, ModuleHasOneTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 1u);
}

TEST_F(NettypeResolutionFn, NettypeIsTypedefTypespecNamedRealNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  const uhdm::TypedefTypespec *const td =
      any_cast<uhdm::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getName(), "real_net");
}

TEST_F(NettypeResolutionFn, NettypeAliasIsRealTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      any_cast<uhdm::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const uhdm::RefTypespec *const alias = td->getTypedefAlias();
  ASSERT_NE(alias, nullptr);
  EXPECT_NE(alias->getActual<uhdm::RealTypespec>(), nullptr)
      << "nettype real real_net: base type alias is RealTypespec";
}

// ---------------------------------------------------------------------------
// TypedefTypespec has resolution function RefObj → Function "real_sum"
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, NettypeHasResolutionFunction) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      any_cast<uhdm::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const uhdm::RefObj *const fnRef = td->getResolutionFunc();
  ASSERT_NE(fnRef, nullptr)
      << "nettype with 'with' clause stores resolution function as RefObj";
  EXPECT_EQ(fnRef->getName(), "real_sum");
}

TEST_F(NettypeResolutionFn, ResolutionFunctionRefersToFunction) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      any_cast<uhdm::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const uhdm::RefObj *const fnRef = td->getResolutionFunc();
  ASSERT_NE(fnRef, nullptr);
  EXPECT_NE(fnRef->getActual<uhdm::Function>(), nullptr)
      << "RefObj vpiActual points to the Function node real_sum";
}

// ---------------------------------------------------------------------------
// Module contains exactly one task/function: Function "real_sum"
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, ModuleHasOneTaskFunc) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
}

TEST_F(NettypeResolutionFn, TaskFuncIsFunctionNamedRealSum) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), "real_sum");
}

TEST_F(NettypeResolutionFn, FunctionIsAutomatic) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getAutomatic())
      << "automatic function keyword sets vpiAutomatic=true";
}

// ---------------------------------------------------------------------------
// Function return type: RefTypespec → RealTypespec
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, FunctionReturnTypeIsReal) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const uhdm::RefTypespec *const ret = fn->getReturn();
  ASSERT_NE(ret, nullptr);
  EXPECT_NE(ret->getActual<uhdm::RealTypespec>(), nullptr)
      << "function real real_sum: return type is RealTypespec";
}

// ---------------------------------------------------------------------------
// Function parameter: IODecl "driver" (input, RefTypespec → ArrayTypespec)
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, FunctionHasOneIODecl) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  EXPECT_EQ(fn->getIODecls()->size(), 1u);
}

TEST_F(NettypeResolutionFn, IODeclNameIsDriver) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const uhdm::IODecl *const driver = fn->getIODecls()->at(0);
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(driver->getName(), "driver");
}

TEST_F(NettypeResolutionFn, IODeclIsInput) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const uhdm::IODecl *const driver = fn->getIODecls()->at(0);
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(driver->getDirection(), vpiInput)
      << "driver is an input port (vpiInput=1)";
}

TEST_F(NettypeResolutionFn, IODeclTypespecIsArray) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const uhdm::IODecl *const driver = fn->getIODecls()->at(0);
  ASSERT_NE(driver, nullptr);
  const uhdm::RefTypespec *const rts = driver->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<uhdm::ArrayTypespec>(), nullptr)
      << "real driver[] is an ArrayTypespec (dynamic array of real)";
}

// ---------------------------------------------------------------------------
// Function body: Begin with 2 statements
//   stmt[0]: Assignment real_sum = 0.0  (blocking)
//   stmt[1]: ForeachStmt (driver[i] loop)
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, FunctionBodyIsBeginWith2Statements) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const uhdm::Begin *const blk = fn->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(NettypeResolutionFn, FirstStmtIsBlockingAssignment) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const uhdm::Begin *const blk = fn->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "real_sum = 0.0 is the first statement";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(NettypeResolutionFn, FirstAssignmentRhsIsZeroPointZero) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const uhdm::Begin *const blk = fn->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiRealConst);
  EXPECT_EQ(rhs->getDecompile(), "0.0");
}

TEST_F(NettypeResolutionFn, SecondStmtIsForeachStmt) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Function *const fn =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const uhdm::Begin *const blk = fn->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::ForeachStmt *const loop =
      any_cast<uhdm::ForeachStmt>(blk->getStmts()->at(1));
  EXPECT_NE(loop, nullptr) << "foreach(driver[i]) is the second statement";
}

// --- structural completeness -----------------------------------------------

TEST_F(NettypeResolutionFn, NoNets) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "nettype declaration does not create a net instance in the module";
}

TEST_F(NettypeResolutionFn, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
