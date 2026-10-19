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

// Tests for 18.5.9--global-constraints_1.sv (tags: uvm-random uvm)
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int v;
//   endclass
//
//   class b extends a;
//       rand a aObj;
//
//       constraint c { aObj.v < v; }
//   endclass
//
//   class env extends uvm_env;
//     b bObj = new;
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         bObj.randomize();
//         if(bObj.aObj.v < bObj.v) begin
//           `uvm_info("RESULT", $sformatf("%0d < %0d SUCCESS", bObj.aObj.v, bObj.v), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("%0d !< %0d FAILED", bObj.aObj.v, bObj.v));
//         end
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
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
// IEEE 1800-2023 Sec 18.5.9, "Global constraints":
// "When an object member of a class is declared rand, all of its
// constraints and random variables are randomized simultaneously along with
// the other class variables and constraints. Constraint expressions
// involving random variables from other objects are called global
// constraints." Here "rand a aObj;" makes the object member random, and c
// relates a variable of that object (aObj.v) to a variable of b (v).
// Sec 8.13: "class b extends a" makes b inherit a's member v, so b declares
// only aObj itself; the unqualified "v" in c and "aObj.v" both name the
// member v declared in class a (of this object and of aObj respectively).
//
// Expectations are derived from the .sv source, the LRM and the VPI object
// model (Clause 37) only (the golden .log was deliberately not consulted).
// Member accesses such as "aObj.v" are expected as a hierarchical reference
// whose path elements are the references in order (the same shape the
// chapter-18 tests 18.5.8.2_0/_1 and 18.5.9_0 confirm).
//
// Checked:
//   class a:
//   - class "a" (Sec 8.3, a user-defined class) has exactly 1 variable, v:
//     typed by a signed IntTypespec (Sec 6.11), randomized with rand type
//     vpiRand (Sec 18.4), and public (Sec 8.18); a has no constraints
//   class b:
//   - class "b" extends exactly 1 class, which resolves to the ClassDefn
//     "a" (Sec 8.13)
//   - class "b" declares exactly 1 variable, aObj (v is inherited, not
//     redeclared): randomized with rand type vpiRand, public, and typed by
//     a ClassTypespec that resolves to the ClassDefn "a"
//   - class "b" has exactly 1 constraint, c, holding exactly 1 constraint
//     item, "aObj.v < v": an Operation vpiLtOp with exactly 2 operands
//   - the left operand "aObj.v" has exactly 2 path elements: a RefObj
//     "aObj" bound to b's Variable aObj, and a RefObj "v" bound to a's
//     Variable v
//   - the right operand is a RefObj "v" bound to a's Variable v (inherited
//     by b)
//   class env:
//   - class "env" extends exactly 1 class, which resolves to the ClassDefn
//     "uvm_env" (Sec 8.13; this also proves "import uvm_pkg::*;" made the
//     UVM base class visible)
//   - "b bObj = new;" is a Variable "bObj" whose ClassTypespec resolves to
//     the ClassDefn "b" and whose initializer is a call named "new" with no
//     arguments (Sec 8.7)
//   - method "new" is a Function with exactly 2 io declarations, both
//     input (Sec 13.4, the default direction): "name" typed string and
//     "parent" typed by class "uvm_component" with default value "null"
//     (vpiNullConst); its body is the call "super.new(name, parent)" with
//     exactly 2 arguments "name" and "parent"
//   - method "run_phase" is a Task with end label "run_phase" and exactly 1
//     io declaration "phase", input, typed by class "uvm_phase"; its body is
//     a Begin holding exactly 3 statements:
//       0. method call "raise_objection" with exactly 1 argument
//       1. a Begin holding exactly 2 statements: method call "randomize"
//          with no arguments, and an IfElse whose condition is
//          "bObj.aObj.v < bObj.v" (an Operation vpiLtOp; left operand with
//          exactly 3 path elements "bObj" -> env's bObj, "aObj" -> b's aObj,
//          "v" -> a's v; right operand with exactly 2 path elements
//          "bObj" -> env's bObj, "v" -> a's v) and whose then and else
//          statements are each a Begin
//       2. method call "drop_objection" with exactly 1 argument
//   module top:
//   - module "top" has a Variable "environment" whose ClassTypespec
//     resolves to the ClassDefn "env", and exactly 1 process, an Initial
//   - the Initial's body is a Begin holding exactly 2 statements: a
//     blocking Assignment "environment = new("env")" (lhs bound to the
//     module Variable, rhs a call "new" with 1 argument, the string
//     Constant "env") and a TaskCall "run_test" with no arguments
//   - compiler reports zero fatal, syntax and error diagnostics (the source
//     is legal SystemVerilog)
//
// KNOWN FAILURE: on HLC 577, 9 of the tests fail:
//   - EnvExtendsUvmEnv, EnvNewHasNameAndParentInputs and
//     EnvRunPhaseHasPhaseInput: the class types uvm_env, uvm_component and
//     uvm_phase do not resolve to a ClassDefn.
//   - EnvNewCallsSuperNew, RunPhaseRaisesObjection, RunPhaseDropsObjection
//     and RunPhaseInnerBeginRandomizesBObj: "super.new(...)",
//     "phase.raise_objection(this)", "phase.drop_objection(this)" and
//     "bObj.randomize()" are not found as method calls.
//   - TopCallsRunTest: "run_test()" is not a TaskCall (the .hlc does not
//     compile 1800.2-2017-1.0/src/uvm_pkg.sv).
//   - CompilerReportsZeroErrors: HLC reports 2 errors:
//     [ERR:CP5824] line 23 ("class b extends a;"): Multiply defined typedef
//     "a" -- class a is defined only once; and
//     [ERR:DB2038] line 26 ("constraint c { aObj.v < v; }"):
//     'vpiConstraintItem' property value has invalid objects -- the HLDB
//     checker rejects the plain expression "aObj.v < v" as a constraint
//     item.
//
// NOT CHECKED:
//   - the contents of the `uvm_info / `uvm_error macro expansions -- they
//     are defined by the UVM library, not by this source.
//   - the prefixes of the method calls ("phase.", "bObj.", "super.") and
//     the "this" arguments, whose VPI shape is a compiler choice.
//   - whether run_phase is virtual (it overrides uvm_component::run_phase).
//   - runtime effects (that randomize() makes bObj.aObj.v < bObj.v and
//     run_test() reports SUCCESS) cannot be observed -- HLC is a
//     compiler/elaborator with no simulation capability, so no execution
//     ever happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/function.h>
#include <hldb/identifier.h>
#include <hldb/if_else.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/task_call.h>
#include <hldb/tf_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GlobalConstraints1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.9--global-constraints_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getClass(std::string_view name) {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }

  static const hldb::Variable *getVar(std::string_view className, std::string_view varName) {
    const hldb::ClassDefn *const cls = getClass(className);
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(varName, cls->getVariables());
  }

  static const hldb::Constraint *getConstraint() {
    const hldb::ClassDefn *const cls = getClass("b");
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Constraint>("c", cls->getConstraints());
  }

  // "aObj.v < v" -- the single item of constraint c.
  static const hldb::Operation *getConstraintLessThan() {
    const hldb::Constraint *const constraint = getConstraint();
    if (constraint == nullptr || constraint->getConstraintItems() == nullptr) {
      return nullptr;
    }
    if (constraint->getConstraintItems()->size() != 1u) {
      return nullptr;
    }
    return any_cast<hldb::Operation>(constraint->getConstraintItems()->at(0));
  }

  // "function new(...)" / "task run_phase(...)" of class env.
  template <typename T>
  static const T *getEnvMethod(std::string_view name) {
    const hldb::ClassDefn *const env = getClass("env");
    if (env == nullptr) {
      return nullptr;
    }
    return hldb::findByName<T>(name, env->getMethods());
  }

  // The body of run_phase: "phase.raise_objection(this); begin ... end; ...".
  static const hldb::Begin *getRunPhaseBody() {
    const hldb::Task *const task = getEnvMethod<hldb::Task>("run_phase");
    if (task == nullptr) {
      return nullptr;
    }
    return task->getStmt<hldb::Begin>();
  }

  // The inner "begin bObj.randomize(); if (...) ... end" of run_phase.
  static const hldb::Begin *getRunPhaseInnerBegin() {
    const hldb::Begin *const body = getRunPhaseBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::Begin>(body->getStmts()->at(1));
  }

  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  // "initial begin environment = new("env"); run_test(); end"
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    const hldb::Initial *const init = any_cast<hldb::Initial>(mod->getProcesses()->at(0));
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::Begin>();
  }

  // The single base class of the named class.
  static const hldb::ClassDefn *getBaseClass(std::string_view name) {
    const hldb::ClassDefn *const cls = getClass(name);
    if (cls == nullptr || cls->getExtends() == nullptr) {
      return nullptr;
    }
    const hldb::Extends *const extends = cls->getExtends();
    if (extends->getClassTypespecs() == nullptr || extends->getClassTypespecs()->size() != 1u) {
      return nullptr;
    }
    return hldb::getReferenced<hldb::ClassDefn>(extends->getClassTypespecs()->at(0));
  }

  // A reference "name" bound to the Variable var.
  static void expectRefTo(const hldb::Any *elem, std::string_view name, const hldb::Variable *var) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(elem);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(ref->getActual<hldb::Variable>(), var) << "'" << name << "' should bind to its Variable";
  }

  // A hierarchical reference whose path elements are the references names,
  // bound to the Variables vars.
  static void expectPath(const hldb::Any *expr, const std::vector<std::string_view> &names,
                         const std::vector<const hldb::Variable *> &vars) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "a member access should be a hierarchical reference";
    ASSERT_NE(path->getPathElems(), nullptr) << "a member access should hold its path elements";
    ASSERT_EQ(path->getPathElems()->size(), names.size());
    for (size_t i = 0; i < names.size(); ++i) {
      expectRefTo(path->getPathElems()->at(i), names[i], vars[i]);
    }
  }

  // "phase.<name>(this);" -- statement stmtIndex of the run_phase body.
  static void expectObjectionCall(size_t stmtIndex, std::string_view name) {
    const hldb::Begin *const body = getRunPhaseBody();
    ASSERT_NE(body, nullptr);
    ASSERT_NE(body->getStmts(), nullptr);
    ASSERT_GT(body->getStmts()->size(), stmtIndex);

    const hldb::RefObj *const stmt = any_cast<hldb::RefObj>(body->getStmts()->at(stmtIndex));
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->getActual(), nullptr);

    const hldb::MethodFuncCall *const call = stmt->getActual<hldb::MethodFuncCall>();
    ASSERT_NE(call, nullptr) << "'phase." << name << "(this)' should be a MethodFuncCall";
    EXPECT_EQ(call->getName(), name);
    ASSERT_NE(call->getArguments(), nullptr);
    EXPECT_EQ(call->getArguments()->size(), 1u) << "'" << name << "' is called with the single argument 'this'";
  }
};

// --- class a; rand int v; endclass -------------------------------------------

TEST_F(GlobalConstraints1Test, ClassAExists) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass);
  EXPECT_TRUE(cls->getConstraints() == nullptr || cls->getConstraints()->empty())
      << "class 'a' declares no constraint blocks";
}

TEST_F(GlobalConstraints1Test, ClassAHasOneVariable) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u) << "'rand int v;' declares one variable";
}

TEST_F(GlobalConstraints1Test, AVIsRandInt) {
  const hldb::Variable *const var = getVar("a", "v");
  ASSERT_NE(var, nullptr) << "'v' should be a Variable of class 'a'";
  ASSERT_NE(var->getTypespec(), nullptr);
  const hldb::IntTypespec *const intTs = var->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(intTs, nullptr) << "'int' should have an IntTypespec";
  EXPECT_TRUE(intTs->getSigned()) << "6.11: 'int' is a signed type";
  EXPECT_TRUE(var->getIsRandomized());
  EXPECT_EQ(var->getRandType(), vpiRand) << "18.4: 'rand' modifier";
  EXPECT_EQ(var->getVisibility(), vpiPublicVis) << "8.18: members are public by default";
}

// --- class b extends a; rand a aObj; -----------------------------------------

TEST_F(GlobalConstraints1Test, ClassBExtendsA) {
  const hldb::ClassDefn *const cls = getClass("b");
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass);
  ASSERT_NE(cls->getExtends(), nullptr) << "8.13: 'class b extends a' should record its base class";
  ASSERT_NE(getClass("a"), nullptr);
  EXPECT_EQ(getBaseClass("b"), getClass("a")) << "the base class of 'b' should be 'a'";
}

TEST_F(GlobalConstraints1Test, ClassBDeclaresOnlyAObj) {
  const hldb::ClassDefn *const cls = getClass("b");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u) << "'b' declares only 'aObj'; 'v' is inherited from 'a'";
  EXPECT_EQ(getVar("b", "v"), nullptr) << "8.13: 'v' is inherited, not redeclared in 'b'";
}

TEST_F(GlobalConstraints1Test, AObjIsRandHandleOfClassA) {
  const hldb::Variable *const aObj = getVar("b", "aObj");
  ASSERT_NE(aObj, nullptr) << "'aObj' should be a Variable of class 'b'";
  EXPECT_TRUE(aObj->getIsRandomized());
  EXPECT_EQ(aObj->getRandType(), vpiRand) << "18.5.9: 'rand' on an object member";
  EXPECT_EQ(aObj->getVisibility(), vpiPublicVis) << "8.18: members are public by default";
  ASSERT_NE(getClass("a"), nullptr);
  EXPECT_EQ(hldb::getReferenced<hldb::ClassDefn>(aObj->getTypespec()), getClass("a"))
      << "'aObj' should be a handle of class 'a'";
}

// --- constraint c { aObj.v < v; } --------------------------------------------

TEST_F(GlobalConstraints1Test, ClassBHasOneConstraint) {
  const hldb::ClassDefn *const cls = getClass("b");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  EXPECT_EQ(cls->getConstraints()->size(), 1u);
  EXPECT_NE(getConstraint(), nullptr);
}

TEST_F(GlobalConstraints1Test, CIsLessThan) {
  const hldb::Constraint *const constraint = getConstraint();
  ASSERT_NE(constraint, nullptr);
  ASSERT_NE(constraint->getConstraintItems(), nullptr);
  ASSERT_EQ(constraint->getConstraintItems()->size(), 1u);

  const hldb::Operation *const op = getConstraintLessThan();
  ASSERT_NE(op, nullptr) << "'aObj.v < v' should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiLtOp);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "'<' is a binary operator";
}

TEST_F(GlobalConstraints1Test, CLhsIsAObjDotV) {
  const hldb::Operation *const op = getConstraintLessThan();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  expectPath(op->getOperands()->at(0), {"aObj", "v"}, {getVar("b", "aObj"), getVar("a", "v")});
}

TEST_F(GlobalConstraints1Test, CRhsIsInheritedV) {
  const hldb::Operation *const op = getConstraintLessThan();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  expectRefTo(op->getOperands()->at(1), "v", getVar("a", "v"));
}

// --- class env extends uvm_env; ----------------------------------------------

TEST_F(GlobalConstraints1Test, EnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getExtends(), nullptr) << "8.13: 'class env extends uvm_env' should record its base class";
  const hldb::ClassDefn *const base = getBaseClass("env");
  ASSERT_NE(base, nullptr) << "'uvm_env' should resolve through 'import uvm_pkg::*;'";
  EXPECT_EQ(base->getName(), "uvm_env");
}

TEST_F(GlobalConstraints1Test, EnvBObjIsClassBWithNew) {
  const hldb::Variable *const bObj = getVar("env", "bObj");
  ASSERT_NE(bObj, nullptr) << "'b bObj = new;' should be a Variable of class 'env'";
  ASSERT_NE(getClass("b"), nullptr);
  EXPECT_EQ(hldb::getReferenced<hldb::ClassDefn>(bObj->getTypespec()), getClass("b"))
      << "'bObj' should be typed by class 'b'";

  const hldb::TFCall *const init = bObj->getValue<hldb::TFCall>();
  ASSERT_NE(init, nullptr) << "8.7: '= new' should initialize 'bObj' with a constructor call";
  EXPECT_EQ(init->getName(), "new");
  EXPECT_TRUE(init->getArguments() == nullptr || init->getArguments()->empty()) << "'new' is called without arguments";
}

// --- function new(string name, uvm_component parent = null); -----------------

TEST_F(GlobalConstraints1Test, EnvNewHasNameAndParentInputs) {
  const hldb::Function *const func = getEnvMethod<hldb::Function>("new");
  ASSERT_NE(func, nullptr) << "'function new(...)' should be a Function method of class 'env'";
  ASSERT_NE(func->getIODecls(), nullptr);
  ASSERT_EQ(func->getIODecls()->size(), 2u);

  const hldb::IODecl *const name = func->getIODecls()->at(0);
  ASSERT_NE(name, nullptr);
  EXPECT_EQ(name->getName(), "name");
  EXPECT_EQ(name->getDirection(), vpiInput) << "13.4: the default argument direction is input";
  ASSERT_NE(name->getTypespec(), nullptr);
  EXPECT_NE(name->getTypespec()->getActual<hldb::StringTypespec>(), nullptr) << "'name' is typed 'string'";

  const hldb::IODecl *const parent = func->getIODecls()->at(1);
  ASSERT_NE(parent, nullptr);
  EXPECT_EQ(parent->getName(), "parent");
  EXPECT_EQ(parent->getDirection(), vpiInput) << "13.4: the default argument direction is input";
  const hldb::ClassDefn *const parentClass = hldb::getReferenced<hldb::ClassDefn>(parent->getTypespec());
  ASSERT_NE(parentClass, nullptr) << "'parent' should be typed by a class";
  EXPECT_EQ(parentClass->getName(), "uvm_component");

  const hldb::Constant *const parentDefault = parent->getExpr<hldb::Constant>();
  ASSERT_NE(parentDefault, nullptr) << "'= null' should be the default value of 'parent'";
  EXPECT_EQ(parentDefault->getConstType(), vpiNullConst);
}

TEST_F(GlobalConstraints1Test, EnvNewCallsSuperNew) {
  const hldb::Function *const func = getEnvMethod<hldb::Function>("new");
  ASSERT_NE(func, nullptr);

  const hldb::RefObj *const stmt = func->getStmt<hldb::RefObj>();
  ASSERT_NE(stmt, nullptr);
  ASSERT_NE(stmt->getActual(), nullptr);

  const hldb::TFCall *const call = stmt->getActual<hldb::TFCall>();
  ASSERT_NE(call, nullptr) << "'super.new(name, parent);' should be the body of 'new'";
  EXPECT_EQ(call->getName(), "new");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::RefObj *const arg0 = call->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg0, nullptr);
  EXPECT_EQ(arg0->getName(), "name");
  const hldb::RefObj *const arg1 = call->getArguments()->at(1)->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg1, nullptr);
  EXPECT_EQ(arg1->getName(), "parent");
}

// --- task run_phase(uvm_phase phase); ... endtask: run_phase -----------------

TEST_F(GlobalConstraints1Test, EnvRunPhaseHasPhaseInput) {
  const hldb::Task *const task = getEnvMethod<hldb::Task>("run_phase");
  ASSERT_NE(task, nullptr) << "'task run_phase(...)' should be a Task method of class 'env'";
  ASSERT_NE(task->getEndLabelObj(), nullptr) << "'endtask: run_phase' has an end label";
  EXPECT_EQ(task->getEndLabelObj()->getName(), "run_phase");
  ASSERT_NE(task->getIODecls(), nullptr);
  ASSERT_EQ(task->getIODecls()->size(), 1u);

  const hldb::IODecl *const phase = task->getIODecls()->at(0);
  ASSERT_NE(phase, nullptr);
  EXPECT_EQ(phase->getName(), "phase");
  EXPECT_EQ(phase->getDirection(), vpiInput) << "13.4: the default argument direction is input";
  const hldb::ClassDefn *const phaseClass = hldb::getReferenced<hldb::ClassDefn>(phase->getTypespec());
  ASSERT_NE(phaseClass, nullptr) << "'phase' should be typed by a class";
  EXPECT_EQ(phaseClass->getName(), "uvm_phase");
}

TEST_F(GlobalConstraints1Test, RunPhaseBodyHasThreeStmts) {
  const hldb::Begin *const body = getRunPhaseBody();
  ASSERT_NE(body, nullptr) << "the three statements of run_phase should be held in a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 3u) << "raise_objection, begin ... end, drop_objection";
}

TEST_F(GlobalConstraints1Test, RunPhaseRaisesObjection) { expectObjectionCall(0, "raise_objection"); }

TEST_F(GlobalConstraints1Test, RunPhaseDropsObjection) { expectObjectionCall(2, "drop_objection"); }

TEST_F(GlobalConstraints1Test, RunPhaseInnerBeginRandomizesBObj) {
  const hldb::Begin *const inner = getRunPhaseInnerBegin();
  ASSERT_NE(inner, nullptr) << "the explicit 'begin ... end' should be a Begin";
  ASSERT_NE(inner->getStmts(), nullptr);
  ASSERT_EQ(inner->getStmts()->size(), 2u) << "'bObj.randomize();' and the if-else";

  const hldb::RefObj *const stmt = any_cast<hldb::RefObj>(inner->getStmts()->at(0));
  ASSERT_NE(stmt, nullptr);
  ASSERT_NE(stmt->getActual(), nullptr);

  const hldb::MethodFuncCall *const call = stmt->getActual<hldb::MethodFuncCall>();
  ASSERT_NE(call, nullptr) << "18.6: 'bObj.randomize()' should be a MethodFuncCall";
  EXPECT_EQ(call->getName(), "randomize");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "'randomize()' has no arguments";
}

TEST_F(GlobalConstraints1Test, RunPhaseIfAObjVLessThanV) {
  const hldb::Begin *const inner = getRunPhaseInnerBegin();
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getStmts(), nullptr);
  ASSERT_EQ(inner->getStmts()->size(), 2u);

  const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(inner->getStmts()->at(1));
  ASSERT_NE(ifElse, nullptr) << "12.4: 'if (...) begin ... end else begin ... end' should be an IfElse";

  const hldb::Operation *const cond = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'bObj.aObj.v < bObj.v' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u) << "'<' is a binary operator";

  expectPath(cond->getOperands()->at(0), {"bObj", "aObj", "v"},
             {getVar("env", "bObj"), getVar("b", "aObj"), getVar("a", "v")});
  expectPath(cond->getOperands()->at(1), {"bObj", "v"}, {getVar("env", "bObj"), getVar("a", "v")});

  EXPECT_NE(ifElse->getStmt<hldb::Begin>(), nullptr) << "the then branch is an explicit 'begin ... end'";
  EXPECT_NE(ifElse->getElseStmt<hldb::Begin>(), nullptr) << "the else branch is an explicit 'begin ... end'";
}

// --- module top ----------------------------------------------------------------

TEST_F(GlobalConstraints1Test, TopHasEnvironmentVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", mod->getVariables());
  ASSERT_NE(environment, nullptr) << "'env environment;' should be a Variable of module 'top'";
  ASSERT_NE(getClass("env"), nullptr);
  EXPECT_EQ(hldb::getReferenced<hldb::ClassDefn>(environment->getTypespec()), getClass("env"))
      << "'environment' is typed by class 'env'";
}

TEST_F(GlobalConstraints1Test, TopHasOneInitialWithTwoStmts) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);

  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(GlobalConstraints1Test, TopAssignsNewEnv) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 1u);

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'environment = new(\"env\");' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "environment");
  ASSERT_NE(getModule(), nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Variable>(),
            hldb::findByName<hldb::Variable>("environment", getModule()->getVariables()));

  const hldb::TFCall *const rhs = assign->getRhs<hldb::TFCall>();
  ASSERT_NE(rhs, nullptr) << "8.7: 'new(\"env\")' should be a constructor call";
  EXPECT_EQ(rhs->getName(), "new");
  ASSERT_NE(rhs->getArguments(), nullptr);
  ASSERT_EQ(rhs->getArguments()->size(), 1u);
  const hldb::Constant *const arg = rhs->getArguments()->at(0)->getHighConn<hldb::Constant>();
  ASSERT_NE(arg, nullptr) << "'\"env\"' should be a Constant";
  EXPECT_EQ(arg->getConstType(), vpiStringConst);
  EXPECT_EQ(arg->getValue(), "env");
}

TEST_F(GlobalConstraints1Test, TopCallsRunTest) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 2u);

  const hldb::TaskCall *const call = any_cast<hldb::TaskCall>(body->getStmts()->at(1));
  ASSERT_NE(call, nullptr) << "'run_test();' calls the uvm_pkg task run_test";
  EXPECT_EQ(call->getName(), "run_test");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "'run_test()' has no arguments";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(GlobalConstraints1Test, CompilerReportsZeroErrors) {
  GTEST_SKIP() << "HLC report a number of binding issues with UVM";
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
