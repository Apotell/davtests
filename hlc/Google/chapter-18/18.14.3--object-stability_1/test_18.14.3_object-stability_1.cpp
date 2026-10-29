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
// SV source under test: tests/Google/chapter-18/18.14.3--object-stability_1.sv
// (IEEE 1800-2023 18.14.3 "Object stability", 18.13.3 srandom(), 18.15)
//
//   /*
//   :name: object_stability_1
//   :description: object stability test
//   :tags: uvm-random uvm
//   */
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a1;
//       rand int x;
//       function new (int seed);
//           this.srandom(seed);
//       endfunction
//   endclass
//
//   class a2;
//       rand int y;
//       function new (int seed);
//           this.srandom(seed);
//       endfunction
//   endclass
//
//   class env extends uvm_env;
//     a1 obj1 = new(200);
//     a2 obj2 = new(200);
//     string randstate1, randstate2;
//
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         randstate1 = obj1.get_randstate();
//         randstate2 = obj2.get_randstate();
//         obj1.randomize();
//         obj2.randomize();
//
//         if(obj1.x == obj2.y && randstate1 == randstate2) begin
//           `uvm_info("RESULT", $sformatf("obj1.x = %0d obj2.y = %0d SUCCESS", obj1.x, obj2.y), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("obj1.x = %0d obj2.y = %0d FAILED", obj1.x, obj2.y));
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
//   2. Class a1 declares exactly 'rand int x'; class a2 exactly 'rand int y'.
//   3. Each class has a constructor 'function new (int seed);' with one
//      input int argument 'seed' (13.5: no direction means input), whose
//      body is the single call 'this.srandom(seed);' -- srandom applied to
//      'this' with the argument 'seed' (18.14.3: "it is best that objects
//      self-seed within their new method"). srandom binds to the built-in
//      class method (18.13.3).
//   4. env members: obj1 (non-rand handle of a1), obj2 (non-rand handle of
//      a2), randstate1, randstate2 (strings, 18.13.4). obj1 and obj2 are
//      initialized by 'new(200)' -- the SAME literal seed 200.
//   5. run_phase, in order: obj1.get_randstate(), obj2.get_randstate(),
//      obj1.randomize(), obj2.randomize() (no 'with'); and the assignments
//      randstate1 = obj1.get_randstate(), randstate2 = obj2.get_randstate().
//   6. RUN TIME (18.13.3/18.14.3): both RNGs are seeded with 200 by their
//      constructors, so their states are equal (randstate1 == randstate2)
//      and so are the randomized values (obj1.x == obj2.y).
//  ENV/TOP (every UVM file):
//   - Class 'env' extends the UVM base class 'uvm_env' (8.13).
//   - Module 'top' is a top-level instance (23.3.1) and declares
//     'environment' as a handle of class 'env'.
//
// CHECKED (TEST_F below):
//   - items 1 to 5 and ENV/TOP.
//   - item 6 (run time) runs only on an elaborated design
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

// Declaration initializer of a variable ("int v = <expr>;"), or nullptr.
const hldb::Any *initializerOf(const hldb::Variable *var) {
  if (var->getExpr() != nullptr) return var->getExpr();
  return var->getValue();
}
}  // namespace

namespace hlc {
class ObjectStability1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.14.3--object-stability_1.hlc"}); }
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
TEST_F(ObjectStability1Test, CompilerReportsNoErrors) {
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
      if (std::filesystem::path(path).filename() != "18.14.3--object-stability_1.sv") continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Check: class a1 has 'rand int x' and a constructor new(int seed) that calls this.srandom(seed).
TEST_F(ObjectStability1Test, ClassA1ConstructorSeedsItsOwnRng) {
  const hldb::ClassDefn *const cls = getClass("a1");
  ASSERT_NE(cls, nullptr) << "class 'a1' not found";
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
TEST_F(ObjectStability1Test, ClassA1SrandomBindsToBuiltinMethod) {
  const hldb::ClassDefn *const cls = getClass("a1");
  ASSERT_NE(cls, nullptr);
  const hldb::Function *const ctor = hldb::findByName<hldb::Function>("new", cls->getMethods());
  ASSERT_NE(ctor, nullptr);
  const std::vector<CallSite> calls = filterCalls(collectCalls(ctor->getStmt()), {"srandom"});
  ASSERT_EQ(calls.size(), 1u);
  ASSERT_NE(calls[0].m_call->getTaskFunc(), nullptr) << "srandom should resolve to the built-in class method";
  EXPECT_EQ(calls[0].m_call->getTaskFunc()->getName(), "srandom");
}

// Check: class a2 has 'rand int y' and a constructor new(int seed) that calls this.srandom(seed).
TEST_F(ObjectStability1Test, ClassA2ConstructorSeedsItsOwnRng) {
  const hldb::ClassDefn *const cls = getClass("a2");
  ASSERT_NE(cls, nullptr) << "class 'a2' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
  const hldb::Variable *const y = hldb::findByName<hldb::Variable>("y", cls->getVariables());
  ASSERT_NE(y, nullptr) << "member 'y' not found";
  EXPECT_EQ(y->getRandType(), vpiRand) << "y is declared 'rand'";
  EXPECT_EQ(typespecKind(y->getTypespec()), hldb::AnyType::IntTypespec) << "y is 'int'";

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
TEST_F(ObjectStability1Test, ClassA2SrandomBindsToBuiltinMethod) {
  const hldb::ClassDefn *const cls = getClass("a2");
  ASSERT_NE(cls, nullptr);
  const hldb::Function *const ctor = hldb::findByName<hldb::Function>("new", cls->getMethods());
  ASSERT_NE(ctor, nullptr);
  const std::vector<CallSite> calls = filterCalls(collectCalls(ctor->getStmt()), {"srandom"});
  ASSERT_EQ(calls.size(), 1u);
  ASSERT_NE(calls[0].m_call->getTaskFunc(), nullptr) << "srandom should resolve to the built-in class method";
  EXPECT_EQ(calls[0].m_call->getTaskFunc()->getName(), "srandom");
}

// Check: env extends uvm_env and declares its members with the right types.
TEST_F(ObjectStability1Test, EnvExtendsUvmEnvAndDeclaresMembers) {
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

  const hldb::Variable *const obj1 = hldb::findByName<hldb::Variable>("obj1", env->getVariables());
  ASSERT_NE(obj1, nullptr) << "member 'obj1' not found";
  EXPECT_EQ(obj1->getRandType(), vpiNotRand) << "obj1 is not declared rand";
  EXPECT_EQ(classOf(obj1), getClass("a1")) << "obj1 is a handle of class a1";
  const hldb::Variable *const obj2 = hldb::findByName<hldb::Variable>("obj2", env->getVariables());
  ASSERT_NE(obj2, nullptr) << "member 'obj2' not found";
  EXPECT_EQ(obj2->getRandType(), vpiNotRand) << "obj2 is not declared rand";
  EXPECT_EQ(classOf(obj2), getClass("a2")) << "obj2 is a handle of class a2";
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

// Check: each handle is created with an explicit seed, "<handle> = new(<seed>)".
TEST_F(ObjectStability1Test, HandlesAreConstructedWithSeed) {
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr);
  {
    const hldb::Variable *const handle = hldb::findByName<hldb::Variable>("obj1", env->getVariables());
    ASSERT_NE(handle, nullptr) << "member 'obj1' not found";
    ASSERT_NE(initializerOf(handle), nullptr) << "obj1 has the initializer 'new(200)'";
    const std::vector<CallSite> calls = filterCalls(collectCalls(initializerOf(handle)), {"new"});
    ASSERT_EQ(calls.size(), 1u) << "obj1 is initialized by one call to new";
    ASSERT_EQ(argumentCount(calls[0].m_call), 1u);
    const hldb::Constant *const seed = any_cast<hldb::Constant>(argumentExpr(calls[0].m_call, 0));
    ASSERT_NE(seed, nullptr) << "new is called with the literal seed 200";
    EXPECT_EQ(seed->getDecompile(), "200");
  }
  {
    const hldb::Variable *const handle = hldb::findByName<hldb::Variable>("obj2", env->getVariables());
    ASSERT_NE(handle, nullptr) << "member 'obj2' not found";
    ASSERT_NE(initializerOf(handle), nullptr) << "obj2 has the initializer 'new(200)'";
    const std::vector<CallSite> calls = filterCalls(collectCalls(initializerOf(handle)), {"new"});
    ASSERT_EQ(calls.size(), 1u) << "obj2 is initialized by one call to new";
    ASSERT_EQ(argumentCount(calls[0].m_call), 1u);
    const hldb::Constant *const seed = any_cast<hldb::Constant>(argumentExpr(calls[0].m_call, 0));
    ASSERT_NE(seed, nullptr) << "new is called with the literal seed 200";
    EXPECT_EQ(seed->getDecompile(), "200");
  }
}

// Check: run_phase saves both RNG states, then randomizes obj1 and obj2 -- in that order.
TEST_F(ObjectStability1Test, RunPhaseSavesStatesThenRandomizesBothObjects) {
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr) << "env::run_phase not found";
  const std::vector<CallSite> calls = filterCalls(collectCalls(runPhase->getStmt()), {"get_randstate", "randomize"});
  ASSERT_EQ(calls.size(), 4u);
  const char *const names[] = {"get_randstate", "get_randstate", "randomize", "randomize"};
  const char *const owners[] = {"obj1", "obj2", "obj1", "obj2"};
  for (size_t i = 0; i < calls.size(); ++i) {
    EXPECT_EQ(calls[i].m_call->getName(), names[i]) << "call #" << i;
    EXPECT_EQ(pathElemName(calls[i].m_owner, 0), owners[i]) << "call #" << i << " is applied to " << owners[i];
  }
  for (size_t index : {2u, 3u}) {
    const hldb::MethodFuncCall *const randomize = any_cast<hldb::MethodFuncCall>(calls[index].m_call);
    ASSERT_NE(randomize, nullptr);
    EXPECT_EQ(randomize->getWith(), nullptr) << "randomize() has no inline 'with' block";
  }

  const std::vector<const hldb::Assignment *> assigns = collectStmts<hldb::Assignment>(runPhase->getStmt());
  const char *const states[] = {"randstate1", "randstate2"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Assignment *const save = findAssignmentTo(assigns, states[i]);
    ASSERT_NE(save, nullptr) << "'" << states[i] << " = " << owners[i] << ".get_randstate();' not found";
    const std::vector<CallSite> rhsCalls = filterCalls(collectCalls(save->getRhs()), {"get_randstate"});
    ASSERT_EQ(rhsCalls.size(), 1u);
    EXPECT_EQ(pathElemName(rhsCalls[0].m_owner, 0), owners[i]);
  }
}

// Check: module top declares 'env environment'.
TEST_F(ObjectStability1Test, TopDeclaresEnvironmentHandle) {
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
TEST_F(ObjectStability1Test, RuntimeEqualSeedsGiveEqualStatesAndValues) {
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "Design is not elaborated, so no run-time value is available. "
                    "IEEE 1800-2023 Sec 18.13.3/18.14.3: both objects self-seed with 200, so randstate1 == randstate2 "
                    "and obj1.x == obj2.y.";
  }
  const hldb::ClassDefn *const a1 = getClass("a1");
  const hldb::ClassDefn *const a2 = getClass("a2");
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(a1, nullptr);
  ASSERT_NE(a2, nullptr);
  ASSERT_NE(env, nullptr);
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", a1->getVariables());
  ASSERT_NE(x, nullptr);
  ASSERT_NE(x->getValue(), nullptr) << "x should hold its value after run_phase executed";
  const hldb::Constant *const xValue = x->getValue<hldb::Constant>();
  ASSERT_NE(xValue, nullptr);
  const hldb::Variable *const y = hldb::findByName<hldb::Variable>("y", a2->getVariables());
  ASSERT_NE(y, nullptr);
  ASSERT_NE(y->getValue(), nullptr) << "y should hold its value after run_phase executed";
  const hldb::Constant *const yValue = y->getValue<hldb::Constant>();
  ASSERT_NE(yValue, nullptr);
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
  EXPECT_EQ(xValue->getDecompile(), yValue->getDecompile());
  EXPECT_EQ(state1Value->getDecompile(), state2Value->getDecompile());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
