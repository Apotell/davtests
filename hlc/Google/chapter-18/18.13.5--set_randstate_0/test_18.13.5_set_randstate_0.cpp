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
// SV source under test: tests/Google/chapter-18/18.13.5--set_randstate_0.sv
// (IEEE 1800-2023 18.13.4 get_randstate() / 18.13.5 set_randstate())
//
//   /*
//   :name: set_randstate_0
//   :description: set_randstate() test
//   :tags: uvm-random uvm
//   */
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int x;
//   endclass
//
//   class env extends uvm_env;
//
//     a obj = new;
//     string randstate;
//     int prev_x;
//
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         randstate = obj.get_randstate();
//         obj.randomize();
//         prev_x = obj.x;
//         obj.set_randstate(randstate);
//         obj.randomize();
//
//         if(obj.x == prev_x) begin
//           `uvm_info("RESULT", $sformatf("obj.x = %0d prev_x = %0d SUCCESS", obj.x, prev_x), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("obj.x = %0d prev_x = %0d FAILED", obj.x, prev_x));
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
//   2. Class 'a' declares exactly 'rand int x' (vpiRand, int).
//   3. env members: 'obj' (non-rand handle of class a), 'randstate'
//      (string -- get_randstate() returns string, 18.13.4) and 'prev_x'
//      (plain signed int).
//   4. run_phase calls the RNG/randomize methods of obj in exactly this
//      order: obj.get_randstate(), obj.randomize(), obj.set_randstate(..),
//      obj.randomize(). Every call is applied to 'obj'.
//        - get_randstate() has no argument (prototype
//          "function string get_randstate();").
//        - set_randstate() has exactly one argument, the reference
//          'randstate' bound to env::randstate (prototype
//          "function void set_randstate(string state);").
//        - neither randomize() call has an inline 'with' block (37.42).
//   5. get_randstate and set_randstate bind to the built-in class methods
//      (18.13: every class has them).
//   6. Assignments in run_phase: 'randstate = obj.get_randstate();' (rhs
//      is the get_randstate call) and 'prev_x = obj.x;' (rhs is the
//      hierarchical reference obj.x).
//   7. RUN TIME (18.13.5): set_randstate restores the RNG state saved
//      before the first randomize(), so the second randomize() reproduces
//      the same value: obj.x == prev_x.
//  ENV/TOP (every UVM file):
//   - Class 'env' extends the UVM base class 'uvm_env' (8.13).
//   - Module 'top' is a top-level instance (23.3.1) and declares
//     'environment' as a handle of class 'env'.
//
// CHECKED (TEST_F below):
//   - items 1 to 6 and ENV/TOP.
//   - item 7 (run time) runs only on an elaborated design
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
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
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

// The caller-side expression of the i-th call argument, or nullptr.
const hldb::Any *argumentExpr(const hldb::TFCall *call, size_t index) {
  if ((call->getArguments() == nullptr) || (call->getArguments()->size() <= index)) return nullptr;
  const hldb::NamedArgument *const arg = call->getArguments()->at(index);
  if (arg->getHighConn() != nullptr) return arg->getHighConn();
  return arg->getLowConn();
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

// The assignment whose left-hand side is named 'lhs', or nullptr.
const hldb::Assignment *findAssignmentTo(const std::vector<const hldb::Assignment *> &assigns, std::string_view lhs) {
  for (const hldb::Assignment *const assign : assigns) {
    if ((assign->getLhs() != nullptr) && (assign->getLhs()->getName() == lhs)) return assign;
  }
  return nullptr;
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
class SetRandstate0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.13.5--set_randstate_0.hlc"}); }
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
TEST_F(SetRandstate0Test, CompilerReportsNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);

  const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
  size_t count = 0;
  for (const Error &error : m_session->getErrorContainer()->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator info = infos.find(error.getType());
    if (info == infos.cend() || info->second.m_severity > ErrorDefinition::ERROR) continue;
    for (const Location &loc : error.getLocations()) {
      const std::string_view path = FileSystem::toPath(loc.m_fileId);
      if (std::filesystem::path(path).filename() != "18.13.5--set_randstate_0.sv") continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Check: class a declares exactly 'rand int x'.
TEST_F(SetRandstate0Test, ClassAHasRandIntX) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", cls->getVariables());
  ASSERT_NE(x, nullptr) << "member 'x' not found";
  EXPECT_EQ(x->getRandType(), vpiRand) << "x is declared 'rand'";
  EXPECT_EQ(typespecKind(x->getTypespec()), hldb::AnyType::IntTypespec) << "x is 'int'";
}

// Check: env extends uvm_env and declares its members with the right types.
TEST_F(SetRandstate0Test, EnvExtendsUvmEnvAndDeclaresMembers) {
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

  const hldb::Variable *const obj = hldb::findByName<hldb::Variable>("obj", env->getVariables());
  ASSERT_NE(obj, nullptr) << "member 'obj' not found";
  EXPECT_EQ(obj->getRandType(), vpiNotRand) << "obj is not declared rand";
  EXPECT_EQ(classOf(obj), getClass("a")) << "obj is a handle of class a";
  const hldb::Variable *const randstate = hldb::findByName<hldb::Variable>("randstate", env->getVariables());
  ASSERT_NE(randstate, nullptr) << "member 'randstate' not found";
  EXPECT_EQ(randstate->getRandType(), vpiNotRand) << "randstate is not declared rand";
  EXPECT_EQ(typespecKind(randstate->getTypespec()), hldb::AnyType::StringTypespec)
      << "randstate is a string (get_randstate() returns string, 18.13.4)";
  const hldb::Variable *const prev_x = hldb::findByName<hldb::Variable>("prev_x", env->getVariables());
  ASSERT_NE(prev_x, nullptr) << "member 'prev_x' not found";
  EXPECT_EQ(prev_x->getRandType(), vpiNotRand) << "prev_x is not declared rand";
  EXPECT_EQ(typespecKind(prev_x->getTypespec()), hldb::AnyType::IntTypespec) << "prev_x is 'int'";
  ASSERT_NE(prev_x->getTypespec(), nullptr);
  const hldb::IntTypespec *const prev_xTs = prev_x->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(prev_xTs, nullptr);
  EXPECT_TRUE(prev_xTs->getSigned()) << "prev_x is a plain 'int', signed by default (6.11)";
}

// Check: get_randstate, randomize, set_randstate, randomize -- in that order, all on obj.
TEST_F(SetRandstate0Test, RunPhaseSavesRandomizesRestoresAndRandomizesAgain) {
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr) << "env::run_phase not found";
  const std::vector<CallSite> calls =
      filterCalls(collectCalls(runPhase->getStmt()), {"get_randstate", "randomize", "set_randstate"});
  ASSERT_EQ(calls.size(), 4u) << "run_phase has get_randstate, randomize, set_randstate, randomize";
  EXPECT_EQ(calls[0].m_call->getName(), "get_randstate");
  EXPECT_EQ(calls[1].m_call->getName(), "randomize");
  EXPECT_EQ(calls[2].m_call->getName(), "set_randstate");
  EXPECT_EQ(calls[3].m_call->getName(), "randomize");
  for (const CallSite &site : calls) {
    EXPECT_EQ(pathElemName(site.m_owner, 0), "obj") << site.m_call->getName() << " is applied to obj";
  }
  EXPECT_EQ(argumentCount(calls[0].m_call), 0u) << "get_randstate() takes no argument (18.13.4)";
  ASSERT_EQ(argumentCount(calls[2].m_call), 1u) << "set_randstate(string state) takes one argument (18.13.5)";
  const hldb::RefObj *const state = any_cast<hldb::RefObj>(argumentExpr(calls[2].m_call, 0));
  ASSERT_NE(state, nullptr) << "set_randstate is passed the saved state 'randstate'";
  EXPECT_EQ(state->getName(), "randstate");
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(state->getActual(),
            static_cast<const hldb::Any *>(hldb::findByName<hldb::Variable>("randstate", env->getVariables())))
      << "'randstate' binds to env::randstate";
  for (size_t index : {1u, 3u}) {
    const hldb::MethodFuncCall *const randomize = any_cast<hldb::MethodFuncCall>(calls[index].m_call);
    ASSERT_NE(randomize, nullptr);
    EXPECT_EQ(randomize->getWith(), nullptr) << "randomize() has no inline 'with' block";
  }
}

// Check: get_randstate/set_randstate bind to the built-in class methods (18.13).
TEST_F(SetRandstate0Test, RandstateMethodsBindToBuiltinMethods) {
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr);
  const std::vector<CallSite> calls =
      filterCalls(collectCalls(runPhase->getStmt()), {"get_randstate", "set_randstate"});
  ASSERT_EQ(calls.size(), 2u);
  for (const CallSite &site : calls) {
    ASSERT_NE(site.m_call->getTaskFunc(), nullptr)
        << site.m_call->getName() << " should resolve to the built-in method";
    EXPECT_EQ(site.m_call->getTaskFunc()->getName(), site.m_call->getName());
  }
}

// Check: 'randstate = obj.get_randstate();' and 'prev_x = obj.x;'.
TEST_F(SetRandstate0Test, RunPhaseAssignsRandstateAndPrevX) {
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr);
  const std::vector<const hldb::Assignment *> assigns = collectStmts<hldb::Assignment>(runPhase->getStmt());

  const hldb::Assignment *const saveState = findAssignmentTo(assigns, "randstate");
  ASSERT_NE(saveState, nullptr) << "'randstate = obj.get_randstate();' not found";
  const std::vector<CallSite> rhsCalls = filterCalls(collectCalls(saveState->getRhs()), {"get_randstate"});
  EXPECT_EQ(rhsCalls.size(), 1u) << "randstate is assigned the result of obj.get_randstate()";

  const hldb::Assignment *const savePrev = findAssignmentTo(assigns, "prev_x");
  ASSERT_NE(savePrev, nullptr) << "'prev_x = obj.x;' not found";
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(savePrev->getRhs());
  ASSERT_NE(rhs, nullptr) << "prev_x is assigned the member reference obj.x";
  EXPECT_EQ(pathElemName(rhs, 0), "obj");
  EXPECT_EQ(pathElemName(rhs, 1), "x");
}

// Check: module top declares 'env environment'.
TEST_F(SetRandstate0Test, TopDeclaresEnvironmentHandle) {
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
TEST_F(SetRandstate0Test, RuntimeSecondRandomizeReproducesFirstValue) {
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "Design is not elaborated, so no run-time value is available. "
                    "IEEE 1800-2023 Sec 18.13.5: set_randstate restores the saved RNG state, so the second "
                    "randomize() gives obj.x == prev_x.";
  }
  const hldb::ClassDefn *const cls = getClass("a");
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(env, nullptr);
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", cls->getVariables());
  ASSERT_NE(x, nullptr);
  ASSERT_NE(x->getValue(), nullptr) << "x should hold its value after run_phase executed";
  const hldb::Constant *const xValue = x->getValue<hldb::Constant>();
  ASSERT_NE(xValue, nullptr);
  const hldb::Variable *const prevX = hldb::findByName<hldb::Variable>("prev_x", env->getVariables());
  ASSERT_NE(prevX, nullptr);
  ASSERT_NE(prevX->getValue(), nullptr) << "prev_x should hold its value after run_phase executed";
  const hldb::Constant *const prevXValue = prevX->getValue<hldb::Constant>();
  ASSERT_NE(prevXValue, nullptr);
  EXPECT_EQ(xValue->getDecompile(), prevXValue->getDecompile());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
