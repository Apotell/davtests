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
// SV source under test: tests/Google/chapter-18/18.5.12--functions-in-constraint_1.sv
// (IEEE 1800-2017 18.5.12 == IEEE 1800-2023 18.5.11 "Functions in
// constraints", which is the text used here)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b1, b2;
//       function int F (input int d);
//           F=d;
//       endfunction
//
//       constraint c1 { b1 == 5; }
//       constraint c2 { b2 == F(b1); }
//   endclass
//
//   class env extends uvm_env;
//
//     a obj = new;
//
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj.randomize();
//         if(obj.b2 == 5) begin
//           `uvm_info("RESULT", $sformatf("obj.b2 = %0d SUCCESS", obj.b2), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("obj.b2 = %0d FAILED", obj.b2));
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
//   2. Class 'a': b1, b2 are both 'rand int' (one declaration list).
//   3. Method F: automatic (8.6 + 18.5.11), returns int, exactly one
//      argument 'd' that is input int (18.5.11: no output/inout/ref).
//   4. Constraints c1, c2 in syntactic order (37.34 detail 4), parent 'a',
//      non-static (vpiAutomatic == 1), active, in-body.
//   5. c1 is hard "b1 == 5".
//   6. c2 is hard "b2 == F(b1)": b2 bound, call named F bound to method F,
//      single argument bound to b1.
//   7. Class 'env' extends 'uvm_env'; member 'obj' is a non-rand handle of
//      class 'a'.
//   8. run_phase calls obj.randomize() exactly once, without 'with'
//      (37.42: vpiWith null).
//   9. Module 'top' declares 'environment' of class 'env'.
//  10. RUN TIME: 18.5.11 says function arguments create an implicit
//      ordering, so b1 is solved first (b1 == 5), then F(b1) is evaluated
//      as a state value (5), so b2 == 5.
//
// CHECKED (TEST_F below):
//   - items 1 to 9.
//   - item 10 (run time) runs only on an elaborated design
//     (m_design->getElaborated()); otherwise it is skipped.
//
// NOT CHECKED (and why):
//   - The UVM reporting macros and the SUCCESS/FAILED if statement: UVM
//     plumbing, not part of 18.5.11.
//   - F's body "F = d" is checked in functions-in-constraint_0 (same class
//     text); here only the 18.5.11 signature restrictions are re-checked.
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
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/function.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/tf_call.h>
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

// The caller-side expression of a call argument.
const hldb::Any *argumentExpr(const hldb::NamedArgument *arg) {
  if (arg->getHighConn() != nullptr) return arg->getHighConn();
  return arg->getLowConn();
}

const hldb::ClassDefn *classOf(const hldb::Variable *var) {
  if ((var == nullptr) || (var->getTypespec() == nullptr)) return nullptr;
  const hldb::ClassTypespec *const ts = var->getTypespec()->getActual<hldb::ClassTypespec>();
  return (ts == nullptr) ? nullptr : ts->getClassDefn();
}

hldb::AnyType typespecKind(const hldb::RefTypespec *ref) {
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
class FunctionsInConstraint1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.12--functions-in-constraint_1.hlc"}); }
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
TEST_F(FunctionsInConstraint1Test, CompilerReportsNoErrors) {
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
      if (std::filesystem::path(path).filename() != "18.5.12--functions-in-constraint_1.sv") continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Check 2: b1 and b2 are rand int.
TEST_F(FunctionsInConstraint1Test, B1AndB2AreRandInt) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 2u);
  for (std::string_view name : {"b1", "b2"}) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(name, cls->getVariables());
    ASSERT_NE(var, nullptr) << "variable '" << name << "' not found";
    EXPECT_EQ(var->getRandType(), vpiRand) << name;
    EXPECT_EQ(typespecKind(var->getTypespec()), hldb::AnyType::IntTypespec) << name;
  }
}

// Check 3: F is an automatic int function with one input int argument.
TEST_F(FunctionsInConstraint1Test, FunctionFIsAutomaticWithOneInputArgument) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Function *const f = hldb::findByName<hldb::Function>("F", cls->getMethods());
  ASSERT_NE(f, nullptr) << "function 'F' not found";
  EXPECT_TRUE(f->getAutomatic()) << "class methods are automatic (8.6), required by 18.5.11";
  EXPECT_EQ(typespecKind(f->getReturn()), hldb::AnyType::IntTypespec);
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u);
  EXPECT_EQ(f->getIODecls()->at(0)->getName(), "d");
  EXPECT_EQ(f->getIODecls()->at(0)->getDirection(), vpiInput) << "18.5.11: no output, inout or ref arguments";
  EXPECT_EQ(typespecKind(f->getIODecls()->at(0)->getTypespec()), hldb::AnyType::IntTypespec);
}

// Check 4: constraint order and properties.
TEST_F(FunctionsInConstraint1Test, ConstraintsC1AndC2InDeclarationOrder) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 2u);
  EXPECT_EQ(cls->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(cls->getConstraints()->at(1)->getName(), "c2");
  for (const hldb::Constraint *const c : *cls->getConstraints()) {
    EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(cls)) << c->getName();
    EXPECT_TRUE(c->getAutomatic()) << c->getName() << ": not static, so vpiAutomatic is 1 (37.34 detail 1)";
    EXPECT_TRUE(c->getIsConstraintEnabled()) << c->getName();
    EXPECT_EQ(c->getAccessType(), 0) << c->getName();
  }
}

// Check 5: c1 is "b1 == 5".
TEST_F(FunctionsInConstraint1Test, C1IsB1EqualsFive) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", cls->getConstraints());
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 1u);
  const hldb::Operation *const eq = any_cast<hldb::Operation>(c1->getConstraintItems()->at(0));
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

// Check 6: c2 is "b2 == F(b1)".
TEST_F(FunctionsInConstraint1Test, C2IsB2EqualsCallOfFOnB1) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  const hldb::Variable *const b2 = hldb::findByName<hldb::Variable>("b2", cls->getVariables());
  const hldb::Function *const f = hldb::findByName<hldb::Function>("F", cls->getMethods());
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(b2, nullptr);
  ASSERT_NE(f, nullptr);
  const hldb::Constraint *const c2 = hldb::findByName<hldb::Constraint>("c2", cls->getConstraints());
  ASSERT_NE(c2, nullptr);
  ASSERT_NE(c2->getConstraintItems(), nullptr);
  ASSERT_EQ(c2->getConstraintItems()->size(), 1u);
  const hldb::Operation *const eq = any_cast<hldb::Operation>(c2->getConstraintItems()->at(0));
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  EXPECT_FALSE(eq->getSoft());
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  expectRefTo(eq->getOperands()->at(0), b2);
  const hldb::TFCall *const call = any_cast<hldb::TFCall>(eq->getOperands()->at(1));
  ASSERT_NE(call, nullptr) << "right operand is the call F(b1)";
  EXPECT_EQ(call->getName(), "F");
  EXPECT_EQ(call->getTaskFunc(), static_cast<const hldb::TaskFunc *>(f)) << "call binds to class method F";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  expectRefTo(argumentExpr(call->getArguments()->at(0)), b1);
}

// Check 7: env extends uvm_env and holds a non-rand handle 'obj' of class 'a'.
TEST_F(FunctionsInConstraint1Test, EnvExtendsUvmEnvAndHoldsObj) {
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

  const hldb::Variable *const obj = hldb::findByName<hldb::Variable>("obj", env->getVariables());
  ASSERT_NE(obj, nullptr) << "env member 'obj' not found";
  EXPECT_EQ(classOf(obj), getClass("a")) << "obj is declared with type 'a'";
  EXPECT_EQ(obj->getRandType(), vpiNotRand);
}

// Check 8: run_phase calls obj.randomize() once without 'with'.
TEST_F(FunctionsInConstraint1Test, RunPhaseRandomizesObj) {
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr);
  const hldb::Task *const runPhase = hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr) << "env::run_phase not found";
  std::vector<CallSite> calls;
  collectMethodCalls(runPhase->getStmt(), nullptr, "randomize", &calls);
  ASSERT_EQ(calls.size(), 1u) << "run_phase contains exactly one randomize() call";
  EXPECT_EQ(pathElemName(calls[0].m_owner, 0), "obj");
  EXPECT_EQ(calls[0].m_call->getWith(), nullptr) << "no inline 'with' block";
}

// Check 9: module top declares 'env environment'.
TEST_F(FunctionsInConstraint1Test, TopDeclaresEnvironmentHandle) {
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

// Check 10 (run time): b2 == F(b1) == 5.
TEST_F(FunctionsInConstraint1Test, RuntimeB2RandomizesToFive) {
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "Design is not elaborated, so no post-randomize value is available. "
                    "IEEE 1800-2023 Sec 18.5.11: b1 solved first (== 5), then b2 == F(5) == 5.";
  }
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b2 = hldb::findByName<hldb::Variable>("b2", cls->getVariables());
  ASSERT_NE(b2, nullptr);
  ASSERT_NE(b2->getValue(), nullptr) << "b2 should hold its randomized value";
  const hldb::Constant *const value = b2->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "5");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
