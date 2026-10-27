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
// SV source under test: tests/Google/chapter-18/18.15--manually-seeding-randomize_0.sv
// (IEEE 1800-2023 18.15 "Manually seeding randomize", 18.13.3 srandom())
//
//   /*
//   :name: manually_seeding_randomize_0
//   :description: manually seeding randomize test
//   :tags: 18.15
//   */
//
//   class a;
//       rand int x;
//       function new (int seed);
//           this.srandom(seed);
//       endfunction
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error.
//   2. Class 'a' declares exactly 'rand int x' (vpiRand, int).
//   3. Class 'a' has exactly one method, the constructor 'new', with one
//      argument 'seed' of type int and direction input (13.5: an argument
//      with no direction is an input).
//   4. The constructor body is the single call 'this.srandom(seed);':
//      srandom applied to 'this' with exactly one argument, the reference
//      'seed' (18.15: "seeding the RNG internally, as a class method";
//      18.13.3 prototype "function void srandom(int seed);").
//   5. srandom binds to the built-in class method (18.13.3).
//   6. Class 'a' declares no constraints.
//
// CHECKED (TEST_F below):
//   - items 1 to 6.
//
// NOT CHECKED (and why):
//   - "Calling srandom() in an object's new() function assures the object's
//     RNG is set with the new seed before any class member values are
//     randomized" (18.15): run-time behavior, and this file creates no
//     object (see manually-seeding-randomize_1 for the run-time variant).
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
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

hldb::AnyType typespecKind(const hldb::RefTypespec *ref) {
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}
}  // namespace

namespace hlc {
class ManuallySeedingRandomize0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.15--manually-seeding-randomize_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClass(std::string_view name) const {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }
};

// Check: legal file.
TEST_F(ManuallySeedingRandomize0Test, CompilerReportsNoErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0) << "this.srandom(seed) in a constructor is legal (IEEE 1800-2023 Sec 18.15)";
}

// Check: class a has 'rand int x' and a constructor new(int seed) that calls this.srandom(seed).
TEST_F(ManuallySeedingRandomize0Test, ClassAConstructorSeedsItsOwnRng) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", cls->getVariables());
  ASSERT_NE(x, nullptr) << "member 'x' not found";
  EXPECT_EQ(x->getRandType(), vpiRand) << "x is declared 'rand'";
  EXPECT_EQ(typespecKind(x->getTypespec()), hldb::AnyType::IntTypespec) << "x is 'int'";

  const hldb::Function *const ctor = hldb::findByName<hldb::Function>("new", cls->getMethods());
  ASSERT_NE(ctor, nullptr) << "constructor 'new' not found";
  EXPECT_TRUE(ctor->getMethod()) << "new is a class method";
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 1u) << "new takes exactly one argument";
  const hldb::IODecl *const seed = ctor->getIODecls()->at(0);
  EXPECT_EQ(seed->getName(), "seed");
  EXPECT_EQ(seed->getDirection(), vpiInput) << "an argument with no direction is an input (13.5)";
  EXPECT_EQ(typespecKind(seed->getTypespec()), hldb::AnyType::IntTypespec) << "seed is 'int'";

  const std::vector<CallSite> calls = filterCalls(collectCalls(ctor->getStmt()), {"srandom"});
  ASSERT_EQ(calls.size(), 1u) << "new() calls srandom exactly once";
  EXPECT_EQ(pathElemName(calls[0].m_owner, 0), "this") << "srandom is called on 'this' (the object's own RNG)";
  ASSERT_EQ(argumentCount(calls[0].m_call), 1u) << "srandom(int seed) takes one argument (18.13.3)";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(argumentExpr(calls[0].m_call, 0));
  ASSERT_NE(arg, nullptr) << "srandom is passed the constructor argument 'seed'";
  EXPECT_EQ(arg->getName(), "seed");
}

// Check: this.srandom binds to the built-in srandom() method every class has (18.13.3).
TEST_F(ManuallySeedingRandomize0Test, ClassASrandomBindsToBuiltinMethod) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Function *const ctor = hldb::findByName<hldb::Function>("new", cls->getMethods());
  ASSERT_NE(ctor, nullptr);
  const std::vector<CallSite> calls = filterCalls(collectCalls(ctor->getStmt()), {"srandom"});
  ASSERT_EQ(calls.size(), 1u);
  ASSERT_NE(calls[0].m_call->getTaskFunc(), nullptr) << "srandom should resolve to the built-in class method";
  EXPECT_EQ(calls[0].m_call->getTaskFunc()->getName(), "srandom");
}

// Check: 'new' is the only method and there are no constraints.
TEST_F(ManuallySeedingRandomize0Test, ClassAHasOnlyTheConstructorAndNoConstraints) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getMethods(), nullptr);
  ASSERT_EQ(cls->getMethods()->size(), 1u) << "class 'a' declares exactly one method";
  EXPECT_EQ(cls->getMethods()->at(0)->getName(), "new");
  EXPECT_TRUE((cls->getConstraints() == nullptr) || cls->getConstraints()->empty()) << "class 'a' has no constraints";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
