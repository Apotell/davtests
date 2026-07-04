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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/foreach_stmt.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NettypeResolutionFn : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.7--nettype_resolution_fn.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(NettypeResolutionFn, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// TypedefTypespec "real_net" with alias → RealTypespec
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, ModuleHasOneTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 1u);
}

TEST_F(NettypeResolutionFn, NettypeIsTypedefTypespecNamedRealNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getName(), "real_net");
}

TEST_F(NettypeResolutionFn, NettypeAliasIsRealTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getTypedefAlias();
  ASSERT_NE(alias, nullptr);
  EXPECT_NE(alias->getActual<hldb::RealTypespec>(), nullptr)
      << "nettype real real_net: base type alias is RealTypespec";
}

// ---------------------------------------------------------------------------
// TypedefTypespec has resolution function RefObj → Function "real_sum"
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, NettypeHasResolutionFunction) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const hldb::RefObj *const fnRef = td->getResolutionFunc();
  ASSERT_NE(fnRef, nullptr) << "nettype with 'with' clause stores resolution function as RefObj";
  EXPECT_EQ(fnRef->getName(), "real_sum");
}

TEST_F(NettypeResolutionFn, ResolutionFunctionRefersToFunction) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const hldb::RefObj *const fnRef = td->getResolutionFunc();
  ASSERT_NE(fnRef, nullptr);
  EXPECT_NE(fnRef->getActual<hldb::Function>(), nullptr) << "RefObj vpiActual points to the Function node real_sum";
}

// ---------------------------------------------------------------------------
// Module contains exactly one task/function: Function "real_sum"
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, ModuleHasOneTaskFunc) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
}

TEST_F(NettypeResolutionFn, TaskFuncIsFunctionNamedRealSum) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), "real_sum");
}

TEST_F(NettypeResolutionFn, FunctionIsAutomatic) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getAutomatic()) << "automatic function keyword sets vpiAutomatic=true";
}

// ---------------------------------------------------------------------------
// Function return type: RefTypespec → RealTypespec
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, FunctionReturnTypeIsReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::RefTypespec *const ret = fn->getReturn();
  ASSERT_NE(ret, nullptr);
  EXPECT_NE(ret->getActual<hldb::RealTypespec>(), nullptr) << "function real real_sum: return type is RealTypespec";
}

// ---------------------------------------------------------------------------
// Function parameter: IODecl "driver" (input, RefTypespec → ArrayTypespec)
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, FunctionHasOneIODecl) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  EXPECT_EQ(fn->getIODecls()->size(), 1u);
}

TEST_F(NettypeResolutionFn, IODeclNameIsDriver) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::IODecl *const driver = fn->getIODecls()->at(0);
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(driver->getName(), "driver");
}

TEST_F(NettypeResolutionFn, IODeclIsInput) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::IODecl *const driver = fn->getIODecls()->at(0);
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(driver->getDirection(), vpiInput) << "driver is an input port (vpiInput=1)";
}

TEST_F(NettypeResolutionFn, IODeclTypespecIsArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::IODecl *const driver = fn->getIODecls()->at(0);
  ASSERT_NE(driver, nullptr);
  const hldb::RefTypespec *const rts = driver->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::ArrayTypespec>(), nullptr)
      << "real driver[] is an ArrayTypespec (dynamic array of real)";
}

// ---------------------------------------------------------------------------
// Function body: Begin with 2 statements
//   stmt[0]: Assignment real_sum = 0.0  (blocking)
//   stmt[1]: ForeachStmt (driver[i] loop)
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFn, FunctionBodyIsBeginWith2Statements) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const blk = fn->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(NettypeResolutionFn, FirstStmtIsBlockingAssignment) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const blk = fn->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "real_sum = 0.0 is the first statement";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(NettypeResolutionFn, FirstAssignmentRhsIsZeroPointZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const blk = fn->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiRealConst);
  EXPECT_EQ(rhs->getDecompile(), "0.0");
}

TEST_F(NettypeResolutionFn, SecondStmtIsForeachStmt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const blk = fn->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::ForeachStmt *const loop = any_cast<hldb::ForeachStmt>(blk->getStmts()->at(1));
  EXPECT_NE(loop, nullptr) << "foreach(driver[i]) is the second statement";
}

// --- structural completeness -----------------------------------------------

TEST_F(NettypeResolutionFn, NoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "nettype declaration does not create a net instance in the module";
}

TEST_F(NettypeResolutionFn, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
