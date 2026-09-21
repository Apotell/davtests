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

// Regression test for tests/Google/chapter-21/21.2--monitor.sv.
//
// The fixture is one module with one `int` variable and one `initial` block
// holding, in this order:
//
//     $monitoron;  
//     $monitor(a);  
//     $monitorb(a);  
//     $monitoro(a);  
//     $monitorh(a);  
//     $monitoroff;
//
// The regression this file exists to catch is the collapsing of the monitor
// family into one another. IEEE 1800-2017 21.2.3 defines $monitor, $monitorb,
// $monitoro and $monitorh as four distinct tasks that differ *only* in the
// default radix applied to arguments carrying no format specifier, and it
// separately defines $monitoron / $monitoroff as the argument-less monitor
// enable and disable controls. Because none of the six calls here carries a
// format string, the recorded name is the only place in the model that still
// says which member of the family was written: a test that merely checked
// "six system task calls, four of them with one argument" would pass unchanged
// if the parser mapped every spelling onto plain $monitor, or if it swapped
// $monitoron and $monitoroff. So each name is pinned literally, and pinned in
// source order.
//
// The second thing pinned is binding: all four formatted calls name the same
// module-scope `int a` (23.6 name resolution), so each argument must resolve to
// the *same* Variable object the module declares, not to a fresh unbound
// reference per call site.
//
// What is deliberately NOT checked, and why:
//   - Source line/column of any node. A line number encodes no SystemVerilog
//     semantics and breaks the moment the .sv is reformatted.
//   - Design-level typespec ownership. Which scope ends up owning the shared
//     `int` typespec is a tool-internal sharing convention; the type is
//     verified through the declaration's own typespec chain instead.
//   - Library-qualified names (getDefName/getFullName) and warning counts,
//     which are tool conventions rather than things the source text fixes.
//   - Root-module status (getTopModules() / Module::getTopModule()). Nothing in
//     the file instantiates `top`, so 23.3.1 does make it a root module, but
//     this fixture's command file runs a non-elaborating flow, which leaves
//     that flag uncomputed. Everything checked below lives in the module
//     definition, which the parse/compile step does build.

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/any_type.h>
#include <hldb/begin.h>
#include <hldb/containers.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_task_call.h>
#include <hldb/tf_call.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string_view>
#include <vector>

namespace hlc {
namespace {

// One row per statement of the initial block, in the order the .sv writes them.
struct MonitorCall final {
  const char *m_name;           // the task name exactly as spelled in the source, '$' included
  std::size_t m_argumentCount;  // expressions between the parentheses; no parentheses at all == 0
};

constexpr std::array<MonitorCall, 6> kMonitorCalls = {{
    {"$monitoron", 0},   // 21.2.3: enables monitoring, takes no arguments
    {"$monitor", 1},     // 21.2.3: default radix, one argument `a`
    {"$monitorb", 1},    // 21.2.3: binary default radix
    {"$monitoro", 1},    // 21.2.3: octal default radix
    {"$monitorh", 1},    // 21.2.3: hexadecimal default radix
    {"$monitoroff", 0},  // 21.2.3: disables monitoring, takes no arguments
}};

// Size of a possibly-absent collection. HLDB leaves a collection null rather
// than empty when nothing was ever added to it, and "null" and "empty" mean the
// same thing to every check below, so both map to 0 here. Asserting a non-zero
// size through this helper therefore also proves the pointer is usable.
template <typename T>
std::size_t sizeOf(const std::vector<T *> *collection) {
  return (collection == nullptr) ? 0 : collection->size();
}

// AnyType of a node, or AnyType::Any when the node is missing, so that a hole in
// the model fails a type comparison instead of dereferencing a null pointer.
hldb::AnyType anyTypeOf(const hldb::Any *object) {
  return (object == nullptr) ? hldb::AnyType::Any : object->getAnyType();
}

}  // namespace

class MonitorTaskTest : public Test {
 protected:
  // This fixture's command file. CMake runs each test with its own source
  // directory as the working directory, and Test::Compile takes the compiler's
  // working directory from the parent of the path handed to it, so the command
  // file's own path is what pins both.
  static constexpr std::string_view kCommandFile = "21.2--monitor.hlc";

  static void SetUpTestSuite() { Compile(std::filesystem::current_path() / kCommandFile, {"-f", kCommandFile}); }

  static void TearDownTestSuite() { Shutdown(); }

  // Navigation helpers. Each returns nullptr when the node it looks for is not
  // there, so a broken chain surfaces as a failed comparison in the test that
  // cares about it rather than as a crash in an unrelated one.

  static const hldb::Module *topModule() {
    const hldb::ModuleCollection *const modules = m_design->getAllModules();
    return (sizeOf(modules) == 1) ? modules->front() : nullptr;
  }

  static const hldb::Variable *variableA() {
    const hldb::Module *const top = topModule();
    if (top == nullptr) return nullptr;

    const hldb::VariableCollection *const variables = top->getVariables();
    return (sizeOf(variables) == 1) ? variables->front() : nullptr;
  }

  static const hldb::Initial *initialProcess() {
    const hldb::Module *const top = topModule();
    if (top == nullptr) return nullptr;

    const hldb::ProcessCollection *const processes = top->getProcesses();
    return (sizeOf(processes) == 1) ? any_cast<hldb::Initial>(processes->front()) : nullptr;
  }

  static const hldb::Begin *initialBlock() {
    const hldb::Initial *const initial = initialProcess();
    return (initial == nullptr) ? nullptr : initial->getStmt<hldb::Begin>();
  }

  static const hldb::AnyCollection *initialStmts() {
    const hldb::Begin *const block = initialBlock();
    return (block == nullptr) ? nullptr : block->getStmts();
  }
};

// Everything in the fixture is legal IEEE 1800-2017 and all six task names are
// standard, so the compile must be clean of hard diagnostics.
TEST_F(MonitorTaskTest, CompilesWithoutErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);

  // 21.2 / 21.2.3: none of the six spellings is an unknown system name.
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_SYSTEM_FUNCTION), nullptr);
  // 23.6: `a` is visible from the initial block, so no reference may fail to bind.
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

// 23.2 / 23.2.2: the file declares exactly one module, named `top`, and
// `module top();` is the empty port list form, so it declares no ports.
TEST_F(MonitorTaskTest, DeclaresOneModuleNamedTopWithNoPorts) {
  ASSERT_EQ(sizeOf(m_design->getAllModules()), 1u);

  const hldb::Module *const top = topModule();
  ASSERT_EQ(anyTypeOf(top), hldb::AnyType::Module);
  EXPECT_EQ(top->getName(), "top");
  EXPECT_EQ(sizeOf(top->getPorts()), 0u);
  EXPECT_EQ(top->getEndLabel(), "");  // plain `endmodule`, no `: top` label
}

// 6.11.1 and Table 6-8: `int` is the 32-bit signed 2-state integer atom type.
// Being an atom rather than a vector type, it carries no packed range.
TEST_F(MonitorTaskTest, DeclaresOneSignedIntVariableNamedA) {
  const hldb::Module *const top = topModule();
  ASSERT_EQ(anyTypeOf(top), hldb::AnyType::Module);
  ASSERT_EQ(sizeOf(top->getVariables()), 1u);

  const hldb::Variable *const a = variableA();
  ASSERT_EQ(anyTypeOf(a), hldb::AnyType::Variable);
  EXPECT_EQ(a->getName(), "a");

  const hldb::Typespec *const typespec = hldb::getTypespec(a);
  ASSERT_EQ(anyTypeOf(typespec), hldb::AnyType::IntTypespec);

  const hldb::IntTypespec *const intTypespec = any_cast<hldb::IntTypespec>(typespec);
  EXPECT_TRUE(intTypespec->getSigned());
  EXPECT_EQ(sizeOf(intTypespec->getRanges()), 0u);
}

// 9.2.1: one `initial` construct. 9.3.1: its body is a `begin ... end`
// sequential block, unnamed because the source writes no `: label`, holding
// exactly the six statements of the fixture.
TEST_F(MonitorTaskTest, SingleInitialWrapsAnUnnamedSequentialBlockOfSixStatements) {
  const hldb::Module *const top = topModule();
  ASSERT_EQ(anyTypeOf(top), hldb::AnyType::Module);
  ASSERT_EQ(sizeOf(top->getProcesses()), 1u);
  ASSERT_EQ(anyTypeOf(top->getProcesses()->front()), hldb::AnyType::Initial);

  const hldb::Initial *const initial = initialProcess();
  ASSERT_EQ(anyTypeOf(initial->getStmt()), hldb::AnyType::Begin);

  const hldb::Begin *const block = initialBlock();
  EXPECT_EQ(block->getName(), "");
  EXPECT_EQ(block->getEndLabel(), "");
  EXPECT_EQ(sizeOf(block->getStmts()), kMonitorCalls.size());
}

// The core of the test: each of the six statements is a system *task* call
// (21.2 -- they are tasks, not functions, and are written here as statements),
// the recorded name is the exact spelling from the source, and the calls appear
// in the order written. $monitoron / $monitoroff take no arguments at all,
// while the four formatted variants take exactly one.
TEST_F(MonitorTaskTest, MonitorFamilyIsRecordedByNameInSourceOrder) {
  const hldb::AnyCollection *const stmts = initialStmts();
  ASSERT_EQ(sizeOf(stmts), kMonitorCalls.size());

  for (std::size_t i = 0; i < kMonitorCalls.size(); ++i) {
    SCOPED_TRACE(kMonitorCalls[i].m_name);

    ASSERT_EQ(anyTypeOf(stmts->at(i)), hldb::AnyType::SysTaskCall);

    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(stmts->at(i));
    EXPECT_EQ(call->getName(), kMonitorCalls[i].m_name);
    EXPECT_EQ(sizeOf(call->getArguments()), kMonitorCalls[i].m_argumentCount);
    EXPECT_FALSE(call->getUserDefn());  // 21.2 built-in, not a $systf registered by the user
  }
}

// 23.6: the `a` written inside the initial block is the `int a` declared in the
// enclosing module, so every one of the four formatted calls must resolve to
// that one Variable object.
TEST_F(MonitorTaskTest, EveryFormattedMonitorCallReferencesTheModuleVariableA) {
  const hldb::Variable *const a = variableA();
  ASSERT_EQ(anyTypeOf(a), hldb::AnyType::Variable);

  const hldb::AnyCollection *const stmts = initialStmts();
  ASSERT_EQ(sizeOf(stmts), kMonitorCalls.size());

  for (std::size_t i = 0; i < kMonitorCalls.size(); ++i) {
    if (kMonitorCalls[i].m_argumentCount == 0) continue;

    SCOPED_TRACE(kMonitorCalls[i].m_name);

    ASSERT_EQ(anyTypeOf(stmts->at(i)), hldb::AnyType::SysTaskCall);

    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(stmts->at(i));
    ASSERT_EQ(sizeOf(call->getArguments()), 1u);
    ASSERT_EQ(anyTypeOf(call->getArguments()->front()), hldb::AnyType::RefObj);

    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(call->getArguments()->front());
    EXPECT_EQ(ref->getName(), "a");
    EXPECT_EQ(ref->getActual<hldb::Variable>(), a);
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
