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

// Tests for subroutines.sv (tags: 7.7 7.4.2)
//   module top ();
//     task fun(int a [2:0]);
//       $display(":assert: ((%d == 0) and (%d == 1) and (%d == 2))",
//         a[0], a[1], a[2]);
//     endtask;
//     initial begin
//       int b [2:0];
//       b[0] = 0;
//       b[1] = 1;
//       b[2] = 2;
//       $display(":assert: ((%d == 0) and (%d == 1) and (%d == 2))",
//         b[0], b[1], b[2]);
//       fun(b);
//     end
//   endmodule
//
// Checked:
//   - design has module top with zero nets (no module-level variable
//     declarations; "b" is a process-local Variable, "a" is a task IODecl)
//   - module has exactly 1 task "fun" with 1 IODecl "a": direction=input,
//     typespec -> ArrayTypespec static(1) range [2:0], elem -> IntTypespec
//     (signed)
//   - task body is NOT wrapped in a Begin (single unpacked statement): the
//     task's vpiStmt is directly a SysTaskCall "$display" with 4 args
//     (format + BitSelect a[0], a[1], a[2], each prefix RefObj "a"
//     resolving the IODecl "a")
//   - Initial process: Begin declaring 1 local Variable "b" (typespec ->
//     ArrayTypespec static(1) range [2:0], elem -> IntTypespec signed) and
//     containing 5 stmts (3 BitSelect Assignment + 1 SysFuncCall + 1
//     FuncCall)
//   - Stmt[0..2]: blocking Assignment, lhs BitSelect "b[N]" (prefix RefObj
//     "b" resolving the local Variable "b", Constant index N), rhs
//     Constant N, for N in 0..2
//   - Stmt[3]: $display with 4 args (format + BitSelect b[0], b[1], b[2])
//   - Stmt[4]: fun(b) -- FuncCall "fun" with 1 argument, RefObj "b"
//     resolving the local Variable "b"
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//   - no continuous assignments
//
// Not checked:
//   - actual runtime $display output inside task fun() when invoked via
//     fun(b) -- that requires running a simulator, which this harness does
//     not do (it only compiles/elaborates). subroutines.sv's own $display
//     format strings (both the task-local one and the caller's) document the
//     expected runtime values, but nothing here can observe them.
//
// Checked as a known compile-time gap (see FuncCallResolvesToDeclaredTask
// below): whether the FuncCall "fun" resolves (via getTaskFunc<Task>()) back
// to the declared Task "fun". Unlike runtime $display output, this IS
// something the compiler could resolve without simulating anything -- it is
// pure compile-time name binding from a call site to its declaration. There
// is no working example anywhere in this repo of that resolution being
// populated, so this test is expected to fail until the compiler implements
// it.

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
#include <hldb/func_call.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/task.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class UnpackedSubroutinesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "subroutines.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Task *getTaskFun() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Task>("fun", top->getTaskFuncs());
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }
};

// --- module ------------------------------------------------------------------

TEST_F(UnpackedSubroutinesTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(UnpackedSubroutinesTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr);
}

// --- task fun(int a [2:0]) ---------------------------------------------------

TEST_F(UnpackedSubroutinesTest, ModuleHasOneTaskFunc) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
}

TEST_F(UnpackedSubroutinesTest, TaskIsFunNamed) { EXPECT_NE(getTaskFun(), nullptr); }

TEST_F(UnpackedSubroutinesTest, TaskHasOneIODeclNamedA) {
  const hldb::Task *const task = getTaskFun();
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  ASSERT_EQ(task->getIODecls()->size(), 1u);
  EXPECT_EQ(task->getIODecls()->at(0)->getName(), "a");
}

TEST_F(UnpackedSubroutinesTest, IODeclDirectionIsInput) {
  const hldb::Task *const task = getTaskFun();
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->getIODecls()->at(0)->getDirection(), vpiInput);
}

TEST_F(UnpackedSubroutinesTest, IODeclTypespecIsArrayOfSignedIntRangeTwoToZero) {
  const hldb::Task *const task = getTaskFun();
  ASSERT_NE(task, nullptr);
  const hldb::IODecl *const a = task->getIODecls()->at(0);
  ASSERT_NE(a, nullptr);
  const hldb::ArrayTypespec *const at = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "2");
  EXPECT_EQ(at->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(UnpackedSubroutinesTest, TaskBodyIsBareDisplayNotWrappedInBegin) {
  const hldb::Task *const task = getTaskFun();
  ASSERT_NE(task, nullptr);
  const hldb::SysTaskCall *const disp = task->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(disp, nullptr) << "single-statement task body should not be wrapped in a Begin";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 4u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 0) and (%d == 1) and (%d == 2))");
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(i + 1));
    ASSERT_NE(sel, nullptr) << "argument " << (i + 1);
    EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "a");
    EXPECT_NE(sel->getPrefix<hldb::RefObj>()->getActual<hldb::IODecl>(), nullptr);
    EXPECT_EQ(sel->getIndex<hldb::Constant>()->getDecompile(), std::to_string(i));
  }
}

// --- initial process: local variable b + fun(b) -----------------------------

TEST_F(UnpackedSubroutinesTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(UnpackedSubroutinesTest, InitialBeginDeclaresLocalVariableB) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getVariables(), nullptr);
  ASSERT_EQ(begin->getVariables()->size(), 1u);
  const hldb::Variable *const b = any_cast<hldb::Variable>(begin->getVariables()->at(0));
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), "b");
  const hldb::ArrayTypespec *const at = b->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "2");
  EXPECT_EQ(at->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(UnpackedSubroutinesTest, InitialBeginHasFiveStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 5u);
}

TEST_F(UnpackedSubroutinesTest, FirstThreeStmtsAssignBIndicesToMatchingConstants) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(i));
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "]";
    EXPECT_TRUE(assign->getBlocking());
    const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "b[" + std::to_string(i) + "]");
    EXPECT_NE(lhs->getPrefix<hldb::RefObj>()->getActual<hldb::Variable>(), nullptr);
    EXPECT_EQ(lhs->getIndex<hldb::Constant>()->getDecompile(), std::to_string(i));
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), std::to_string(i));
  }
}

TEST_F(UnpackedSubroutinesTest, FourthStmtDisplaysBIndices) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 4u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 0) and (%d == 1) and (%d == 2))");
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(i + 1));
    ASSERT_NE(sel, nullptr) << "argument " << (i + 1);
    EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "b");
    EXPECT_EQ(sel->getIndex<hldb::Constant>()->getDecompile(), std::to_string(i));
  }
}

TEST_F(UnpackedSubroutinesTest, FifthStmtIsFunCallWithBArgument) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::TaskCall *const tc = any_cast<hldb::TaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(tc, nullptr) << "'fun(b)' should be a TaskCall";
  EXPECT_EQ(tc->getName(), "fun");
  ASSERT_NE(tc->getArguments(), nullptr);
  ASSERT_EQ(tc->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(tc->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "b");
  EXPECT_NE(arg->getActual<hldb::Variable>(), nullptr);
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(UnpackedSubroutinesTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(UnpackedSubroutinesTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(UnpackedSubroutinesTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

TEST_F(UnpackedSubroutinesTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(UnpackedSubroutinesTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- known gap: call-site to declaration binding for task invocations -------

// 'fun(b)' is parsed as a FuncCall (see FifthStmtIsFunCallWithBArgument
// above). A compiler that performs full compile-time name binding must also
// link that call site back to the Task it invokes, via getTaskFunc<Task>().
// This is NOT a simulation question -- no execution is required to know that
// 'fun(b)' calls the task declared as 'task fun(...)' two lines above it.
TEST_F(UnpackedSubroutinesTest, TaskCallResolvesToDeclaredTask) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::TaskCall *const tc = any_cast<hldb::TaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(tc, nullptr) << "'fun(b)' should be a TaskCall";
  const hldb::Task *const declaredTask = getTaskFun();
  ASSERT_NE(declaredTask, nullptr);
  EXPECT_EQ(tc->getTaskFunc<hldb::Task>(), declaredTask)
      << "call-site 'fun(b)' must resolve (via getTaskFunc<Task>()) back to the "
         "declared task 'fun' -- if this fails, the compiler is not performing "
         "compile-time name binding from task-invocation call sites to their "
         "declarations (a pure static/elaboration-time capability, unrelated "
         "to simulation)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
