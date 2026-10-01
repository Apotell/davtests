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

// Validates the HLDB model built for
// tests/Google/chapter-18/18.13.3--srandom_0.sv. Lines 1-14 are the license
// and the suite's :name:/:description:/:tags: comment; the rest is:
//
//   16  import uvm_pkg::*;
//   17  `include "uvm_macros.svh"
//   18
//   19  class a;
//   20      rand int x;
//   21      constraint c {x > 0 && x < 30;};
//   22  endclass
//   23
//   24  class env extends uvm_env;
//   25
//   26    a obj = new;
//   27    int ret1, ret2, prev_x, seed = 20;
//   28
//   29    function new(string name, uvm_component parent = null);
//   30      super.new(name, parent);
//   31    endfunction
//   32
//   33    task run_phase(uvm_phase phase);
//   34      phase.raise_objection(this);
//   35      begin
//   36        obj.srandom(seed);
//   37        ret1 = obj.randomize();
//   38        prev_x = obj.x;
//   39        obj.srandom(seed);
//   40        ret2 = obj.randomize();
//   41        if(ret1 == 1 && ret2 == 1 && obj.x > 0 && obj.x < 30 && prev_x == obj.x) begin
//   42          `uvm_info("RESULT", $sformatf("prev_x = %0d obj.x = %0d SUCCESS", prev_x, obj.x), UVM_LOW);
//   43        end else begin
//   44          `uvm_error("RESULT", $sformatf("prev_x = %0d obj.x = %0d FAILED", prev_x, obj.x));
//   45        end
//   46
//   47      end
//   48      phase.drop_objection(this);
//   49    endtask: run_phase
//   50
//   51  endclass
//   52
//   53  module top;
//   54
//   55    env environment;
//   56
//   57    initial begin
//   58      environment = new("env");
//   59      run_test();
//   60    end
//   61
//   62  endmodule
//
// The point of the fixture is the built-in object method srandom()
// (IEEE 1800-2023 18.13.3), which seeds an object's own random number
// generator. The env seeds 'obj' with the same value twice, calling the
// built-in randomize() (18.6.1) after each seeding. That makes both calls
// solve class a's constraint from the same random stream, so the test
// expects the same value of 'x' both times (random stability, 18.14).
//
// UVM macros. uvm_macros.svh is SystemVerilog source included by the file,
// so what the two macro calls expand to is fixed by that source (22.5.1).
// Every UVM version bundled in third_party/UVM (1.1d, 1.2, 1800.2-2017)
// expands them to the same shape:
//
//   `uvm_info(ID, MSG, VERBOSITY)  ->
//       begin
//         if (uvm_report_enabled(VERBOSITY, UVM_INFO, ID))
//           uvm_report_info(ID, MSG, VERBOSITY, <file/line arguments>);
//       end
//   `uvm_error(ID, MSG)            ->
//       begin
//         if (uvm_report_enabled(UVM_NONE, UVM_ERROR, ID))
//           uvm_report_error(ID, MSG, UVM_NONE, <file/line arguments>);
//       end
//
// Only this common part is asserted; the trailing arguments differ between
// versions (see below).
//
// What is checked, and why:
//   Compilation-unit items
//     - `include "uvm_macros.svh" (22.4): the source file records that
//       include
//     - the file uses the macro uvm_info exactly once and uvm_error
//       exactly once
//     - import uvm_pkg::*; (26.3): asserted through its effect -- every UVM
//       name the file uses binds into package uvm_pkg: uvm_env,
//       uvm_component, uvm_phase, run_test, UVM_LOW, UVM_NONE, UVM_INFO and
//       UVM_ERROR. See KNOWN COMPILER BUG (wildcard import resolves types
//       only) below.
//   class a (8.3)
//     - exactly one class named "a"; user-defined; not virtual; no
//       'extends' (8.13), no parameters (8.25), no end label
//     - declares no methods. randomize() and srandom() are built in to
//       every class (18.6.1, 18.13.3); they are not declared here.
//   rand int x; (18.4)
//     - the only property; a signed int (6.11) with no initializer
//     - declared 'rand': getIsRandomized() is true and getRandType() is
//       vpiRand, not vpiRandC (18.4.1)
//     - public (8.18), not const (8.19)
//   constraint c {x > 0 && x < 30;}; (18.5)
//     - the class has exactly one constraint, named "c"; not declared
//       'pure' (18.5.2)
//     - it holds exactly one constraint item: the expression
//       'x > 0 && x < 30', an Operation vpiLogAndOp over vpiGtOp(x, 0) and
//       vpiLtOp(x, 30). Relational operators bind tighter than '&&'
//       (11.3.2). Both 'x' references bind to the rand property x. See
//       KNOWN COMPILER BUG (constraint item wrapped in Distribution) below.
//     - the ';' after the closing '}' is an empty class item (A.1.9
//       class_item ::= ... | ';'). It is legal and creates no node.
//   class env extends uvm_env (8.3, 8.13)
//     - exactly one class named "env"; user-defined; not virtual; no
//       parameters; no end label
//     - its Extends holds exactly one class typespec, "uvm_env", resolving
//       to uvm_pkg's own uvm_env class
//     - exactly 5 properties, in declaration order: obj, ret1, ret2, prev_x,
//       seed. Each is public (8.18), not rand/randc (18.4) and not const
//       (8.19).
//     - 'a obj = new;' -- obj is a handle to class a whose initializer is a
//       'new' call with no arguments (8.7)
//     - 'int ret1, ret2, prev_x, seed = 20;' -- all four are signed ints;
//       only seed has an initializer, 20
//     - exactly 2 methods: the Function 'new' and the Task 'run_phase',
//       both automatic (8.6; see KNOWN COMPILER BUG below)
//   function new(string name, uvm_component parent = null); (8.7)
//     - formals, in order: 'name' (input, string, 6.16) and 'parent'
//       (input, a handle to uvm_pkg's uvm_component)
//     - the body is the single statement 'super.new(name, parent);' (8.15):
//       a hierarchical RefObj whose prefix 'super' binds to uvm_env, and
//       whose MethodFuncCall 'new' binds to uvm_env's own constructor. Its
//       2 arguments, name then parent, bind to THIS constructor's formals.
//   task run_phase(uvm_phase phase); ... endtask: run_phase (13.3)
//     - one formal, 'phase': an input, a handle to uvm_pkg's uvm_phase; the
//       end label 'run_phase'
//     - the body executes 3 statements, in order: phase.raise_objection
//       (this), a begin-end, and phase.drop_objection(this)
//     - each objection call is a hierarchical RefObj: prefix 'phase' bound
//       to the formal, then a MethodFuncCall of that name that resolves to
//       a Function of the same name, with exactly one argument, 'this',
//       bound to class env (8.11)
//   begin ... end (lines 35-47)
//     - exactly 6 statements, in source order: srandom, the ret1
//       assignment, the prev_x assignment, srandom, the ret2 assignment and
//       the if. The blank line 46 is not a statement.
//   obj.srandom(seed); (lines 36 and 39, 18.13.3)
//     - a bare hierarchical RefObj statement: prefix 'obj' bound to env's
//       property, then a MethodFuncCall "srandom" with exactly one
//       argument, a RefObj bound to env's property 'seed'
//   ret1 = obj.randomize(); and ret2 = obj.randomize(); (18.6.1)
//     - blocking Assignments to env's 'ret1' / 'ret2' whose RHS is the
//       hierarchical RefObj obj.randomize(): a MethodFuncCall "randomize"
//       with no arguments and no 'with' inline constraint (18.7)
//   prev_x = obj.x;
//     - blocking Assignment to env's 'prev_x' whose RHS is the hierarchical
//       RefObj obj.x: prefix bound to env's 'obj', member bound to class
//       a's 'x'
//   if (ret1 == 1 && ret2 == 1 && obj.x > 0 && obj.x < 30 && prev_x == obj.x)
//     - an IfElse (12.4) whose condition is a LEFT-associative chain of
//       four vpiLogAndOp operations (11.3.2: operators of equal precedence
//       associate left to right), i.e. (((A && B) && C) && D) && E, with the
//       five conjuncts in source order:
//         A  ret1 == 1      vpiEqOp, RefObj ret1, literal 1
//         B  ret2 == 1      vpiEqOp, RefObj ret2, literal 1
//         C  obj.x > 0      vpiGtOp, RefObj obj.x, literal 0
//         D  obj.x < 30     vpiLtOp, RefObj obj.x, literal 30
//         E  prev_x == obj.x vpiEqOp, RefObj prev_x, RefObj obj.x
//     - C and D restate constraint c on the randomized object
//   `uvm_info("RESULT", $sformatf("prev_x = %0d obj.x = %0d SUCCESS",
//             prev_x, obj.x), UVM_LOW);
//     - the then-branch starts with the macro's own begin-end, holding
//       exactly one IfStmt whose condition is uvm_report_enabled(UVM_LOW,
//       UVM_INFO, "RESULT") and whose action is uvm_report_info with first
//       3 arguments "RESULT", $sformatf(...) and UVM_LOW
//     - $sformatf is a SysFuncCall with 3 arguments: the string literal
//       (vpiStringConst), a RefObj bound to 'prev_x', and the hierarchical
//       RefObj obj.x
//     - every UVM_* name binds to an EnumConst of the same name
//   `uvm_error("RESULT", $sformatf("prev_x = %0d obj.x = %0d FAILED",
//              prev_x, obj.x));
//     - the else-branch has the same shape, with
//       uvm_report_enabled(UVM_NONE, UVM_ERROR, "RESULT") and
//       uvm_report_error(..., UVM_NONE, ...)
//   module top (23.2)
//     - no ports; exactly 1 variable, 'environment', a handle to class env
//     - exactly 1 process: an initial whose begin-end holds 2 statements
//     - 'environment = new("env");' is a blocking Assignment. Its RHS is a
//       MethodFuncCall 'new' with the one argument "env", resolving to
//       env's own constructor (8.7).
//     - 'run_test();' is a TaskCall bound by object identity to uvm_pkg's
//       package-level task run_test, and writes no argument
//   Diagnostics
//     - no COMP_UNDEFINED_VARIABLE for any variable or formal the file
//       declares, and no COMP_FAILED_TO_BIND for any name it calls,
//       references or extends -- including the built-ins srandom and
//       randomize
//     - the file is legal: zero fatal, syntax and error diagnostics. See
//       KNOWN COMPILER BUG (UVM library does not compile cleanly) below.
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no parameter, no module instance and no class specialization.
// The constraint and the if condition read variables, so neither is a
// constant expression. The literals are already constants, and every
// binding asserted is name resolution. No check is gated on
// getElaborated().
//
// KNOWN COMPILER BUG (class-method lifetime), not a defect in this test:
// the 18.13.2--urandom_range tests found that HLC gives class methods the
// static default that 13.4.2 applies to subroutines declared outside a
// class, although 8.6 says "The lifetime of methods declared as part of a
// class type shall be automatic." EnvMethodsHaveAutomaticLifetime asserts
// the LRM value for env's 'new' and 'run_phase' and is expected to fail
// until HLC is fixed; it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (constraint item wrapped in Distribution), not a
// defect in this test: HLC stores the single item of
// 'constraint c {x > 0 && x < 30;}' as a Distribution node whose
// expression is the '&&' Operation, although no 'dist' is written. Per
// 18.5 (Syntax 18-2) and A.1.10, constraint_expression ::= [soft]
// expression_or_dist ';' and expression_or_dist ::= expression
// [dist { dist_list }]; without 'dist' the item is the expression itself,
// and a distribution (18.5.3) exists only when a dist_list is written.
// ConstraintCHasOneExpressionItem and ConstraintCBoundsXBetweenZeroAndThirty
// assert the LRM shape and are expected to fail until HLC is fixed; they
// are intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (wildcard import resolves types only), not a defect in
// this test: 26.3 makes every identifier declared in uvm_pkg a candidate
// for import through 'import uvm_pkg::*;', enumeration literals and
// package-level subroutines included. HLC resolves the imported class names
// (uvm_env, uvm_component, uvm_phase) but not the enum constants UVM_LOW,
// UVM_INFO, UVM_NONE and UVM_ERROR, nor the package task run_test, although
// uvm_pkg declares all of them (UvmPkgProvidesTheNamesTheFileUses finds
// run_test there). HLC reports each as "Failed to bind", and because
// run_test does not bind, 'run_test();' is not modeled as a TaskCall.
// ThenBranchIsUvmInfoExpansion, ElseBranchIsUvmErrorExpansion,
// InitialCallsRunTestFromUvmPkg and the 'run_test' entry of
// EveryCalledOrExtendedNameBinds assert the LRM binding and are expected to
// fail until HLC is fixed; they are intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (UVM library does not compile cleanly), not a defect in
// this test: HLC reports about 1900 errors when it compiles UVM
// 1800.2-2017-1.0 on its own, and none of them is in this file. The largest
// groups are about 600 CP5824 "Multiply defined typedef" errors, raised on
// classes UVM forward-declares with 'typedef class ...;' before defining
// them (legal per 6.18; uvm_object is one), and about 1000 LN7705 "Null
// Actual" references HLC leaves unresolved inside the library.
// NoFatalSyntaxOrErrorDiagnostics asserts zero errors, as legal source
// requires, and is expected to fail until HLC is fixed. The same unresolved
// library references make the 'randomize' and 'new' entries of
// EveryCalledOrExtendedNameBinds fail: findError() searches the whole
// design, and every failed bind it finds for those two names is in UVM's own
// source, not in this file. Neither is skipped or relaxed.
//
// What is NOT checked, and why:
//   - The value randomize() picks for x, its return value (1 on success,
//     18.6.1), the constraint solving that keeps x within 1..29, srandom()
//     reseeding obj's generator (18.13.3) so that both randomize() calls
//     yield the same x (18.14), whether 'prev_x == obj.x' holds, which
//     branch runs, the text the report prints (including '%0d'
//     formatting), objection counting, phase scheduling and which test
//     run_test starts all only exist while simulation runs. Permanently
//     out of scope. The static half is covered by:
//     ConstraintCBoundsXBetweenZeroAndThirty, FirstSrandomSeedsObjWithSeed,
//     SecondSrandomSeedsObjWithSeed, Ret1IsAssignedObjRandomize,
//     Ret2IsAssignedObjRandomize, PrevXIsAssignedObjX,
//     IfConditionIsFiveConjunctsInSourceOrder, ThenBranchIsUvmInfoExpansion,
//     ElseBranchIsUvmErrorExpansion and InitialCallsRunTestFromUvmPkg.
//   - Whether constraint c is active (constraint_mode(), 18.9) is object
//     state that changes while simulation runs, so
//     Constraint::getIsConstraintEnabled() is not asserted. The constraint's
//     declaration is covered by ClassAHasOneConstraintC.
//   - srandom() and randomize() are built-in methods (18.13.3, 18.6.1).
//     Whether HLC binds such a call to a built-in declaration through
//     getTaskFunc() is a tool convention. Their names, arguments and missing
//     'with' clause are asserted, and EveryCalledOrExtendedNameBinds checks
//     that neither draws a bind error.
//   - Arguments of uvm_report_info/uvm_report_error after the third
//     (`uvm_file, `uvm_line, and in 1.2/1800.2 also "" and 1) depend on the
//     UVM version the .hlc selects and on UVM_REPORT_DISABLE_FILE_LINE, so
//     they are not asserted.
//   - Each macro call is followed by ';', which leaves a null statement
//     after the macro's begin-end (A.6.4 statement_or_null). Whether HLC
//     stores null statements is a tool convention, so the branch blocks are
//     read at index 0 and their statement count is not asserted.
//   - Whether the unqualified inherited calls uvm_report_enabled/
//     uvm_report_info/uvm_report_error are modeled as FuncCall or
//     MethodFuncCall is a tool convention; they are checked through TFCall
//     by name and arguments.
//   - Where HLC records a compilation-unit 'import uvm_pkg::*;' is a model
//     convention. Its effect is asserted through the bindings listed above.
//   - raise_objection/drop_objection are declared 'extern virtual' in
//     uvm_phase in every bundled UVM version. Whether a call binds to the
//     prototype or the out-of-block body is a tool convention, so the bound
//     method's name and kind are asserted, not its object identity.
//   - Whether run_phase is flagged virtual: it overrides uvm_component's
//     virtual run_phase, so 8.20 makes it virtual although the keyword is not
//     written. Whether getVirtual() reports that or only the written keyword
//     is a model convention.
//   - 'a obj = new;' invokes class a's implicit constructor (8.7). How HLC
//     represents an implicit constructor is a tool convention, so that
//     call's binding is not asserted; class a's method count leaves 'new'
//     out for the same reason.
//   - Default values of formals ('parent = null'): no test in this suite
//     reads a subroutine formal's default, so there is no grounded accessor
//     for it.
//   - Design-wide counts of classes and packages depend on the UVM library
//     compiled with the file, so every class is found by name. The .hlc
//     only puts UVM 1800.2-2017 on the include path, so SetUpTestSuite
//     compiles 1800.2-2017-1.0/src/uvm_pkg.sv (loaded from HLC's
//     precompiled package cache) before the .hlc's source file, which
//     imports it.
//   - The warning count: compiling the UVM library can draw tool-specific
//     warnings, so only the fatal, syntax and error counts are asserted.
//   - Source line numbers. A start line encodes nothing about
//     SystemVerilog semantics and changes whenever the fixture is
//     reformatted or a comment is added, so no node's location is asserted.
//   - The license and metadata comments are comments, not design objects.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
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
#include <hldb/enum_const.h>
#include <hldb/extends.h>
#include <hldb/function.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/task.h>
#include <hldb/task_call.h>
#include <hldb/task_func.h>
#include <hldb/tf_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace hlc {

class Srandom0Test : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-wd", "../../../../third_party/UVM", "1800.2-2017-1.0/src/uvm_pkg.sv", "-wd", ".", "-f",
                       "18.13.3--srandom_0.hlc"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // --- UVM declarations the file binds to --------------------------------

  static const hldb::Package *getUvmPkg() {
    return hldb::findByName<hldb::Package>("uvm_pkg", m_design->getAllPackages());
  }

  static const hldb::ClassDefn *getUvmClass(std::string_view name) {
    const hldb::Package *const pkg = getUvmPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>(name, pkg->getClassDefns());
  }

  static const hldb::Function *getUvmEnvNew() {
    const hldb::ClassDefn *const uvmEnv = getUvmClass("uvm_env");
    if (uvmEnv == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("new", uvmEnv->getMethods());
  }

  static const hldb::Task *getRunTest() {
    const hldb::Package *const pkg = getUvmPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Task>("run_test", pkg->getTaskFuncs());
  }

  // --- class a ------------------------------------------------------------

  static const hldb::ClassDefn *getClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Variable *getX() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("x", cls->getVariables());
  }

  static const hldb::Constraint *getConstraintC() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) return nullptr;
    return hldb::findByName<hldb::Constraint>("c", cls->getConstraints());
  }

  // The methods class a declares. 8.7 provides an implicit 'new' when a
  // class writes none; whether HLC materializes it in getMethods() is a
  // tool convention, so it is left out.
  static std::vector<const hldb::TaskFunc *> getClassADeclaredMethods() {
    std::vector<const hldb::TaskFunc *> methods;
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr || cls->getMethods() == nullptr) return methods;
    for (const hldb::TaskFunc *const method : *cls->getMethods()) {
      if (method->getName() != "new") methods.emplace_back(method);
    }
    return methods;
  }

  // --- class env ----------------------------------------------------------

  static const hldb::ClassDefn *getEnv() { return hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses()); }

  static const hldb::Variable *getProperty(std::string_view name) {
    const hldb::ClassDefn *const env = getEnv();
    if (env == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, env->getVariables());
  }

  static const hldb::Function *getEnvNew() {
    const hldb::ClassDefn *const env = getEnv();
    if (env == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("new", env->getMethods());
  }

  static const hldb::Task *getRunPhase() {
    const hldb::ClassDefn *const env = getEnv();
    if (env == nullptr) return nullptr;
    return hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  }

  static const hldb::Any *getRunPhaseStmt(size_t index) {
    const hldb::Task *const task = getRunPhase();
    if (task == nullptr) return nullptr;
    const hldb::Begin *const body = task->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= index) return nullptr;
    return body->getStmts()->at(index);
  }

  // The explicit 'begin ... end' on lines 35-47.
  static const hldb::Begin *getInnerBlock() { return any_cast<hldb::Begin>(getRunPhaseStmt(1)); }

  static const hldb::Any *getInnerStmt(size_t index) {
    const hldb::Begin *const block = getInnerBlock();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() <= index) return nullptr;
    return block->getStmts()->at(index);
  }

  static const hldb::IfElse *getIfElse() { return any_cast<hldb::IfElse>(getInnerStmt(5)); }

  // The operands of a left-associative '&&' chain, in source order. 11.3.2:
  // operators of equal precedence associate left to right, so 'p && q && r'
  // is '(p && q) && r'. The walk follows the LEFT operand only, so a chain
  // nested any other way, or flattened into one n-ary operation, yields a
  // different count.
  static std::vector<const hldb::Any *> getConjuncts(const hldb::Any *cond) {
    std::vector<const hldb::Any *> conjuncts;
    const hldb::Any *node = cond;
    while (const hldb::Operation *const op = any_cast<hldb::Operation>(node)) {
      if (op->getOpType() != vpiLogAndOp || op->getOperands() == nullptr || op->getOperands()->size() != 2) break;
      conjuncts.emplace(conjuncts.begin(), op->getOperands()->at(1));
      node = op->getOperands()->at(0);
    }
    if (node != nullptr) conjuncts.emplace(conjuncts.begin(), node);
    return conjuncts;
  }

  static std::vector<const hldb::Any *> getIfConjuncts() {
    const hldb::IfElse *const ifElse = getIfElse();
    if (ifElse == nullptr) return {};
    return getConjuncts(ifElse->getCondition());
  }

  // --- module top ---------------------------------------------------------

  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getEnvironment() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("environment", top->getVariables());
  }

  static const hldb::Initial *getInitial() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Initial>(top->getProcesses()->at(0));
  }

  static const hldb::Any *getInitialStmt(size_t index) {
    const hldb::Initial *const init = getInitial();
    if (init == nullptr) return nullptr;
    const hldb::Begin *const block = init->getStmt<hldb::Begin>();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() <= index) return nullptr;
    return block->getStmts()->at(index);
  }

  // --- source file ----------------------------------------------------------

  static const hldb::SourceFile *getSourceFile() {
    if (m_design->getSourceFiles() == nullptr) return nullptr;
    for (const hldb::SourceFile *const sf : *m_design->getSourceFiles()) {
      if (sf->getName().find("18.13.3--srandom_0.sv") != std::string_view::npos) return sf;
    }
    return nullptr;
  }

  static std::vector<const hldb::PreprocMacroInstance *> getMacroInstances(std::string_view name) {
    std::vector<const hldb::PreprocMacroInstance *> found;
    const hldb::SourceFile *const sf = getSourceFile();
    if (sf == nullptr || sf->getPreprocMacroInstances() == nullptr) return found;
    for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
      if (mi->getName() == name) found.emplace_back(mi);
    }
    return found;
  }

  // --- expectations -------------------------------------------------------

  // Verifies 'expr' is a RefObj named 'name' bound by object identity to
  // 'target'.
  static void ExpectBoundRef(const hldb::Any *expr, std::string_view name, const hldb::Any *target) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(target, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(ref->getActual(), target) << "'" << name << "' must bind to its declaration";
  }

  // Verifies 'expr' is a RefObj named 'name' bound to the enum constant of
  // the same name that uvm_pkg declares.
  static void ExpectEnumConstRef(const hldb::Any *expr, std::string_view name) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    const hldb::EnumConst *const value = ref->getActual<hldb::EnumConst>();
    ASSERT_NE(value, nullptr) << "'" << name << "' is an enum constant declared in uvm_pkg";
    EXPECT_EQ(value->getName(), name);
  }

  // Verifies 'expr' is the string literal whose source text is 'text'.
  static void ExpectStringLiteral(const hldb::Any *expr, std::string_view text) {
    const hldb::Constant *const literal = any_cast<hldb::Constant>(expr);
    ASSERT_NE(literal, nullptr) << text << " should be a Constant";
    EXPECT_EQ(literal->getDecompile(), text);
    EXPECT_EQ(literal->getConstType(), vpiStringConst) << text << " is a string literal (5.9)";
  }

  // Verifies 'expr' is the integer literal whose source text is 'text'.
  static void ExpectIntLiteral(const hldb::Any *expr, std::string_view text) {
    const hldb::Constant *const literal = any_cast<hldb::Constant>(expr);
    ASSERT_NE(literal, nullptr) << "'" << text << "' should be a Constant";
    EXPECT_EQ(literal->getDecompile(), text);
  }

  // Verifies 'type' is 'int': an IntTypespec that is signed.
  static void ExpectSignedInt(const hldb::RefTypespec *type, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::IntTypespec *const ts = type->getActual<hldb::IntTypespec>();
    ASSERT_NE(ts, nullptr) << what << " is declared 'int'";
    EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is a signed type, so " << what << " is signed";
  }

  // Verifies 'type' is a class handle type resolving to 'cls'.
  static void ExpectClassHandle(const hldb::RefTypespec *type, const hldb::ClassDefn *cls, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::ClassTypespec *const ct = type->getActual<hldb::ClassTypespec>();
    ASSERT_NE(ct, nullptr) << what << " is declared with a class type";
    ASSERT_NE(cls, nullptr) << "the class " << what << " refers to was not found";
    EXPECT_EQ(ct->getClassDefn(), cls) << what << " must resolve to its class";
  }

  // Verifies 'expr' is 'obj.x': a hierarchical RefObj whose prefix binds to
  // env's 'obj' and whose member binds to class a's 'x'.
  static void ExpectObjX(const hldb::Any *expr) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'obj.x' should be a hierarchical RefObj";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    ExpectBoundRef(path->getPathElems()->at(0), "obj", getProperty("obj"));
    ExpectBoundRef(path->getPathElems()->at(1), "x", getX());
  }

  // Verifies 'expr' is 'obj.<method>(...)': a hierarchical RefObj whose
  // prefix binds to env's 'obj' and whose last element is a MethodFuncCall
  // named 'method'. Stores that call in 'call'.
  static void ExpectObjMethodCall(const hldb::Any *expr, std::string_view method, const hldb::MethodFuncCall **call) {
    *call = nullptr;
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'obj." << method << "(...)' should be a hierarchical RefObj";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    ExpectBoundRef(path->getPathElems()->at(0), "obj", getProperty("obj"));
    const hldb::MethodFuncCall *const found = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
    ASSERT_NE(found, nullptr) << "'" << method << "' is a function called through a handle, so a MethodFuncCall";
    EXPECT_EQ(found->getName(), method);
    *call = found;
  }

  // Verifies 'stmt' is 'obj.srandom(seed);' (18.13.3).
  static void ExpectSrandomOfSeed(const hldb::Any *stmt) {
    const hldb::MethodFuncCall *call = nullptr;
    ExpectObjMethodCall(stmt, "srandom", &call);
    ASSERT_NE(call, nullptr);
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u) << "18.13.3: srandom takes one argument, the seed";
    ExpectBoundRef(call->getArguments()->at(0), "seed", getProperty("seed"));
  }

  // Verifies 'stmt' is '<target> = obj.randomize();' (18.6.1).
  static void ExpectRandomizeInto(const hldb::Any *stmt, std::string_view target) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
    ASSERT_NE(assign, nullptr) << "'" << target << " = obj.randomize();' should be an Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
    ExpectBoundRef(assign->getLhs(), target, getProperty(target));
    const hldb::MethodFuncCall *call = nullptr;
    ExpectObjMethodCall(assign->getRhs(), "randomize", &call);
    ASSERT_NE(call, nullptr);
    EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
        << "'randomize()' is written with no arguments";
    EXPECT_EQ(call->getWith(), nullptr) << "18.7: no 'with' inline constraint is written";
  }

  // Verifies 'stmt' is 'phase.<method>(this);'.
  static void ExpectObjectionCall(const hldb::Any *stmt, std::string_view method) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(stmt);
    ASSERT_NE(path, nullptr) << "'phase." << method << "(this)' should be a hierarchical RefObj";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    const hldb::Task *const runPhase = getRunPhase();
    ASSERT_NE(runPhase, nullptr);
    ExpectBoundRef(path->getPathElems()->at(0), "phase",
                   hldb::findByName<hldb::IODecl>("phase", runPhase->getIODecls()));

    const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
    ASSERT_NE(call, nullptr) << "'" << method << "' is a function of uvm_phase, so it is a MethodFuncCall";
    EXPECT_EQ(call->getName(), method);
    const hldb::Function *const target = call->getTaskFunc<hldb::Function>();
    ASSERT_NE(target, nullptr) << "'" << method << "' must resolve to uvm_phase's function";
    EXPECT_EQ(target->getName(), method);
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u) << "only 'this' is written";
    const hldb::RefObj *const self = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    ASSERT_NE(self, nullptr) << "'this' should be a RefObj";
    EXPECT_EQ(self->getName(), "this");
    ASSERT_NE(getEnv(), nullptr);
    EXPECT_EQ(self->getActual<hldb::ClassDefn>(), getEnv()) << "8.11: 'this' is the enclosing class env";
  }

  // Verifies 'expr' is a binary Operation of type 'opType' and stores its two
  // operands in 'lhs' and 'rhs'.
  static void ExpectBinaryOp(const hldb::Any *expr, int32_t opType, std::string_view what, const hldb::Any **lhs,
                             const hldb::Any **rhs) {
    *lhs = nullptr;
    *rhs = nullptr;
    const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
    ASSERT_NE(op, nullptr) << "'" << what << "' should be an Operation";
    EXPECT_EQ(op->getOpType(), opType) << "'" << what << "'";
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 2u) << "'" << what << "' is a binary operation";
    *lhs = op->getOperands()->at(0);
    *rhs = op->getOperands()->at(1);
  }

  // Verifies 'expr' is '$sformatf(<format>, prev_x, obj.x)'.
  static void ExpectSformatfOfPrevXAndObjX(const hldb::Any *expr, std::string_view format) {
    const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(expr);
    ASSERT_NE(call, nullptr) << "the message argument should be a $sformatf SysFuncCall";
    EXPECT_EQ(call->getName(), "$sformatf");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 3u) << "a format string, 'prev_x' and 'obj.x'";
    ExpectStringLiteral(call->getArguments()->at(0), format);
    ExpectBoundRef(call->getArguments()->at(1), "prev_x", getProperty("prev_x"));
    ExpectObjX(call->getArguments()->at(2));
  }

  // Verifies 'branch' is a begin-end starting with the expansion of a UVM
  // report macro:
  //   begin
  //     if (uvm_report_enabled(<verbosity>, <severity>, "RESULT"))
  //       <reportFn>("RESULT", $sformatf(<format>, prev_x, obj.x), <verbosity>, ...);
  //   end
  static void ExpectUvmReportMacro(const hldb::Any *branch, std::string_view reportFn, std::string_view severity,
                                   std::string_view verbosity, std::string_view format) {
    const hldb::Begin *const block = any_cast<hldb::Begin>(branch);
    ASSERT_NE(block, nullptr) << "the branch is an explicit begin-end";
    ASSERT_NE(block->getStmts(), nullptr);
    ASSERT_FALSE(block->getStmts()->empty());
    const hldb::Begin *const macroBlock = any_cast<hldb::Begin>(block->getStmts()->at(0));
    ASSERT_NE(macroBlock, nullptr) << "the macro expands to its own begin-end";
    ASSERT_NE(macroBlock->getStmts(), nullptr);
    ASSERT_EQ(macroBlock->getStmts()->size(), 1u) << "the macro's begin-end holds a single if statement";
    const hldb::IfStmt *const guard = any_cast<hldb::IfStmt>(macroBlock->getStmts()->at(0));
    ASSERT_NE(guard, nullptr) << "the macro's if has no else, so it is an IfStmt";

    const hldb::TFCall *const enabled = guard->getCondition<hldb::TFCall>();
    ASSERT_NE(enabled, nullptr) << "the if condition is a call";
    EXPECT_EQ(enabled->getName(), "uvm_report_enabled");
    ASSERT_NE(enabled->getArguments(), nullptr);
    ASSERT_EQ(enabled->getArguments()->size(), 3u);
    ExpectEnumConstRef(enabled->getArguments()->at(0), verbosity);
    ExpectEnumConstRef(enabled->getArguments()->at(1), severity);
    ExpectStringLiteral(enabled->getArguments()->at(2), "\"RESULT\"");

    const hldb::TFCall *const report = guard->getStmt<hldb::TFCall>();
    ASSERT_NE(report, nullptr) << "the if action is a call";
    EXPECT_EQ(report->getName(), reportFn);
    ASSERT_NE(report->getArguments(), nullptr);
    ASSERT_GE(report->getArguments()->size(), 3u);
    ExpectStringLiteral(report->getArguments()->at(0), "\"RESULT\"");
    ExpectSformatfOfPrevXAndObjX(report->getArguments()->at(1), format);
    ExpectEnumConstRef(report->getArguments()->at(2), verbosity);
  }
};

// ---------------------------------------------------------------------------
// import uvm_pkg::*;  `include "uvm_macros.svh"
// ---------------------------------------------------------------------------

TEST_F(Srandom0Test, SourceFileIncludesUvmMacros) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr) << "18.13.3--srandom_0.sv not found in Design::getSourceFiles()";
  ASSERT_NE(sf->getIncludes(), nullptr) << "22.4: the file includes uvm_macros.svh";
  size_t count = 0;
  for (const hldb::SourceFile *const inc : *sf->getIncludes()) {
    if (inc->getName().find("uvm_macros.svh") != std::string_view::npos) ++count;
  }
  EXPECT_EQ(count, 1u) << "'`include \"uvm_macros.svh\"' is written once";
}

TEST_F(Srandom0Test, FileUsesUvmInfoOnceAndUvmErrorOnce) {
  const std::vector<const hldb::PreprocMacroInstance *> infos = getMacroInstances("uvm_info");
  ASSERT_EQ(infos.size(), 1u);
  const std::vector<const hldb::PreprocMacroInstance *> errors = getMacroInstances("uvm_error");
  ASSERT_EQ(errors.size(), 1u);
}

TEST_F(Srandom0Test, UvmPkgProvidesTheNamesTheFileUses) {
  ASSERT_NE(getUvmPkg(), nullptr) << "26.3: 'import uvm_pkg::*;' names package uvm_pkg";
  for (std::string_view name : {"uvm_env", "uvm_component", "uvm_phase"}) {
    const hldb::ClassDefn *const cls = getUvmClass(name);
    ASSERT_NE(cls, nullptr) << "class '" << name << "' not found in uvm_pkg";
    EXPECT_EQ(cls->getName(), name);
  }
  const hldb::Task *const runTest = getRunTest();
  ASSERT_NE(runTest, nullptr) << "task 'run_test' not found in uvm_pkg";
  EXPECT_EQ(runTest->getName(), "run_test");
}

// ---------------------------------------------------------------------------
// class a; rand int x; constraint c {...}; endclass
// ---------------------------------------------------------------------------

TEST_F(Srandom0Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  size_t count = 0;
  for (const hldb::ClassDefn *const cls : *m_design->getAllClasses()) {
    if (cls->getName() == "a") ++count;
  }
  EXPECT_EQ(count, 1u) << "the source declares class 'a' exactly once";
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass) << "8.3: 'class a;' is a user-defined class";
}

TEST_F(Srandom0Test, ClassAHasNoBaseClassParametersOrEndLabel) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getName(), "a");
  EXPECT_FALSE(cls->getVirtual()) << "8.21: 'class a' is not declared 'virtual class'";
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
}

TEST_F(Srandom0Test, ClassADeclaresNoMethods) {
  ASSERT_NE(getClassA(), nullptr);
  EXPECT_EQ(getClassADeclaredMethods().size(), 0u)
      << "class a declares no task or function; randomize() and srandom() are built in (18.6.1, 18.13.3)";
}

TEST_F(Srandom0Test, ClassAHasOnePropertyX) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'rand int x;' is the only property; a constraint is not one";
  EXPECT_EQ(cls->getVariables()->at(0)->getName(), "x");
  EXPECT_EQ(cls->getVariables()->at(0), getX());
}

TEST_F(Srandom0Test, XIsRandSignedIntWithoutInitializer) {
  const hldb::Variable *const x = getX();
  ASSERT_NE(x, nullptr);
  ExpectSignedInt(x->getTypespec(), "'x'");
  EXPECT_TRUE(x->getIsRandomized()) << "18.4: 'x' is declared with the 'rand' qualifier";
  EXPECT_EQ(x->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(x->getVisibility(), vpiPublicVis) << "8.18: 'x' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(x->getConstantVariable()) << "8.19: 'x' has no 'const' qualifier";
  EXPECT_EQ(x->getValue(), nullptr) << "'rand int x;' has no initializer";
}

TEST_F(Srandom0Test, ClassAHasOneConstraintC) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr) << "18.5: 'constraint c {...}' declares a constraint block";
  ASSERT_EQ(cls->getConstraints()->size(), 1u);
  const hldb::Constraint *const c = getConstraintC();
  ASSERT_NE(c, nullptr) << "constraint 'c' not found";
  EXPECT_EQ(c->getName(), "c");
  EXPECT_FALSE(c->getPure()) << "18.5.2: 'c' is not declared 'pure constraint'";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (constraint
// item wrapped in Distribution) in the file header.
TEST_F(Srandom0Test, ConstraintCHasOneExpressionItem) {
  const hldb::Constraint *const c = getConstraintC();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 1u) << "the block holds one expression, 'x > 0 && x < 30'";
  const hldb::Operation *const item = any_cast<hldb::Operation>(c->getConstraintItems()->at(0));
  ASSERT_NE(item, nullptr) << "18.5 (Syntax 18-2): an expression constraint item is the expression itself";
  EXPECT_EQ(item->getOpType(), vpiLogAndOp) << "11.3.2: '&&' binds looser than '>' and '<'";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (constraint
// item wrapped in Distribution) in the file header.
TEST_F(Srandom0Test, ConstraintCBoundsXBetweenZeroAndThirty) {
  const hldb::Constraint *const c = getConstraintC();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 1u);
  const hldb::Any *lower = nullptr;
  const hldb::Any *upper = nullptr;
  ExpectBinaryOp(c->getConstraintItems()->at(0), vpiLogAndOp, "x > 0 && x < 30", &lower, &upper);

  const hldb::Any *lhs = nullptr;
  const hldb::Any *rhs = nullptr;
  ExpectBinaryOp(lower, vpiGtOp, "x > 0", &lhs, &rhs);
  ExpectBoundRef(lhs, "x", getX());
  ExpectIntLiteral(rhs, "0");

  ExpectBinaryOp(upper, vpiLtOp, "x < 30", &lhs, &rhs);
  ExpectBoundRef(lhs, "x", getX());
  ExpectIntLiteral(rhs, "30");
}

// ---------------------------------------------------------------------------
// class env extends uvm_env; ... endclass
// ---------------------------------------------------------------------------

TEST_F(Srandom0Test, ClassEnvIsDeclaredOnceAsUserDefinedClass) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  size_t count = 0;
  for (const hldb::ClassDefn *const cls : *m_design->getAllClasses()) {
    if (cls->getName() == "env") ++count;
  }
  EXPECT_EQ(count, 1u) << "the source declares class 'env' exactly once";
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(env->getClassType(), vpiUserDefinedClass);
  EXPECT_FALSE(env->getVirtual()) << "8.21: 'class env' is not declared 'virtual class'";
  EXPECT_TRUE(env->getParameters() == nullptr || env->getParameters()->empty())
      << "8.25: 'class env' has no parameter port list";
  EXPECT_EQ(env->getEndLabelObj(), nullptr) << "'endclass' is written without ': env'";
}

TEST_F(Srandom0Test, ClassEnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  const hldb::Extends *const ext = env->getExtends();
  ASSERT_NE(ext, nullptr) << "8.13: 'class env extends uvm_env' names a base class";
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  const hldb::RefTypespec *const base = ext->getClassTypespecs()->at(0);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "uvm_env");
  const hldb::ClassTypespec *const ct = base->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  ASSERT_NE(getUvmClass("uvm_env"), nullptr);
  EXPECT_EQ(ct->getClassDefn(), getUvmClass("uvm_env")) << "26.3: 'uvm_env' is imported from uvm_pkg";
}

TEST_F(Srandom0Test, ClassEnvHasFivePropertiesInDeclarationOrder) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getVariables(), nullptr);
  ASSERT_EQ(env->getVariables()->size(), 5u) << "'a obj' plus 'int ret1, ret2, prev_x, seed'";
  const std::vector<std::string_view> expected = {"obj", "ret1", "ret2", "prev_x", "seed"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = env->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getIsRandomized()) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(Srandom0Test, ObjIsClassAHandleInitializedWithNew) {
  const hldb::Variable *const obj = getProperty("obj");
  ASSERT_NE(obj, nullptr);
  ExpectClassHandle(obj->getTypespec(), getClassA(), "'obj'");
  const hldb::MethodFuncCall *const init = obj->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(init, nullptr) << "8.7: 'a obj = new;' initializes obj with a constructor call";
  EXPECT_EQ(init->getName(), "new");
  EXPECT_TRUE(init->getArguments() == nullptr || init->getArguments()->empty()) << "'new' is written with no arguments";
}

TEST_F(Srandom0Test, IntPropertiesAreSignedAndOnlySeedIsInitialized) {
  for (std::string_view name : {"ret1", "ret2", "prev_x", "seed"}) {
    const hldb::Variable *const prop = getProperty(name);
    ASSERT_NE(prop, nullptr) << "property '" << name << "' not found";
    ExpectSignedInt(prop->getTypespec(), name);
  }
  for (std::string_view name : {"ret1", "ret2", "prev_x"}) {
    EXPECT_EQ(getProperty(name)->getValue(), nullptr) << "'" << name << "' is declared with no initializer";
  }
  const hldb::Constant *const seedInit = getProperty("seed")->getValue<hldb::Constant>();
  ASSERT_NE(seedInit, nullptr) << "'seed = 20'";
  EXPECT_EQ(seedInit->getDecompile(), "20");
}

TEST_F(Srandom0Test, ClassEnvHasMethodsNewAndRunPhase) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getMethods(), nullptr);
  EXPECT_EQ(env->getMethods()->size(), 2u) << "the class body declares 'new' and 'run_phase'";
  const hldb::Function *const ctor = getEnvNew();
  ASSERT_NE(ctor, nullptr) << "8.7: 'function new(...)' is a Function";
  EXPECT_TRUE(ctor->getMethod());
  EXPECT_EQ(ctor->getVisibility(), vpiPublicVis);
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr) << "13.3: 'task run_phase(...)' is a Task";
  EXPECT_TRUE(runPhase->getMethod());
  EXPECT_EQ(runPhase->getVisibility(), vpiPublicVis);
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (class-method
// lifetime) in the file header.
TEST_F(Srandom0Test, EnvMethodsHaveAutomaticLifetime) {
  ASSERT_NE(getEnvNew(), nullptr);
  ASSERT_NE(getRunPhase(), nullptr);
  EXPECT_TRUE(getEnvNew()->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
  EXPECT_TRUE(getRunPhase()->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
}

// ---------------------------------------------------------------------------
// function new(string name, uvm_component parent = null);
//   super.new(name, parent);
// endfunction
// ---------------------------------------------------------------------------

TEST_F(Srandom0Test, EnvNewHasFormalsNameAndParent) {
  const hldb::Function *const ctor = getEnvNew();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 2u);

  const hldb::IODecl *const name = ctor->getIODecls()->at(0);
  ASSERT_NE(name, nullptr);
  EXPECT_EQ(name->getName(), "name");
  EXPECT_EQ(name->getDirection(), vpiInput);
  ASSERT_NE(name->getTypespec(), nullptr);
  EXPECT_NE(name->getTypespec()->getActual<hldb::StringTypespec>(), nullptr) << "6.16: 'name' is declared 'string'";

  const hldb::IODecl *const parent = ctor->getIODecls()->at(1);
  ASSERT_NE(parent, nullptr);
  EXPECT_EQ(parent->getName(), "parent");
  EXPECT_EQ(parent->getDirection(), vpiInput);
  ExpectClassHandle(parent->getTypespec(), getUvmClass("uvm_component"), "'parent'");
}

TEST_F(Srandom0Test, EnvNewBodyIsSuperNewOfNameAndParent) {
  const hldb::Function *const ctor = getEnvNew();
  ASSERT_NE(ctor, nullptr);
  const hldb::RefObj *const path = ctor->getStmt<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'super.new(name, parent);' is the only statement, so it is not wrapped in a Begin";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const superRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(superRef, nullptr);
  EXPECT_EQ(superRef->getName(), "super");
  ASSERT_NE(getUvmClass("uvm_env"), nullptr);
  EXPECT_EQ(superRef->getActual<hldb::ClassDefn>(), getUvmClass("uvm_env"))
      << "8.15: 'super' is the base class uvm_env";

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'super.new(...)' should end in a MethodFuncCall";
  EXPECT_EQ(call->getName(), "new");
  ASSERT_NE(getUvmEnvNew(), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getUvmEnvNew()) << "8.15: 'super.new' calls uvm_env's own constructor";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  ExpectBoundRef(call->getArguments()->at(0), "name", hldb::findByName<hldb::IODecl>("name", ctor->getIODecls()));
  ExpectBoundRef(call->getArguments()->at(1), "parent", hldb::findByName<hldb::IODecl>("parent", ctor->getIODecls()));
}

// ---------------------------------------------------------------------------
// task run_phase(uvm_phase phase); ... endtask: run_phase
// ---------------------------------------------------------------------------

TEST_F(Srandom0Test, RunPhaseHasOneUvmPhaseFormalAndEndLabel) {
  const hldb::Task *const task = getRunPhase();
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  ASSERT_EQ(task->getIODecls()->size(), 1u);
  const hldb::IODecl *const phase = task->getIODecls()->at(0);
  ASSERT_NE(phase, nullptr);
  EXPECT_EQ(phase->getName(), "phase");
  EXPECT_EQ(phase->getDirection(), vpiInput) << "13.3: 'phase' writes no direction, so it is an input";
  ExpectClassHandle(phase->getTypespec(), getUvmClass("uvm_phase"), "'phase'");
  ASSERT_NE(task->getEndLabelObj(), nullptr) << "'endtask: run_phase' carries an end label";
  EXPECT_EQ(task->getEndLabelObj()->getName(), "run_phase");
}

TEST_F(Srandom0Test, RunPhaseBodyIsRaiseBlockDrop) {
  const hldb::Task *const task = getRunPhase();
  ASSERT_NE(task, nullptr);
  const hldb::Begin *const body = task->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a body with more than one statement should be wrapped in a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u) << "raise_objection, begin-end, drop_objection";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(0)), nullptr);
  EXPECT_NE(any_cast<hldb::Begin>(body->getStmts()->at(1)), nullptr) << "line 35 opens an explicit begin-end";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(2)), nullptr);
}

TEST_F(Srandom0Test, RunPhaseRaisesObjectionOnThis) { ExpectObjectionCall(getRunPhaseStmt(0), "raise_objection"); }

TEST_F(Srandom0Test, RunPhaseDropsObjectionOnThis) { ExpectObjectionCall(getRunPhaseStmt(2), "drop_objection"); }

TEST_F(Srandom0Test, InnerBlockHoldsSixStatementsInOrder) {
  const hldb::Begin *const block = getInnerBlock();
  ASSERT_NE(block, nullptr);
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 6u) << "lines 36-41; the blank line 46 is not a statement";
  EXPECT_NE(any_cast<hldb::RefObj>(getInnerStmt(0)), nullptr) << "statement 0 is 'obj.srandom(seed);'";
  EXPECT_NE(any_cast<hldb::Assignment>(getInnerStmt(1)), nullptr) << "statement 1 is 'ret1 = obj.randomize();'";
  EXPECT_NE(any_cast<hldb::Assignment>(getInnerStmt(2)), nullptr) << "statement 2 is 'prev_x = obj.x;'";
  EXPECT_NE(any_cast<hldb::RefObj>(getInnerStmt(3)), nullptr) << "statement 3 is 'obj.srandom(seed);'";
  EXPECT_NE(any_cast<hldb::Assignment>(getInnerStmt(4)), nullptr) << "statement 4 is 'ret2 = obj.randomize();'";
  EXPECT_NE(getIfElse(), nullptr) << "12.4: statement 5 is an if with an else, so it is an IfElse";
}

// ---------------------------------------------------------------------------
// obj.srandom(seed); ret1 = obj.randomize(); prev_x = obj.x;
// obj.srandom(seed); ret2 = obj.randomize();
// ---------------------------------------------------------------------------

TEST_F(Srandom0Test, FirstSrandomSeedsObjWithSeed) { ExpectSrandomOfSeed(getInnerStmt(0)); }

TEST_F(Srandom0Test, Ret1IsAssignedObjRandomize) { ExpectRandomizeInto(getInnerStmt(1), "ret1"); }

TEST_F(Srandom0Test, PrevXIsAssignedObjX) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(getInnerStmt(2));
  ASSERT_NE(assign, nullptr) << "'prev_x = obj.x;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "prev_x", getProperty("prev_x"));
  ExpectObjX(assign->getRhs());
}

TEST_F(Srandom0Test, SecondSrandomSeedsObjWithSeed) { ExpectSrandomOfSeed(getInnerStmt(3)); }

TEST_F(Srandom0Test, Ret2IsAssignedObjRandomize) { ExpectRandomizeInto(getInnerStmt(4), "ret2"); }

// ---------------------------------------------------------------------------
// if(ret1 == 1 && ret2 == 1 && obj.x > 0 && obj.x < 30 && prev_x == obj.x)
// ---------------------------------------------------------------------------

TEST_F(Srandom0Test, IfConditionIsFiveConjunctsInSourceOrder) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const std::vector<const hldb::Any *> conjuncts = getIfConjuncts();
  ASSERT_EQ(conjuncts.size(), 5u) << "11.3.2: four '&&' associate left to right, (((A && B) && C) && D) && E";
  const std::vector<int32_t> expected = {vpiEqOp, vpiEqOp, vpiGtOp, vpiLtOp, vpiEqOp};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(conjuncts[i]);
    ASSERT_NE(op, nullptr) << "conjunct " << i << " should be a comparison Operation";
    EXPECT_EQ(op->getOpType(), expected[i]) << "conjunct " << i;
  }
}

TEST_F(Srandom0Test, ConditionChecksBothRandomizeResultsAreOne) {
  const std::vector<const hldb::Any *> conjuncts = getIfConjuncts();
  ASSERT_EQ(conjuncts.size(), 5u);
  const hldb::Any *lhs = nullptr;
  const hldb::Any *rhs = nullptr;
  ExpectBinaryOp(conjuncts[0], vpiEqOp, "ret1 == 1", &lhs, &rhs);
  ExpectBoundRef(lhs, "ret1", getProperty("ret1"));
  ExpectIntLiteral(rhs, "1");
  ExpectBinaryOp(conjuncts[1], vpiEqOp, "ret2 == 1", &lhs, &rhs);
  ExpectBoundRef(lhs, "ret2", getProperty("ret2"));
  ExpectIntLiteral(rhs, "1");
}

TEST_F(Srandom0Test, ConditionRestatesTheConstraintOnObjX) {
  const std::vector<const hldb::Any *> conjuncts = getIfConjuncts();
  ASSERT_EQ(conjuncts.size(), 5u);
  const hldb::Any *lhs = nullptr;
  const hldb::Any *rhs = nullptr;
  ExpectBinaryOp(conjuncts[2], vpiGtOp, "obj.x > 0", &lhs, &rhs);
  ExpectObjX(lhs);
  ExpectIntLiteral(rhs, "0");
  ExpectBinaryOp(conjuncts[3], vpiLtOp, "obj.x < 30", &lhs, &rhs);
  ExpectObjX(lhs);
  ExpectIntLiteral(rhs, "30");
}

TEST_F(Srandom0Test, ConditionComparesPrevXWithObjX) {
  const std::vector<const hldb::Any *> conjuncts = getIfConjuncts();
  ASSERT_EQ(conjuncts.size(), 5u);
  const hldb::Any *lhs = nullptr;
  const hldb::Any *rhs = nullptr;
  ExpectBinaryOp(conjuncts[4], vpiEqOp, "prev_x == obj.x", &lhs, &rhs);
  ExpectBoundRef(lhs, "prev_x", getProperty("prev_x"));
  ExpectObjX(rhs);
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (wildcard
// import resolves types only) in the file header.
TEST_F(Srandom0Test, ThenBranchIsUvmInfoExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getStmt(), "uvm_report_info", "UVM_INFO", "UVM_LOW",
                       "\"prev_x = %0d obj.x = %0d SUCCESS\"");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (wildcard
// import resolves types only) in the file header.
TEST_F(Srandom0Test, ElseBranchIsUvmErrorExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getElseStmt(), "uvm_report_error", "UVM_ERROR", "UVM_NONE",
                       "\"prev_x = %0d obj.x = %0d FAILED\"");
}

// ---------------------------------------------------------------------------
// module top; env environment; initial begin ... end endmodule
// ---------------------------------------------------------------------------

TEST_F(Srandom0Test, TopHasNoPortsAndOneEnvHandle) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top;' has no port list";
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "'env environment;' is the module's only declaration";
  const hldb::Variable *const environment = getEnvironment();
  ASSERT_NE(environment, nullptr) << "a class-typed data declaration is a variable (6.8)";
  ExpectClassHandle(environment->getTypespec(), getEnv(), "'environment'");
}

TEST_F(Srandom0Test, TopHasOneInitialWithTwoStatements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = getInitial();
  ASSERT_NE(init, nullptr) << "9.2.1: the only process is an initial procedure";
  const hldb::Begin *const block = init->getStmt<hldb::Begin>();
  ASSERT_NE(block, nullptr);
  ASSERT_NE(block->getStmts(), nullptr);
  EXPECT_EQ(block->getStmts()->size(), 2u);
}

TEST_F(Srandom0Test, InitialConstructsEnvironment) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(getInitialStmt(0));
  ASSERT_NE(assign, nullptr) << "statement 0 is 'environment = new(\"env\");'";
  EXPECT_TRUE(assign->getBlocking());
  ExpectBoundRef(assign->getLhs(), "environment", getEnvironment());
  const hldb::MethodFuncCall *const call = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(call, nullptr) << "8.7: 'new(...)' is a constructor call";
  EXPECT_EQ(call->getName(), "new");
  ASSERT_NE(getEnvNew(), nullptr);
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getEnvNew())
      << "8.7: the target 'environment' has type env, so 'new' is env's constructor";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "only 'name' is written; 'parent' takes its default";
  ExpectStringLiteral(call->getArguments()->at(0), "\"env\"");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (wildcard
// import resolves types only) in the file header.
TEST_F(Srandom0Test, InitialCallsRunTestFromUvmPkg) {
  const hldb::TaskCall *const call = any_cast<hldb::TaskCall>(getInitialStmt(1));
  ASSERT_NE(call, nullptr) << "statement 1 'run_test();' calls a task, so it is a TaskCall";
  EXPECT_EQ(call->getName(), "run_test");
  ASSERT_NE(getRunTest(), nullptr);
  EXPECT_EQ(call->getTaskFunc<hldb::Task>(), getRunTest()) << "26.3: run_test is imported from uvm_pkg";
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "'run_test()' writes no argument";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(Srandom0Test, EveryDeclaredVariableAndFormalIsFound) {
  for (std::string_view name :
       {"x", "obj", "ret1", "ret2", "prev_x", "seed", "name", "parent", "phase", "environment"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "'" << name << "' is declared in this file, so it is not undefined";
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (wildcard
// import resolves types only) for 'run_test', and KNOWN COMPILER BUG (UVM
// library does not compile cleanly) for 'randomize' and 'new', in the file
// header.
TEST_F(Srandom0Test, EveryCalledOrExtendedNameBinds) {
  for (std::string_view name : {"srandom", "randomize", "x", "raise_objection", "drop_objection", "run_test", "new",
                                "uvm_env", "uvm_component", "uvm_phase", "a", "env"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "'" << name << "' is declared in this file, in uvm_pkg, or built in to every class, so it must bind";
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (UVM library
// does not compile cleanly) in the file header.
TEST_F(Srandom0Test, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the ';' after the constraint block is a legal empty class item (A.1.9)";
  EXPECT_EQ(stats.nbError, 0) << "the file, and the UVM library it uses, are legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
