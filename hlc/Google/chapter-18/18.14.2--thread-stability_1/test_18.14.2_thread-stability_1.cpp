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

// ---------------------------------------------------------------------------
// SV source under test: tests/Google/chapter-18/18.14.2--thread-stability_1.sv
// (IEEE 1800-2023 18.14.2 "Thread stability", 18.14.1 hierarchical seeding, 9.7 process class)
//
//   /*
//   :name: thread_stability_1
//   :description: thread stability test
//   :tags: uvm-random uvm
//   */
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class env extends uvm_env;
//     int unsigned val1, val2;
//     process p1, p2;
//     string randstate1, randstate2;
//
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//
//         fork
//           begin
//               p1 = process::self();
//               randstate1 = p1.get_randstate();
//               val1 = $urandom;
//           end
//           begin
//               p2 = process::self();
//               randstate2 = p2.get_randstate();
//               val2 = $urandom;
//           end
//         join
//
//         if(val1 != val2 && randstate1 != randstate2) begin
//           `uvm_info("RESULT", $sformatf("val1 = %0d val2 = %0d SUCCESS", val1, val2), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("val1 = %0d val2 = %0d FAILED", val1, val2));
//         end
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//
//   endclass
//
//   module top;
//
//     env environment;
//
//     initial begin
//       environment = new("env");
//       run_test();
//     end
//
//   endmodule
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error.
//   2. env members: val1, val2 'int unsigned' (int, NOT signed, 6.11);
//      p1, p2 handles of the built-in class 'process' (9.7); randstate1,
//      randstate2 strings (get_randstate() returns string, 18.13.4). None
//      is rand.
//   3. run_phase has exactly one fork...join (9.3.2) with exactly two
//      begin...end threads.
//   4. Thread N (N = 1, 2), in order:
//        - "pN = process::self();"           : rhs is a call to self,
//        - "randstateN = pN.get_randstate();": get_randstate applied to
//                                              pN, no argument,
//        - "valN = $urandom;"                : rhs is $urandom (18.13.1),
//                                              no seed argument.
//      No srandom call: the threads keep their hierarchical seeds.
//   5. RUN TIME (18.14.1/18.14.2 hierarchical seeding): "Each thread is
//      seeded with a unique value, determined solely by its parent", so the
//      two thread RNG states differ (randstate1 != randstate2) and so do the
//      drawn values (val1 != val2).
//  ENV/TOP (every UVM file):
//   - Class 'env' extends the UVM base class 'uvm_env' (8.13).
//   - Module 'top' is a top-level instance (23.3.1) and declares
//     'environment' as a handle of class 'env'.
//
// CHECKED (TEST_F below):
//   - items 1 to 4 and ENV/TOP.
//   - item 5 (run time) runs only on an elaborated design
//     (m_design->getElaborated()); otherwise it is skipped.
//
// NOT CHECKED (and why):
//   - The UVM reporting macros (`uvm_info / `uvm_error) and the if
//     condition that prints SUCCESS/FAILED: UVM plumbing, not part of the
//     clause under test.
//   - The exact RNG state string: 18.13.4 says its length and contents are
//     implementation dependent, so only its type (string) is checkable.
// ---------------------------------------------------------------------------

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/for_stmt.h>
#include <hldb/fork_stmt.h>
#include <hldb/function.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/task.h>
#include <hldb/tf_call.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct CallSite {
  const hldb::TFCall *m_call = nullptr;
  // Hierarchical reference "x.y.method(...)" that holds the call, if any.
  const hldb::RefObj *m_owner = nullptr;
};

// Collects, in source order, every task/function call reachable from a statement.
void collectCalls(const hldb::Any *node, const hldb::RefObj *owner, std::vector<CallSite> *out) {
  if (node == nullptr) return;
  if (const hldb::TFCall *const call = any_cast<hldb::TFCall>(node)) {
    out->emplace_back(CallSite{call, owner});
  } else if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(node)) {
    if (const hldb::AnyCollection *const elems = ref->getPathElems()) {
      for (const hldb::Any *const elem : *elems) collectCalls(elem, ref, out);
    }
  } else if (const hldb::Assignment *const assign = any_cast<hldb::Assignment>(node)) {
    collectCalls(assign->getRhs(), nullptr, out);
  } else if (const hldb::Begin *const begin = any_cast<hldb::Begin>(node)) {
    if (const hldb::AnyCollection *const stmts = begin->getStmts()) {
      for (const hldb::Any *const stmt : *stmts) collectCalls(stmt, nullptr, out);
    }
  } else if (const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(node)) {
    if (const hldb::AnyCollection *const stmts = fork->getStmts()) {
      for (const hldb::Any *const stmt : *stmts) collectCalls(stmt, nullptr, out);
    }
  } else if (const hldb::ForStmt *const loop = any_cast<hldb::ForStmt>(node)) {
    collectCalls(loop->getStmt(), nullptr, out);
  } else if (const hldb::CaseStmt *const cs = any_cast<hldb::CaseStmt>(node)) {
    if (const hldb::CaseItemCollection *const items = cs->getCaseItems()) {
      for (const hldb::CaseItem *const item : *items) collectCalls(item->getStmt(), nullptr, out);
    }
  } else if (const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(node)) {
    collectCalls(ifElse->getStmt(), nullptr, out);
    collectCalls(ifElse->getElseStmt(), nullptr, out);
  } else if (const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(node)) {
    collectCalls(ifStmt->getStmt(), nullptr, out);
  }
}

std::vector<CallSite> collectCalls(const hldb::Any *node) {
  std::vector<CallSite> calls;
  collectCalls(node, nullptr, &calls);
  return calls;
}

// Keeps, in order, only the calls whose name is one of 'names'.
std::vector<CallSite> filterCalls(const std::vector<CallSite> &calls, std::initializer_list<std::string_view> names) {
  std::vector<CallSite> kept;
  for (const CallSite &site : calls) {
    if (std::find(names.begin(), names.end(), site.m_call->getName()) != names.end()) kept.emplace_back(site);
  }
  return kept;
}

// Name of the i-th element of a hierarchical reference, or "" when absent.
std::string_view pathElemName(const hldb::RefObj *owner, size_t index) {
  if ((owner == nullptr) || (owner->getPathElems() == nullptr) || (owner->getPathElems()->size() <= index)) return "";
  return owner->getPathElems()->at(index)->getName();
}

size_t argumentCount(const hldb::TFCall *call) {
  return (call->getArguments() == nullptr) ? 0 : call->getArguments()->size();
}

// Collects, in source order, every statement of type T reachable from 'node' (nested blocks included).
template <typename T>
void collectStmts(const hldb::Any *node, std::vector<const T *> *out) {
  if (node == nullptr) return;
  if (const T *const match = any_cast<T>(node)) out->emplace_back(match);
  if (const hldb::Begin *const begin = any_cast<hldb::Begin>(node)) {
    if (const hldb::AnyCollection *const stmts = begin->getStmts()) {
      for (const hldb::Any *const stmt : *stmts) collectStmts(stmt, out);
    }
  } else if (const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(node)) {
    if (const hldb::AnyCollection *const stmts = fork->getStmts()) {
      for (const hldb::Any *const stmt : *stmts) collectStmts(stmt, out);
    }
  } else if (const hldb::ForStmt *const loop = any_cast<hldb::ForStmt>(node)) {
    collectStmts(loop->getStmt(), out);
  } else if (const hldb::CaseStmt *const cs = any_cast<hldb::CaseStmt>(node)) {
    if (const hldb::CaseItemCollection *const items = cs->getCaseItems()) {
      for (const hldb::CaseItem *const item : *items) collectStmts(item->getStmt(), out);
    }
  } else if (const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(node)) {
    collectStmts(ifElse->getStmt(), out);
    collectStmts(ifElse->getElseStmt(), out);
  } else if (const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(node)) {
    collectStmts(ifStmt->getStmt(), out);
  }
}

template <typename T>
std::vector<const T *> collectStmts(const hldb::Any *node) {
  std::vector<const T *> found;
  collectStmts(node, &found);
  return found;
}

hldb::AnyType typespecKind(const hldb::RefTypespec *ref) {
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}

const hldb::ClassDefn *classOf(const hldb::Variable *var) {
  if ((var == nullptr) || (var->getTypespec() == nullptr)) return nullptr;
  const hldb::ClassTypespec *const ts = var->getTypespec()->getActual<hldb::ClassTypespec>();
  return (ts == nullptr) ? nullptr : ts->getClassDefn();
}
}  // namespace

namespace hlc {
class ThreadStability1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.14.2--thread-stability_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClass(std::string_view name) const {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }
  const hldb::Task *getRunPhase() const {
    const hldb::ClassDefn *const env = getClass("env");
    return (env == nullptr) ? nullptr : hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  }
};

// Check: legal file. Fatal and syntax diagnostics are checked for the whole
// compile. Error diagnostics are checked only where they point into this file:
// HLC reports errors inside the UVM 1800.2-2017 library itself (the same set as
// BuildUVMPkg), and those are not about this source.
TEST_F(ThreadStability1Test, CompilerReportsNoErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);

  size_t count = 0;
  for (const Event &error : m_session->getEventListener()->getEvents()) {
    const EventDefinition *const info = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if (info == nullptr || info->m_severity > EventSeverity::Error) continue;
    for (const EventArg &loc : error.getArguments()) {
      const std::string_view path = loc.m_file;
      if (std::filesystem::path(path).filename() != "18.14.2--thread-stability_1.sv") continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Check: env extends uvm_env and declares its members with the right types.
TEST_F(ThreadStability1Test, EnvExtendsUvmEnvAndDeclaresMembers) {
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr) << "class 'env' not found";
  ASSERT_NE(env->getExtends(), nullptr) << "env has an 'extends uvm_env' clause";
  ASSERT_NE(env->getExtends()->getClassTypespecs(), nullptr);
  ASSERT_EQ(env->getExtends()->getClassTypespecs()->size(), 1u);
  const hldb::ClassTypespec *const base =
      env->getExtends()->getClassTypespecs()->at(0)->getActual<hldb::ClassTypespec>();
  ASSERT_NE(base, nullptr);
  ASSERT_NE(base->getClassDefn(), nullptr) << "base class should bind to the uvm_env definition";
  EXPECT_EQ(base->getClassDefn()->getName(), "uvm_env");

  const hldb::Variable *const val1 = hldb::findByName<hldb::Variable>("val1", env->getVariables());
  ASSERT_NE(val1, nullptr) << "member 'val1' not found";
  EXPECT_EQ(val1->getRandType(), vpiNotRand) << "val1 is not declared rand";
  EXPECT_EQ(typespecKind(val1->getTypespec()), hldb::AnyType::IntTypespec) << "val1 is 'int'";
  ASSERT_NE(val1->getTypespec(), nullptr);
  const hldb::IntTypespec *const val1Ts = val1->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(val1Ts, nullptr);
  EXPECT_FALSE(val1Ts->getSigned()) << "val1 is declared 'int unsigned' (6.11)";
  const hldb::Variable *const val2 = hldb::findByName<hldb::Variable>("val2", env->getVariables());
  ASSERT_NE(val2, nullptr) << "member 'val2' not found";
  EXPECT_EQ(val2->getRandType(), vpiNotRand) << "val2 is not declared rand";
  EXPECT_EQ(typespecKind(val2->getTypespec()), hldb::AnyType::IntTypespec) << "val2 is 'int'";
  ASSERT_NE(val2->getTypespec(), nullptr);
  const hldb::IntTypespec *const val2Ts = val2->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(val2Ts, nullptr);
  EXPECT_FALSE(val2Ts->getSigned()) << "val2 is declared 'int unsigned' (6.11)";
  const hldb::Variable *const p1 = hldb::findByName<hldb::Variable>("p1", env->getVariables());
  ASSERT_NE(p1, nullptr) << "member 'p1' not found";
  EXPECT_EQ(p1->getRandType(), vpiNotRand) << "p1 is not declared rand";
  ASSERT_NE(classOf(p1), nullptr) << "p1 is a handle of the built-in class 'process' (9.7)";
  EXPECT_EQ(classOf(p1)->getName(), "process");
  const hldb::Variable *const p2 = hldb::findByName<hldb::Variable>("p2", env->getVariables());
  ASSERT_NE(p2, nullptr) << "member 'p2' not found";
  EXPECT_EQ(p2->getRandType(), vpiNotRand) << "p2 is not declared rand";
  ASSERT_NE(classOf(p2), nullptr) << "p2 is a handle of the built-in class 'process' (9.7)";
  EXPECT_EQ(classOf(p2)->getName(), "process");
  const hldb::Variable *const randstate1 = hldb::findByName<hldb::Variable>("randstate1", env->getVariables());
  ASSERT_NE(randstate1, nullptr) << "member 'randstate1' not found";
  EXPECT_EQ(randstate1->getRandType(), vpiNotRand) << "randstate1 is not declared rand";
  EXPECT_EQ(typespecKind(randstate1->getTypespec()), hldb::AnyType::StringTypespec)
      << "randstate1 is a string (get_randstate() returns string, 18.13.4)";
  const hldb::Variable *const randstate2 = hldb::findByName<hldb::Variable>("randstate2", env->getVariables());
  ASSERT_NE(randstate2, nullptr) << "member 'randstate2' not found";
  EXPECT_EQ(randstate2->getRandType(), vpiNotRand) << "randstate2 is not declared rand";
  EXPECT_EQ(typespecKind(randstate2->getTypespec()), hldb::AnyType::StringTypespec)
      << "randstate2 is a string (get_randstate() returns string, 18.13.4)";
}

// Check: run_phase has one fork...join of two begin...end threads.
TEST_F(ThreadStability1Test, RunPhaseForksTwoThreadsWithJoin) {
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr) << "env::run_phase not found";
  const std::vector<const hldb::ForkStmt *> forks = collectStmts<hldb::ForkStmt>(runPhase->getStmt());
  ASSERT_EQ(forks.size(), 1u) << "run_phase contains exactly one fork";
  EXPECT_EQ(forks[0]->getJoinType(), vpiJoin) << "the fork ends with 'join' (9.3.2)";
  ASSERT_NE(forks[0]->getStmts(), nullptr);
  ASSERT_EQ(forks[0]->getStmts()->size(), 2u) << "the fork has two threads";
  for (const hldb::Any *const thread : *forks[0]->getStmts()) {
    ASSERT_NE(thread, nullptr);
    EXPECT_EQ(thread->getAnyType(), hldb::AnyType::Begin) << "each thread is a begin...end block";
  }
}

// Check: each forked thread takes its own process handle (process::self()) and draws $urandom.
TEST_F(ThreadStability1Test, EachForkedThreadUsesItsOwnProcess) {
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr);
  const std::vector<const hldb::ForkStmt *> forks = collectStmts<hldb::ForkStmt>(runPhase->getStmt());
  ASSERT_EQ(forks.size(), 1u);
  ASSERT_NE(forks[0]->getStmts(), nullptr);
  ASSERT_EQ(forks[0]->getStmts()->size(), 2u);
  const char *const processes[] = {"p1", "p2"};
  const char *const values[] = {"val1", "val2"};
  const char *const states[] = {"randstate1", "randstate2"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Any *const branch = forks[0]->getStmts()->at(i);
    const std::vector<const hldb::Assignment *> assigns = collectStmts<hldb::Assignment>(branch);
    ASSERT_EQ(assigns.size(), 3u) << "thread " << i + 1 << " has three assignments";

    // "pN = process::self();" -- comes first in the thread.
    ASSERT_NE(assigns[0]->getLhs(), nullptr);
    EXPECT_EQ(assigns[0]->getLhs()->getName(), processes[i]);
    EXPECT_EQ(filterCalls(collectCalls(assigns[0]->getRhs()), {"self"}).size(), 1u)
        << processes[i] << " is assigned process::self() (9.7)";

    // "randstateN = pN.get_randstate();"
    ASSERT_NE(assigns[1]->getLhs(), nullptr);
    EXPECT_EQ(assigns[1]->getLhs()->getName(), states[i]);
    const std::vector<CallSite> getState = filterCalls(collectCalls(assigns[1]->getRhs()), {"get_randstate"});
    ASSERT_EQ(getState.size(), 1u) << states[i] << " is assigned " << processes[i] << ".get_randstate()";
    EXPECT_EQ(pathElemName(getState[0].m_owner, 0), processes[i]);
    EXPECT_EQ(argumentCount(getState[0].m_call), 0u);

    // "valN = $urandom;" -- comes last in the thread.
    ASSERT_NE(assigns[2]->getLhs(), nullptr);
    EXPECT_EQ(assigns[2]->getLhs()->getName(), values[i]);
    const hldb::SysFuncCall *const urandom = any_cast<hldb::SysFuncCall>(assigns[2]->getRhs());
    ASSERT_NE(urandom, nullptr) << values[i] << " is assigned the system function $urandom (18.13.1)";
    EXPECT_EQ(urandom->getName(), "$urandom");
    EXPECT_EQ(argumentCount(urandom), 0u) << "$urandom is called without a seed argument";
  }
}

// Check: neither thread re-seeds its RNG (no srandom call anywhere in run_phase).
TEST_F(ThreadStability1Test, ThreadsKeepTheirHierarchicalSeeds) {
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr);
  EXPECT_TRUE(filterCalls(collectCalls(runPhase->getStmt()), {"srandom"}).empty())
      << "the threads are not manually seeded";
}

// Check: module top declares 'env environment'.
TEST_F(ThreadStability1Test, TopDeclaresEnvironmentHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  if (m_design->getElaborated()) {
    // IEEE 1800-2023 Sec 23.3.1: a top-level module is implicitly instantiated once, under its own name.
    EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getTopModules()), nullptr)
        << "top-level instance 'top' not found";
  }
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr) << "variable 'environment' not found in top";
  EXPECT_EQ(classOf(environment), getClass("env")) << "'environment' is declared with type 'env'";
}

// Check (run time): runs only on an elaborated design.
TEST_F(ThreadStability1Test, RuntimeThreadsGetDifferentSeeds) {
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "Design is not elaborated, so no run-time value is available. "
                    "IEEE 1800-2023 Sec 18.14.2: each child thread is seeded with a unique value from its parent, so "
                    "randstate1 != randstate2 and val1 != val2.";
  }
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr);
  const hldb::Variable *const val1 = hldb::findByName<hldb::Variable>("val1", env->getVariables());
  ASSERT_NE(val1, nullptr);
  ASSERT_NE(val1->getValue(), nullptr) << "val1 should hold its value after run_phase executed";
  const hldb::Constant *const val1Value = val1->getValue<hldb::Constant>();
  ASSERT_NE(val1Value, nullptr);
  const hldb::Variable *const val2 = hldb::findByName<hldb::Variable>("val2", env->getVariables());
  ASSERT_NE(val2, nullptr);
  ASSERT_NE(val2->getValue(), nullptr) << "val2 should hold its value after run_phase executed";
  const hldb::Constant *const val2Value = val2->getValue<hldb::Constant>();
  ASSERT_NE(val2Value, nullptr);
  const hldb::Variable *const state1 = hldb::findByName<hldb::Variable>("randstate1", env->getVariables());
  ASSERT_NE(state1, nullptr);
  ASSERT_NE(state1->getValue(), nullptr) << "randstate1 should hold its value after run_phase executed";
  const hldb::Constant *const state1Value = state1->getValue<hldb::Constant>();
  ASSERT_NE(state1Value, nullptr);
  const hldb::Variable *const state2 = hldb::findByName<hldb::Variable>("randstate2", env->getVariables());
  ASSERT_NE(state2, nullptr);
  ASSERT_NE(state2->getValue(), nullptr) << "randstate2 should hold its value after run_phase executed";
  const hldb::Constant *const state2Value = state2->getValue<hldb::Constant>();
  ASSERT_NE(state2Value, nullptr);
  EXPECT_NE(val1Value->getDecompile(), val2Value->getDecompile());
  EXPECT_NE(state1Value->getDecompile(), state2Value->getDecompile());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
