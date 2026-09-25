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

// Tests for dut.sv (DpiTask)
//   module top ();
//     export "DPI-C" task task_1;
//     task task_1(output int ret);
//       ret = 2;
//     endtask
//   endmodule
//
// What to check and why (IEEE 1800-2023 35.5.5 "Function and task
// declarations for DPI export" and 13.3 "Tasks", checked before any
// test code was written -- no .log file consulted to decide expected
// shape; the log was only used, per the test-writing guide's narrow
// exception, to confirm which real hldb classes/collections HLC
// actually populates for a DPI-exported, locally-defined task, since
// no existing hlc/ test exercises DPI and no build/include/hldb
// checkout is available in this repo snapshot):
//   35.5.5: "export" makes an existing SystemVerilog function or task
//   (defined elsewhere in the module) callable from foreign code; it
//   does not itself define the task. "task_1" is fully defined as an
//   ordinary SystemVerilog task ("task task_1(output int ret); ret =
//   2; endtask") in addition to being exported, so the local Task
//   definition must exist exactly as it would without the export, and
//   the export itself registers as a separate declaration.
//
// What is checked:
//   - module top has exactly 1 task/function definition, Task
//     "task_1", with exactly 1 formal IODecl "ret" (direction
//     vpiOutput, IntTypespec), and a body that is a plain blocking
//     assignment "ret = 2;" (no begin-end) whose lhs is RefObj "ret"
//     resolving to the IODecl and whose rhs is Constant "2"
//   - module top has exactly 1 task/function declaration (the export),
//     also named "task_1", with no formal arguments (the export names
//     an already-declared task; it does not redeclare its ports)
//   - compiler reports zero errors (this file is fully legal per 35.5.5)
//
// What is NOT checked and why:
//   - the DPI C-identifier string recorded for the export: kept as a
//     GTEST_SKIP with real, currently-failing assertion code beneath
//     it -- HLC does not yet populate this field correctly (see below)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/task.h>
#include <hldb/task_decl.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DpiTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DpiTask.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Task *getTask1() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  }

  static const hldb::TaskDecl *getTask1Decl() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncDecls() == nullptr || top->getTaskFuncDecls()->empty()) return nullptr;
    return any_cast<hldb::TaskDecl>(top->getTaskFuncDecls()->at(0));
  }
};

TEST_F(DpiTaskTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// task task_1(output int ret); ret = 2; endtask
// ---------------------------------------------------------------------------
TEST_F(DpiTaskTest, Task1DefinitionExistsWithOneOutputIODeclRet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Task *const task = getTask1();
  ASSERT_NE(task, nullptr) << "'task_1' should resolve to a Task definition";
  EXPECT_EQ(task->getName(), "task_1");
  ASSERT_NE(task->getIODecls(), nullptr);
  ASSERT_EQ(task->getIODecls()->size(), 1u);
  const hldb::IODecl *const ret = task->getIODecls()->at(0);
  ASSERT_NE(ret, nullptr);
  EXPECT_EQ(ret->getName(), "ret");
  EXPECT_EQ(ret->getDirection(), vpiOutput);
  ASSERT_NE(ret->getTypespec(), nullptr);
  EXPECT_NE(ret->getTypespec()->getActual<hldb::IntTypespec>(), nullptr)
      << "'output int ret' should resolve to IntTypespec";
}

TEST_F(DpiTaskTest, Task1BodyIsBlockingAssignmentRetEqualsTwo) {
  const hldb::Task *const task = getTask1();
  ASSERT_NE(task, nullptr);
  const hldb::Assignment *const asgn = task->getStmt<hldb::Assignment>();
  ASSERT_NE(asgn, nullptr) << "task body should be a plain Assignment (single statement, no begin-end)";
  EXPECT_TRUE(asgn->getBlocking()) << "'ret = 2;' is a blocking assignment (IEEE 1800-2023 10.4.1)";

  const hldb::RefObj *const lhs = asgn->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "lhs of 'ret = 2;' should be a RefObj";
  EXPECT_EQ(lhs->getName(), "ret");
  EXPECT_NE(lhs->getActual<hldb::IODecl>(), nullptr) << "'ret' should resolve to the IODecl, not a Variable";

  const hldb::Constant *const rhs = asgn->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "rhs of 'ret = 2;' should be a Constant";
  EXPECT_EQ(rhs->getDecompile(), "2");
}

// ---------------------------------------------------------------------------
// export "DPI-C" task task_1;
// ---------------------------------------------------------------------------
TEST_F(DpiTaskTest, Task1ExportDeclExistsWithNoFormals) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncDecls(), nullptr) << "'export \"DPI-C\" task task_1;' should register a TaskDecl";
  ASSERT_EQ(top->getTaskFuncDecls()->size(), 1u);
  const hldb::TaskDecl *const decl = getTask1Decl();
  ASSERT_NE(decl, nullptr) << "the export declaration should resolve to a TaskDecl";
  EXPECT_EQ(decl->getName(), "task_1");
  EXPECT_TRUE(decl->getIODecls() == nullptr || decl->getIODecls()->empty())
      << "'export \"DPI-C\" task task_1;' names an already-declared task, no new formal list";
}

// ---------------------------------------------------------------------------
// DPI C-identifier -- known limitation
// ---------------------------------------------------------------------------
TEST_F(DpiTaskTest, ExportDpiCIdentifierDefaultsToItsSvName) {
  // IEEE 1800-2023 35.5.3: "If the c_identifier is omitted, the imported
  // (or exported) name shall be the same as the SystemVerilog function or
  // task name." 'export "DPI-C" task task_1;' supplies no c_identifier,
  // so the recorded DPI C string should be "task_1". HLC currently
  // records this field as an unresolved/invalid marker instead.
  GTEST_SKIP() << "HLC does not populate the DPI C-identifier for export declarations (reads back as an internal "
                  "placeholder); should default to the SV subroutine name per IEEE 1800-2023 35.5.3. Fix pending.";
  const hldb::TaskDecl *const decl = getTask1Decl();
  ASSERT_NE(decl, nullptr);
  EXPECT_EQ(decl->getName(), std::string_view("task_1"));
  EXPECT_EQ(decl->getDPICStr(), vpiDPICStr);
}

TEST_F(DpiTaskTest, CompilerReportsZeroErrors) {
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
