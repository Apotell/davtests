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
// What to check and why (IEEE 1800-2023 6.6.7 "User-defined nettypes",
// p.97-98, checked before any test code was written):
//   "A user-defined resolution function for a net of a user-defined
//   nettype with a data type T shall be a function with a return type
//   of T and a single input argument whose type is a dynamic array of
//   elements of type T. A resolution function shall be automatic ...".
//   "real_sum" matches exactly: automatic, returns real (T=real), single
//   input "real driver[]" (dynamic array of real). "nettype real
//   real_net with real_sum;" is the "second form" the spec describes:
//   an nettype with an explicit resolution function. This file has no
//   :should_fail_because: tag and is fully legal.
//
//   As with the sibling file 6.6.7--nettype.sv, hldb represents the
//   nettype declaration as a TypedefTypespec (no dedicated nettype
//   class exists), with getResolutionFunc() populated this time (a
//   RefObj resolving to the Function "real_sum") -- this is not a
//   misclassification, it is the same reused-typedef representation the
//   spec text itself invites ("similar to a typedef").
//
// What is checked:
//   - module top has exactly 1 typespec: TypedefTypespec "real_net",
//     alias -> RealTypespec, resolutionFunc -> RefObj -> Function
//     "real_sum"
//   - module has exactly 1 task/function: Function "real_sum" --
//     automatic, return type RealTypespec, exactly 1 IODecl "driver"
//     (input, RefTypespec -> ArrayTypespec of RealTypespec elements)
//   - function body: Begin with 2 statements: blocking Assignment
//     (real_sum = 0.0, vpiRealConst) then a ForeachStmt
//   - foreach body: real_sum += driver[i] is a blocking Assignment whose
//     RHS is an add Operation over RefObj "real_sum" and BitSelect
//     "driver[i]"
//   - top has no Nets (nettype declares a type, not an instance) and no
//     ContAssigns
//   - compiler reports zero errors (this file is fully legal per 6.6.7)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/foreach_stmt.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NettypeResolutionFnTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.7--nettype_resolution_fn.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(NettypeResolutionFnTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// TypedefTypespec "real_net" with alias -> RealTypespec
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFnTest, ModuleHasOneTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 1u);
}

TEST_F(NettypeResolutionFnTest, NettypeIsTypedefTypespecNamedRealNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getName(), "real_net");
}

TEST_F(NettypeResolutionFnTest, NettypeAliasIsRealTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getTypedefAlias();
  ASSERT_NE(alias, nullptr);
  EXPECT_NE(alias->getActual<hldb::RealTypespec>(), nullptr)
      << "nettype real real_net: base type alias is RealTypespec";
}

// ---------------------------------------------------------------------------
// TypedefTypespec has resolution function RefObj -> Function "real_sum"
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFnTest, NettypeHasResolutionFunction) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const hldb::RefObj *const fnRef = td->getResolutionFunc();
  ASSERT_NE(fnRef, nullptr) << "nettype with 'with' clause stores resolution function as RefObj";
  EXPECT_EQ(fnRef->getName(), "real_sum");
}

TEST_F(NettypeResolutionFnTest, ResolutionFunctionRefersToFunction) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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
TEST_F(NettypeResolutionFnTest, ModuleHasOneTaskFunc) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
}

TEST_F(NettypeResolutionFnTest, TaskFuncIsFunctionNamedRealSum) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), "real_sum");
}

TEST_F(NettypeResolutionFnTest, FunctionIsAutomatic) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getAutomatic()) << "automatic function keyword sets vpiAutomatic=true";
}

// ---------------------------------------------------------------------------
// Function return type: RefTypespec -> RealTypespec
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFnTest, FunctionReturnTypeIsReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::RefTypespec *const ret = fn->getReturn();
  ASSERT_NE(ret, nullptr);
  EXPECT_NE(ret->getActual<hldb::RealTypespec>(), nullptr) << "function real real_sum: return type is RealTypespec";
}

// ---------------------------------------------------------------------------
// Function parameter: IODecl "driver" (input, RefTypespec -> ArrayTypespec)
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFnTest, FunctionHasOneIODecl) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  EXPECT_EQ(fn->getIODecls()->size(), 1u);
}

TEST_F(NettypeResolutionFnTest, IODeclNameIsDriver) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::IODecl *const driver = fn->getIODecls()->at(0);
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(driver->getName(), "driver");
}

TEST_F(NettypeResolutionFnTest, IODeclIsInput) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::IODecl *const driver = fn->getIODecls()->at(0);
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(driver->getDirection(), vpiInput) << "driver is an input port (vpiInput=1)";
}

TEST_F(NettypeResolutionFnTest, IODeclTypespecIsArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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
TEST_F(NettypeResolutionFnTest, FunctionBodyIsBeginWith2Statements) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const blk = fn->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(NettypeResolutionFnTest, FirstStmtIsBlockingAssignment) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const blk = fn->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "real_sum = 0.0 is the first statement";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(NettypeResolutionFnTest, FirstAssignmentRhsIsZeroPointZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(NettypeResolutionFnTest, SecondStmtIsForeachStmt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const blk = fn->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::ForeachStmt *const loop = any_cast<hldb::ForeachStmt>(blk->getStmts()->at(1));
  EXPECT_NE(loop, nullptr) << "foreach(driver[i]) is the second statement";
}

// --- structural completeness -----------------------------------------------

TEST_F(NettypeResolutionFnTest, NoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "nettype declaration does not create a net instance in the module";
}

TEST_F(NettypeResolutionFnTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

// ---------------------------------------------------------------------------
// foreach body: real_sum += driver[i] -> blocking Assignment, rhs = add(real_sum, driver[i])
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFnTest, ForeachBodyIsCompoundAddAssignment) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const blk = fn->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::ForeachStmt *const loop = any_cast<hldb::ForeachStmt>(blk->getStmts()->at(1));
  ASSERT_NE(loop, nullptr);
  const hldb::Assignment *const assign = loop->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "foreach body should be a single Assignment";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "real_sum");
}

TEST_F(NettypeResolutionFnTest, ForeachAssignmentRhsIsAddOfRealSumAndDriverIndex) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const blk = fn->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::ForeachStmt *const loop = any_cast<hldb::ForeachStmt>(blk->getStmts()->at(1));
  ASSERT_NE(loop, nullptr);
  const hldb::Assignment *const assign = loop->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const addOp = assign->getRhs<hldb::Operation>();
  ASSERT_NE(addOp, nullptr) << "real_sum += driver[i] rhs should be an add Operation";
  EXPECT_EQ(addOp->getOpType(), vpiAddOp);
  ASSERT_NE(addOp->getOperands(), nullptr);
  ASSERT_EQ(addOp->getOperands()->size(), 2u);
  const hldb::RefObj *const lhsOp = any_cast<hldb::RefObj>(addOp->getOperands()->at(0));
  ASSERT_NE(lhsOp, nullptr);
  EXPECT_EQ(lhsOp->getName(), "real_sum");
  const hldb::BitSelect *const rhsOp = any_cast<hldb::BitSelect>(addOp->getOperands()->at(1));
  ASSERT_NE(rhsOp, nullptr);
  EXPECT_EQ(rhsOp->getName(), "driver[i]");
}

// ---------------------------------------------------------------------------
// IODecl "driver" ArrayTypespec element type is RealTypespec
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFnTest, IODeclArrayElementTypeIsReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fn, nullptr);
  const hldb::IODecl *const driver = fn->getIODecls()->at(0);
  ASSERT_NE(driver, nullptr);
  const hldb::ArrayTypespec *const arrTs = driver->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(arrTs, nullptr);
  const hldb::RefTypespec *const elemRts = arrTs->getElemTypespec();
  ASSERT_NE(elemRts, nullptr) << "dynamic array of real should carry an element typespec";
  EXPECT_NE(elemRts->getActual<hldb::RealTypespec>(), nullptr) << "real driver[] element type should be RealTypespec";
}

// ---------------------------------------------------------------------------
// This file is fully legal per IEEE 1800-2023 6.6.7 -- no errors expected
// ---------------------------------------------------------------------------
TEST_F(NettypeResolutionFnTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
