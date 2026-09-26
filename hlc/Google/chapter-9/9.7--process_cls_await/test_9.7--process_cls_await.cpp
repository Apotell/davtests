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

// ============================================================================
// SystemVerilog source under test:
// tests/Google/chapter-9/9.7--process_cls_await.sv
// ----------------------------------------------------------------------------
// // Copyright (C) 2019-2021  The SymbiFlow Authors.
// //
// // Use of this source code is governed by a ISC-style
// // license that can be found in the LICENSE file or at
// // https://opensource.org/licenses/ISC
// //
// // SPDX-License-Identifier: ISC
//
// /*
// :name: process_cls_await
// :description: process class await method
// :tags: 9.7
// */
// module process_tb ();
// 	task automatic test (int N);
// 		process job[] = new [N];
//
// 		foreach(job[i])
// 			fork
// 				automatic int k = i;
// 				begin
// 					job[k] = process::self();
// 					$display("process %d", k);
// 				end
// 			join_none
//
// 		foreach(job[i])
// 			wait(job[i] != null);
//
// 		job[1].await();
// 	endtask
//
// 	initial begin
// 		test(8);
// 	end
//
// endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs under test (Sec 9.7, "Process class"):
//   "The process class provides ... methods that allow processes to control
//   themselves or other processes ... process::self() is a static method
//   that returns a handle to the currently executing process." Each branch
//   of the "fork ... join_none" (9.3.2) captures its own handle via
//   "job[k] = process::self();", using a per-branch "automatic int k = i;"
//   (a par_block's own block_item_declaration, shared by all branches, but
//   written once per foreach iteration -- 12.7.3) to give each spawned
//   process a stable index despite "i" itself being reused by the
//   surrounding foreach-loop.
//   "task.await() ... blocks the calling process until the process
//   referred to by the handle completes" -- "job[1].await();" blocks
//   process_tb.test until the second forked process (index 1) finishes.
//   The second "foreach(job[i]) wait(job[i] != null);" (9.4.4, "Level
//   sensitive event control") is unrelated to the process class itself: it
//   is the ordinary idiom used to wait until every "job[i]" handle has been
//   populated (i.e. every forked branch has reached its first statement)
//   before any handle is used, since "join_none" (9.3.2) does not wait for
//   its branches to start.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - the built-in "process" class exists as a real ClassDefn (findable via
//     Design::getAllClasses()), with a method set that includes "self" and
//     "await"
//   - design has module "process_tb" with one task "test", declared
//     automatic, with one input IODecl "N" whose typespec resolves to
//     IntTypespec
//   - "test"'s body is a Begin (from "task automatic test (...); ... N
//     statements ... endtask" with more than one item at the top level)
//     whose own scope declares exactly one Variable: "job"
//   - "job"'s typespec resolves to ArrayTypespec with getArrayType() ==
//     vpiDynamicArray (a "process job[] = new [N];" dynamic array, not a
//     fixed-size one), whose element typespec resolves to a ClassTypespec
//     whose getClassDefn() is the built-in "process" ClassDefn
//   - "job"'s initializer ("= new [N]") is an ArrayExpr with exactly one
//     expression, a RefObj "N" resolving to the task's own IODecl "N"
//   - the Begin's own statement list has exactly 4 entries, in source
//     order: the "job" declaration itself, the first ForeachStmt (the
//     fork/join_none loop), the second ForeachStmt (the wait loop), and
//     the "job[1].await();" call
//   - the first ForeachStmt: own scope declares exactly one Variable "i"
//     (12.7.3's implicit per-loop scope); its loop variable resolves "job"
//     to the same Variable; its controlled statement is a ForkStmt with
//     getJoinType() == vpiJoinNone
//   - the ForkStmt: own scope declares exactly one Variable "k", flagged
//     getAutomatic(), whose typespec resolves to IntTypespec and whose
//     initializer is a RefObj "i" resolving to the foreach loop's own
//     Variable "i"; its own statement list has exactly 2 entries -- the "k"
//     declaration itself, then a Begin (from the explicit "begin ... end"
//     wrapping the fork branch's two statements)
//   - that Begin has exactly 2 statements: a blocking Assignment whose lhs
//     is BitSelect "job[k]" (prefix resolves to Variable "job", index
//     resolves to Variable "k") and whose rhs is a RefObj named
//     "process::self()" with exactly 2 path elements (getPathElems()) -- a
//     RefTypespec resolving to a ClassTypespec for the "process" ClassDefn
//     (the "process::" scope prefix), then a MethodFuncCall named "self"
//     whose getTaskFunc<Function>() resolves to "process"'s own "self"
//     method (a function, since "self()" returns the process handle) --
//     then a SysTaskCall "$display" with 2 arguments: the string literal
//     "process %d" and a RefObj "k" resolving to Variable "k"
//   - the second ForeachStmt: own scope declares exactly one Variable "i"
//     (its own, distinct per-loop scope, same as the first ForeachStmt);
//     its loop variable resolves "job" to the same Variable; its
//     controlled statement is a WaitStmt whose condition is an Operation
//     with getOpType() == vpiNeqOp and 2 operands: BitSelect "job[i]"
//     (prefix/index resolving to Variable "job"/Variable "i") and a
//     Constant with getConstType() == vpiNullConst
//   - "job[1].await();" is a RefObj directly bound as the Begin's 4th
//     statement, named "job[1].await()" with exactly 2 path elements -- a
//     BitSelect "job[1]" (prefix resolves to Variable "job", index is
//     Constant "1"), then a MethodTaskCall named "await" whose
//     getTaskFunc<Task>() resolves to "process"'s own "await" method (a
//     task, since "await()" returns nothing)
//   - module "process_tb" has exactly one process, an Initial, whose
//     statement is a Begin wrapping exactly one TaskCall resolving to
//     "test" with one Constant argument "8"
//   - COMP_FAILED_TO_BIND is never raised: both "self" and "await" bind
//     cleanly to the built-in "process" class's own methods
//
// NOT CHECKED (out of scope regardless of pass/fail; every assertion above
// states only what IEEE 1800-2023 requires -- none of it is based on
// reading a .log file or any other tool-output dump):
//   - Runtime effects (that "job[1].await()" actually blocks until the
//     second forked process completes, that "process::self()" actually
//     returns a distinct handle per branch, or that the fork branches
//     actually run in any particular order) cannot be observed: HLC is a
//     compiler/elaborator with no simulation capability, so no execution
//     ever happens for this test to check.
// ============================================================================

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_expr.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/foreach_stmt.h>
#include <hldb/fork_stmt.h>
#include <hldb/function.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/method_task_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_task_call.h>
#include <hldb/task.h>
#include <hldb/task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>
#include <hldb/wait_stmt.h>

namespace hlc {

class ProcessClsAwaitTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.7--process_cls_await.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // clang-format off
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("process_tb", m_design->getAllModules());
  }

  static const hldb::Task *getTestTask() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getTaskFuncs() == nullptr)
      return nullptr;
    return hldb::findByName<hldb::Task>("test", mod->getTaskFuncs());
  }

  static const hldb::Begin *getTaskBody() {
    const hldb::Task *const task = getTestTask();
    if (task == nullptr)
      return nullptr;
    return task->getStmt<hldb::Begin>();
  }

  static const hldb::Variable *getJobVariable() {
    const hldb::Begin *const body = getTaskBody();
    if (body == nullptr)
      return nullptr;
    return hldb::findByName<hldb::Variable>("job", body->getVariables());
  }

  static const hldb::ForeachStmt *getForkForeach() {
    const hldb::Begin *const body = getTaskBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2)
      return nullptr;
    return any_cast<hldb::ForeachStmt>(body->getStmts()->at(1));
  }

  static const hldb::ForeachStmt *getWaitForeach() {
    const hldb::Begin *const body = getTaskBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 3)
      return nullptr;
    return any_cast<hldb::ForeachStmt>(body->getStmts()->at(2));
  }

  static const hldb::RefObj *getAwaitCall() {
    const hldb::Begin *const body = getTaskBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 4)
      return nullptr;
    return any_cast<hldb::RefObj>(body->getStmts()->at(3));
  }

  static const hldb::ForkStmt *getForkStmt() {
    const hldb::ForeachStmt *const fe = getForkForeach();
    if (fe == nullptr)
      return nullptr;
    return fe->getStmt<hldb::ForkStmt>();
  }

  static const hldb::Variable *getKVariable() {
    const hldb::ForkStmt *const fork = getForkStmt();
    if (fork == nullptr)
      return nullptr;
    return hldb::findByName<hldb::Variable>("k", fork->getVariables());
  }

  static const hldb::Begin *getForkBranchBegin() {
    const hldb::ForkStmt *const fork = getForkStmt();
    if (fork == nullptr || fork->getStmts() == nullptr || fork->getStmts()->size() < 2)
      return nullptr;
    return any_cast<hldb::Begin>(fork->getStmts()->at(1));
  }

  static const hldb::Assignment *getSelfAssignment() {
    const hldb::Begin *const branch = getForkBranchBegin();
    if (branch == nullptr || branch->getStmts() == nullptr || branch->getStmts()->empty())
      return nullptr;
    return any_cast<hldb::Assignment>(branch->getStmts()->at(0));
  }

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const branch = getForkBranchBegin();
    if (branch == nullptr || branch->getStmts() == nullptr || branch->getStmts()->size() < 2)
      return nullptr;
    return any_cast<hldb::SysTaskCall>(branch->getStmts()->at(1));
  }

  static const hldb::WaitStmt *getWaitStmt() {
    const hldb::ForeachStmt *const fe = getWaitForeach();
    if (fe == nullptr)
      return nullptr;
    return fe->getStmt<hldb::WaitStmt>();
  }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty())
      return nullptr;
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::TaskCall *getTestCall() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr)
      return nullptr;
    const hldb::Begin *const body = init->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty())
      return nullptr;
    return any_cast<hldb::TaskCall>(body->getStmts()->at(0));
  }
  // clang-format on
};

// --- module / task "test" ----------------------------------------------------

TEST_F(ProcessClsAwaitTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(ProcessClsAwaitTest, ModuleHasOneTaskTest) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getTaskFuncs(), nullptr);
  EXPECT_EQ(mod->getTaskFuncs()->size(), 1u);
  EXPECT_NE(getTestTask(), nullptr) << "'task automatic test (int N);' not found";
}

TEST_F(ProcessClsAwaitTest, TaskTestIsAutomaticWithOneIntInput) {
  const hldb::Task *const task = getTestTask();
  ASSERT_NE(task, nullptr);
  EXPECT_TRUE(task->getAutomatic()) << "'task automatic test' must be flagged automatic";

  ASSERT_NE(task->getIODecls(), nullptr);
  ASSERT_EQ(task->getIODecls()->size(), 1u);
  const hldb::IODecl *const n = task->getIODecls()->front();
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getName(), "N");

  ASSERT_NE(n->getTypespec(), nullptr);
  EXPECT_NE(n->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'int N' should resolve to IntTypespec";
}

TEST_F(ProcessClsAwaitTest, TaskBodyIsBeginWithFourStmts) {
  const hldb::Begin *const body = getTaskBody();
  ASSERT_NE(body, nullptr) << "task 'test' with more than one top-level item must produce a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 4u)
      << "'job' declaration, the two foreach-loops, and 'job[1].await();' are exactly 4 statements";
}

// --- "process job[] = new [N];" ----------------------------------------------

TEST_F(ProcessClsAwaitTest, TaskScopeHasExactlyOneVariableJob) {
  const hldb::Begin *const body = getTaskBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getVariables(), nullptr);
  ASSERT_EQ(body->getVariables()->size(), 1u);
  EXPECT_NE(getJobVariable(), nullptr) << "Variable 'job' not found";
}

TEST_F(ProcessClsAwaitTest, ProcessClassExists) {
  EXPECT_NE(hldb::findByName<hldb::ClassDefn>("process", m_design->getAllClasses()), nullptr)
      << "the built-in 'process' class should be modeled as a real ClassDefn";
}

TEST_F(ProcessClsAwaitTest, JobTypespecIsDynamicArrayOfProcess) {
  const hldb::Variable *const job = getJobVariable();
  ASSERT_NE(job, nullptr);
  ASSERT_NE(job->getTypespec(), nullptr);

  const hldb::ArrayTypespec *const at = job->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'process job[] = ...' should resolve to ArrayTypespec";
  EXPECT_EQ(at->getArrayType(), vpiDynamicArray) << "'job[]' with no fixed size is a dynamic array";

  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::ClassTypespec *const elem = at->getElemTypespec()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(elem, nullptr) << "'process' should resolve to ClassTypespec";
  ASSERT_NE(elem->getClassDefn(), nullptr);
  EXPECT_EQ(elem->getClassDefn()->getName(), "process");
}

TEST_F(ProcessClsAwaitTest, JobInitializerIsArrayExprOfN) {
  const hldb::Variable *const job = getJobVariable();
  ASSERT_NE(job, nullptr);
  const hldb::ArrayExpr *const value = job->getValue<hldb::ArrayExpr>();
  ASSERT_NE(value, nullptr) << "'= new [N]' should be an ArrayExpr";

  ASSERT_NE(value->getExprs(), nullptr);
  ASSERT_EQ(value->getExprs()->size(), 1u);
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(value->getExprs()->at(0));
  ASSERT_NE(n, nullptr) << "'new [N]' size argument should be a RefObj";
  EXPECT_EQ(n->getName(), "N");

  const hldb::Task *const task = getTestTask();
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  ASSERT_FALSE(task->getIODecls()->empty());
  EXPECT_EQ(n->getActual<hldb::IODecl>(), task->getIODecls()->front()) << "'N' should resolve to the task's own IODecl";
}

// --- foreach(job[i]) fork ... join_none --------------------------------------

TEST_F(ProcessClsAwaitTest, FirstStmtIsForkForeach) {
  EXPECT_NE(getForkForeach(), nullptr) << "the first statement after 'job' should be the fork/join_none foreach-loop";
}

TEST_F(ProcessClsAwaitTest, ForkForeachScopeHasExactlyOneVariableI) {
  const hldb::ForeachStmt *const fe = getForkForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getVariables(), nullptr);
  ASSERT_EQ(fe->getVariables()->size(), 1u) << "12.7.3: 'i' should live in this ForeachStmt's own local scope";
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", fe->getVariables()), nullptr) << "Variable 'i' not found";
}

TEST_F(ProcessClsAwaitTest, ForkForeachVariableIsJobResolvingToTheArrayVariable) {
  const hldb::ForeachStmt *const fe = getForkForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::RefObj *const arr = fe->getVariable<hldb::RefObj>();
  ASSERT_NE(arr, nullptr) << "ForeachStmt.getVariable() is not a RefObj";
  EXPECT_EQ(arr->getName(), "job");
  EXPECT_EQ(arr->getActual<hldb::Variable>(), getJobVariable());
}

TEST_F(ProcessClsAwaitTest, ForkForeachBodyIsForkStmtJoinNone) {
  const hldb::ForkStmt *const fork = getForkStmt();
  ASSERT_NE(fork, nullptr) << "the foreach-loop body should be a ForkStmt";
  EXPECT_EQ(fork->getJoinType(), vpiJoinNone) << "'join_none' must have JoinType vpiJoinNone";
}

// --- fork automatic int k = i; begin ... end ---------------------------------

TEST_F(ProcessClsAwaitTest, ForkScopeHasExactlyOneAutomaticVariableK) {
  const hldb::ForkStmt *const fork = getForkStmt();
  ASSERT_NE(fork, nullptr);
  ASSERT_NE(fork->getVariables(), nullptr);
  ASSERT_EQ(fork->getVariables()->size(), 1u)
      << "'automatic int k = i;' is a par_block declaration shared by all fork branches";

  const hldb::Variable *const k = getKVariable();
  ASSERT_NE(k, nullptr) << "Variable 'k' not found";
  EXPECT_TRUE(k->getAutomatic()) << "'automatic int k' must be flagged automatic";

  ASSERT_NE(k->getTypespec(), nullptr);
  EXPECT_NE(k->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'int k' should resolve to IntTypespec";
}

TEST_F(ProcessClsAwaitTest, KInitializerIsRefObjResolvingToForeachVariableI) {
  const hldb::Variable *const k = getKVariable();
  ASSERT_NE(k, nullptr);
  const hldb::RefObj *const init = k->getValue<hldb::RefObj>();
  ASSERT_NE(init, nullptr) << "'= i' should be a RefObj";
  EXPECT_EQ(init->getName(), "i");

  const hldb::ForeachStmt *const fe = getForkForeach();
  ASSERT_NE(fe, nullptr);
  EXPECT_EQ(init->getActual<hldb::Variable>(), hldb::findByName<hldb::Variable>("i", fe->getVariables()))
      << "'i' in 'automatic int k = i;' should resolve to the enclosing foreach-loop's own Variable 'i'";
}

TEST_F(ProcessClsAwaitTest, ForkHasTwoStmtsKDeclThenBegin) {
  const hldb::ForkStmt *const fork = getForkStmt();
  ASSERT_NE(fork, nullptr);
  ASSERT_NE(fork->getStmts(), nullptr);
  EXPECT_EQ(fork->getStmts()->size(), 2u) << "the 'k' declaration and the single 'begin ... end' branch";
  EXPECT_NE(getForkBranchBegin(), nullptr) << "the fork's single branch should be a Begin";
}

// --- begin job[k] = process::self(); $display("process %d", k); end --------

TEST_F(ProcessClsAwaitTest, ForkBranchBeginHasTwoStmts) {
  const hldb::Begin *const branch = getForkBranchBegin();
  ASSERT_NE(branch, nullptr);
  ASSERT_NE(branch->getStmts(), nullptr);
  EXPECT_EQ(branch->getStmts()->size(), 2u) << "the assignment and the '$display' call are exactly 2 statements";
}

TEST_F(ProcessClsAwaitTest, SelfAssignmentLhsIsJobOfK) {
  const hldb::Assignment *const assign = getSelfAssignment();
  ASSERT_NE(assign, nullptr) << "'job[k] = process::self();' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'job[k] = ...' uses the blocking assignment operator '='";

  const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr) << "'job[k]' should be a BitSelect";
  EXPECT_EQ(lhs->getName(), "job[k]");

  const hldb::RefObj *const prefix = lhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getActual<hldb::Variable>(), getJobVariable());

  const hldb::RefObj *const index = lhs->getIndex<hldb::RefObj>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getActual<hldb::Variable>(), getKVariable());
}

// 'process::self()' resolves to a RefObj carrying a 2-element path (from the
// 'process::' class-scope prefix): a RefTypespec (resolving to a
// ClassTypespec for the built-in 'process' ClassDefn) and a MethodFuncCall
// named 'self' -- a function, not a task, since 'self()' returns the
// process handle -- resolving via getTaskFunc<Function>() to 'process'
// class's own 'self' method.
TEST_F(ProcessClsAwaitTest, SelfAssignmentRhsIsProcessSelfMethodCall) {
  const hldb::Assignment *const assign = getSelfAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'process::self()' should be a RefObj";
  EXPECT_EQ(rhs->getName(), "process::self()");

  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u) << "'process' scope prefix, then the 'self' call";

  const hldb::RefTypespec *const prefix = any_cast<hldb::RefTypespec>(rhs->getPathElems()->at(0));
  ASSERT_NE(prefix, nullptr) << "the 'process' scope-resolution prefix should be a RefTypespec";
  const hldb::ClassTypespec *const process = prefix->getActual<hldb::ClassTypespec>();
  ASSERT_NE(process, nullptr) << "'process' should resolve to ClassTypespec";
  ASSERT_NE(process->getClassDefn(), nullptr);
  EXPECT_EQ(process->getClassDefn()->getName(), "process");

  const hldb::MethodFuncCall *const self = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(self, nullptr) << "'self()' should be a MethodFuncCall";
  EXPECT_EQ(self->getName(), "self");
  const hldb::Function *const selfMethod = self->getTaskFunc<hldb::Function>();
  ASSERT_NE(selfMethod, nullptr) << "'process::self()' should resolve to a real Function (it returns a handle)";
  EXPECT_EQ(selfMethod->getName(), "self");
}

TEST_F(ProcessClsAwaitTest, DisplayCallHasStringAndKArguments) {
  const hldb::SysTaskCall *const display = getDisplayCall();
  ASSERT_NE(display, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(display->getName(), "$display");

  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getDecompile(), "\"process %d\"");

  const hldb::RefObj *const kArg = any_cast<hldb::RefObj>(display->getArguments()->at(1));
  ASSERT_NE(kArg, nullptr);
  EXPECT_EQ(kArg->getName(), "k");
  EXPECT_EQ(kArg->getActual<hldb::Variable>(), getKVariable());
}

// --- foreach(job[i]) wait(job[i] != null); -----------------------------------

TEST_F(ProcessClsAwaitTest, SecondStmtIsWaitForeach) {
  EXPECT_NE(getWaitForeach(), nullptr) << "the second foreach-loop (the wait loop) should be the 3rd task statement";
}

TEST_F(ProcessClsAwaitTest, WaitForeachScopeHasItsOwnVariableI) {
  const hldb::ForeachStmt *const fe = getWaitForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getVariables(), nullptr);
  ASSERT_EQ(fe->getVariables()->size(), 1u)
      << "this foreach-loop has its own local scope, distinct from the fork foreach-loop's";
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", fe->getVariables()), nullptr) << "Variable 'i' not found";

  const hldb::ForeachStmt *const forkFe = getForkForeach();
  ASSERT_NE(forkFe, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", fe->getVariables()),
            hldb::findByName<hldb::Variable>("i", forkFe->getVariables()))
      << "each foreach-loop's 'i' must be its own distinct Variable (12.7.3's implicit per-loop scope)";
}

TEST_F(ProcessClsAwaitTest, WaitForeachBodyIsWaitStmt) {
  EXPECT_NE(getWaitStmt(), nullptr) << "'wait(job[i] != null);' should be a WaitStmt";
}

TEST_F(ProcessClsAwaitTest, WaitConditionIsJobOfINotEqualNull) {
  const hldb::WaitStmt *const wait = getWaitStmt();
  ASSERT_NE(wait, nullptr);
  const hldb::Operation *const cond = wait->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'job[i] != null' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiNeqOp);

  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);

  const hldb::BitSelect *const lhs = any_cast<hldb::BitSelect>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr) << "'job[i]' should be a BitSelect";
  EXPECT_EQ(lhs->getName(), "job[i]");
  EXPECT_EQ(lhs->getPrefix<hldb::RefObj>()->getActual<hldb::Variable>(), getJobVariable());

  const hldb::ForeachStmt *const waitFe = getWaitForeach();
  ASSERT_NE(waitFe, nullptr);
  EXPECT_EQ(lhs->getIndex<hldb::RefObj>()->getActual<hldb::Variable>(),
            hldb::findByName<hldb::Variable>("i", waitFe->getVariables()));

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr) << "'null' should be a Constant";
  EXPECT_EQ(rhs->getConstType(), vpiNullConst);
}

// --- job[1].await(); ----------------------------------------------------------

// 'job[1].await();' resolves the same way as 'process::self()' above: a
// RefObj whose 2 path elements are a BitSelect 'job[1]' (the handle being
// called on) and a MethodTaskCall named 'await' -- a task, not a function,
// since 'await()' returns nothing -- resolving via getTaskFunc<Task>() to
// 'process' class's own 'await' method.
TEST_F(ProcessClsAwaitTest, FourthStmtIsAwaitMethodCallOnJobOne) {
  const hldb::RefObj *const call = getAwaitCall();
  ASSERT_NE(call, nullptr) << "'job[1].await();' should bind directly as a bare RefObj statement";
  EXPECT_EQ(call->getName(), "job[1].await()");

  ASSERT_NE(call->getPathElems(), nullptr);
  ASSERT_EQ(call->getPathElems()->size(), 2u) << "'job[1]' handle, then the 'await' call";

  const hldb::BitSelect *const jobOne = any_cast<hldb::BitSelect>(call->getPathElems()->at(0));
  ASSERT_NE(jobOne, nullptr) << "'job[1]' should be a BitSelect";
  EXPECT_EQ(jobOne->getName(), "job[1]");
  EXPECT_EQ(jobOne->getPrefix<hldb::RefObj>()->getActual<hldb::Variable>(), getJobVariable());
  const hldb::Constant *const index = jobOne->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "1");

  const hldb::MethodTaskCall *const await = any_cast<hldb::MethodTaskCall>(call->getPathElems()->at(1));
  ASSERT_NE(await, nullptr) << "'await()' should be a MethodTaskCall";
  EXPECT_EQ(await->getName(), "await");
  const hldb::Task *const awaitMethod = await->getTaskFunc<hldb::Task>();
  ASSERT_NE(awaitMethod, nullptr) << "'job[1].await()' should resolve to a real Task (it returns nothing)";
  EXPECT_EQ(awaitMethod->getName(), "await");
}

// --- initial begin test(8); end ----------------------------------------------

TEST_F(ProcessClsAwaitTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(ProcessClsAwaitTest, InitialCallsTestWithConstantEight) {
  const hldb::TaskCall *const call = getTestCall();
  ASSERT_NE(call, nullptr) << "'test(8);' should be a TaskCall";
  EXPECT_EQ(call->getTaskFunc<hldb::Task>(), getTestTask()) << "'test(8)' should resolve to task 'test'";

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "8");
  EXPECT_EQ(arg->getConstType(), vpiUIntConst);
}

// --- compiler diagnostics -----------------------------------------------------

// Both 'process::self()' and 'job[1].await()' resolve to real methods of the
// built-in 'process' ClassDefn (see the two tests above), so neither should
// leave any reference unbound.
TEST_F(ProcessClsAwaitTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'self' and 'await' should both bind to the built-in 'process' class's own methods";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
