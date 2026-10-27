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
// SV source under test: tests/Google/chapter-18/18.5.13--constraint-guards_1.sv
// (IEEE 1800-2017 18.5.13 == IEEE 1800-2023 18.5.12 "Constraint guards",
// which is the text used here)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class b;
//       int d1;
//   endclass
//
//   class a;
//       rand int b1;
//       b next;
//
//       constraint c1 { if (next == null) b1 == 5; }
//   endclass
//
//   class env extends uvm_env;
//
//     a obj1 = new;
//
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj1.randomize();
//         if(obj1.b1 == 5) begin
//           `uvm_info("RESULT", $sformatf("obj1.b1 = %0d SUCCESS", obj1.b1), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("obj1.b1 = %0d FAILED", obj1.b1));
//         end
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//
//   endclass
//
//   module top;
//     env environment;
//     initial begin
//       environment = new("env");
//       run_test();
//     end
//   endmodule
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error.
//   2. Class 'b': one plain (non-rand) 'int d1'.
//   3. Class 'a': 'rand int b1' (vpiRand) and the handle 'b next' bound to
//      class 'b' and NOT rand (a state variable, as 18.5.12 requires for
//      guard expressions).
//   4. Class 'a' has exactly one constraint 'c1': parent 'a', non-static
//      (vpiAutomatic == 1), active, in-body.
//   5. c1 is a "constr if" (no else, 37.38) whose condition is the handle
//      comparison "next == null" (vpiEqOp, next bound, vpiNullConst) and
//      whose single guarded constraint is hard "b1 == 5".
//   6. Class 'env' extends 'uvm_env'; member 'obj1' is a non-rand handle of
//      class 'a'.
//   7. run_phase calls obj1.randomize() exactly once, without 'with'.
//   8. Module 'top' declares 'environment' of class 'env'.
//   9. RUN TIME: obj1.next is never assigned, so it stays null; the guard
//      evaluates TRUE and the constraint b1 == 5 is generated (18.5.12
//      "If the result is TRUE, then an unconditional constraint is
//      generated"), so after randomize() obj1.b1 == 5.
//
// CHECKED (TEST_F below):
//   - items 1 to 8.
//   - item 9 (run time) runs only on an elaborated design
//     (m_design->getElaborated()); otherwise it is skipped.
//
// NOT CHECKED (and why):
//   - The UVM reporting macros and the SUCCESS/FAILED if statement: UVM
//     plumbing, not part of 18.5.12.
// ---------------------------------------------------------------------------

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constr_if.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <filesystem>
#include <string_view>
#include <vector>

namespace {
struct CallSite {
  const hldb::MethodFuncCall *m_call = nullptr;
  // Hierarchical reference "x.method(...)" that holds the call, if any.
  const hldb::RefObj *m_owner = nullptr;
};

// Collects, in source order, every method call named 'name' reachable from a statement.
void collectMethodCalls(const hldb::Any *node, const hldb::RefObj *owner, std::string_view name,
                        std::vector<CallSite> *out) {
  if (node == nullptr) return;
  if (const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(node)) {
    if (call->getName() == name) out->emplace_back(CallSite{call, owner});
  } else if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(node)) {
    if (const hldb::AnyCollection *const elems = ref->getPathElems()) {
      for (const hldb::Any *const elem : *elems) collectMethodCalls(elem, ref, name, out);
    }
  } else if (const hldb::Begin *const begin = any_cast<hldb::Begin>(node)) {
    if (const hldb::AnyCollection *const stmts = begin->getStmts()) {
      for (const hldb::Any *const stmt : *stmts) collectMethodCalls(stmt, nullptr, name, out);
    }
  } else if (const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(node)) {
    collectMethodCalls(ifElse->getStmt(), nullptr, name, out);
    collectMethodCalls(ifElse->getElseStmt(), nullptr, name, out);
  } else if (const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(node)) {
    collectMethodCalls(ifStmt->getStmt(), nullptr, name, out);
  }
}

// Name of the i-th element of a hierarchical reference, or "" when absent.
std::string_view pathElemName(const hldb::RefObj *owner, size_t index) {
  if ((owner == nullptr) || (owner->getPathElems() == nullptr) || (owner->getPathElems()->size() <= index)) return "";
  return owner->getPathElems()->at(index)->getName();
}

const hldb::ClassDefn *classOf(const hldb::Variable *var) {
  if ((var == nullptr) || (var->getTypespec() == nullptr)) return nullptr;
  const hldb::ClassTypespec *const ts = var->getTypespec()->getActual<hldb::ClassTypespec>();
  return (ts == nullptr) ? nullptr : ts->getClassDefn();
}

hldb::AnyType typespecKind(const hldb::Variable *var) {
  const hldb::RefTypespec *const ref = var->getTypespec();
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}

// Checks that 'expr' is a reference named like 'var' that binds to 'var'.
void expectRefTo(const hldb::Any *expr, const hldb::Variable *var) {
  ASSERT_NE(expr, nullptr);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
  ASSERT_NE(ref, nullptr) << "operand should be a reference to '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "reference '" << ref->getName() << "' should bind to the class variable";
}
}  // namespace

namespace hlc {
class ConstraintGuards1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.13--constraint-guards_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClass(std::string_view name) const {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }
};

// Check 1: legal file. Fatal and syntax diagnostics are checked for the whole
// compile. Error diagnostics are checked only where they point into this file:
// HLC reports errors inside the UVM 1800.2-2017 library itself (the same set as
// BuildUVMPkg), and those are not about this source.
TEST_F(ConstraintGuards1Test, CompilerReportsNoErrors) {
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
      if (std::filesystem::path(path).filename() != "18.5.13--constraint-guards_1.sv") continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Check 2: class 'b' has a plain 'int d1'.
TEST_F(ConstraintGuards1Test, ClassBHasPlainIntD1) {
  const hldb::ClassDefn *const clsB = getClass("b");
  ASSERT_NE(clsB, nullptr) << "class 'b' not found";
  const hldb::Variable *const d1 = hldb::findByName<hldb::Variable>("d1", clsB->getVariables());
  ASSERT_NE(d1, nullptr) << "variable 'd1' not found";
  EXPECT_EQ(d1->getRandType(), vpiNotRand);
  EXPECT_EQ(typespecKind(d1), hldb::AnyType::IntTypespec);
}

// Check 3: class 'a' has 'rand int b1' and the state handle 'b next'.
TEST_F(ConstraintGuards1Test, ClassAHasRandB1AndStateHandleNext) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 2u);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  EXPECT_EQ(b1->getRandType(), vpiRand);
  EXPECT_EQ(typespecKind(b1), hldb::AnyType::IntTypespec);
  const hldb::Variable *const next = hldb::findByName<hldb::Variable>("next", cls->getVariables());
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->getRandType(), vpiNotRand) << "next is a state variable (18.5.12)";
  EXPECT_EQ(classOf(next), getClass("b")) << "next is a handle of class 'b'";
}

// Check 4: one constraint c1.
TEST_F(ConstraintGuards1Test, ClassAHasOneConstraintC1) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 1u);
  const hldb::Constraint *const c1 = cls->getConstraints()->at(0);
  EXPECT_EQ(c1->getName(), "c1");
  EXPECT_EQ(c1->getParent(), static_cast<const hldb::Any *>(cls));
  EXPECT_TRUE(c1->getAutomatic()) << "not static, so vpiAutomatic is 1 (37.34 detail 1)";
  EXPECT_TRUE(c1->getIsConstraintEnabled());
  EXPECT_EQ(c1->getAccessType(), 0);
}

// Check 5: c1 is "if (next == null) b1 == 5;".
TEST_F(ConstraintGuards1Test, C1IsIfNextIsNullThenB1EqualsFive) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  const hldb::Variable *const next = hldb::findByName<hldb::Variable>("next", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(next, nullptr);
  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", cls->getConstraints());
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 1u);
  const hldb::Any *const item = c1->getConstraintItems()->at(0);
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getAnyType(), hldb::AnyType::ConstrIf) << "'if' without 'else' is a constr if (37.38)";
  const hldb::ConstrIf *const guard = any_cast<hldb::ConstrIf>(item);
  ASSERT_NE(guard, nullptr);

  const hldb::Operation *const cond = any_cast<hldb::Operation>(guard->getCondition());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  expectRefTo(cond->getOperands()->at(0), next);
  const hldb::Constant *const nullLiteral = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(nullLiteral, nullptr);
  EXPECT_EQ(nullLiteral->getConstType(), vpiNullConst);

  ASSERT_NE(guard->getConstraintExprs(), nullptr);
  ASSERT_EQ(guard->getConstraintExprs()->size(), 1u);
  const hldb::Operation *const eq = any_cast<hldb::Operation>(guard->getConstraintExprs()->at(0));
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  EXPECT_FALSE(eq->getSoft());
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  expectRefTo(eq->getOperands()->at(0), b1);
  const hldb::Constant *const five = any_cast<hldb::Constant>(eq->getOperands()->at(1));
  ASSERT_NE(five, nullptr);
  EXPECT_EQ(five->getDecompile(), "5");
}

// Check 6: env extends uvm_env and holds a non-rand handle 'obj1' of class 'a'.
TEST_F(ConstraintGuards1Test, EnvExtendsUvmEnvAndHoldsObj1) {
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr) << "class 'env' not found";
  ASSERT_NE(env->getExtends(), nullptr);
  ASSERT_NE(env->getExtends()->getClassTypespecs(), nullptr);
  ASSERT_EQ(env->getExtends()->getClassTypespecs()->size(), 1u);
  const hldb::ClassTypespec *const base =
      env->getExtends()->getClassTypespecs()->at(0)->getActual<hldb::ClassTypespec>();
  ASSERT_NE(base, nullptr);
  ASSERT_NE(base->getClassDefn(), nullptr);
  EXPECT_EQ(base->getClassDefn()->getName(), "uvm_env");
  const hldb::Variable *const obj1 = hldb::findByName<hldb::Variable>("obj1", env->getVariables());
  ASSERT_NE(obj1, nullptr) << "env member 'obj1' not found";
  EXPECT_EQ(classOf(obj1), getClass("a"));
  EXPECT_EQ(obj1->getRandType(), vpiNotRand);
}

// Check 7: run_phase calls obj1.randomize() once without 'with'.
TEST_F(ConstraintGuards1Test, RunPhaseRandomizesObj1) {
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr);
  const hldb::Task *const runPhase = hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr) << "env::run_phase not found";
  std::vector<CallSite> calls;
  collectMethodCalls(runPhase->getStmt(), nullptr, "randomize", &calls);
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(pathElemName(calls[0].m_owner, 0), "obj1");
  EXPECT_EQ(calls[0].m_call->getWith(), nullptr);
}

// Check 8: module top declares 'env environment'.
TEST_F(ConstraintGuards1Test, TopDeclaresEnvironmentHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  if (m_design->getElaborated()) {
    // IEEE 1800-2023 Sec 23.3.1: a top-level module is implicitly instantiated once, under its own name.
    EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getTopModules()), nullptr)
        << "top-level instance 'top' not found";
  }
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr);
  EXPECT_EQ(classOf(environment), getClass("env"));
}

// Check 9 (run time): next is null, so the guard is TRUE and b1 == 5.
TEST_F(ConstraintGuards1Test, RuntimeB1RandomizesToFive) {
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "Design is not elaborated, so no post-randomize value is available. "
                    "IEEE 1800-2023 Sec 18.5.12: next == null is TRUE, constraint b1 == 5 is generated.";
  }
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(b1->getValue(), nullptr) << "b1 should hold its randomized value";
  const hldb::Constant *const value = b1->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "5");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
