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
// tests/Google/chapter-18/18.13.2--urandom_range_2.sv. Lines 1-14 are the
// license and the suite's :name:/:description:/:tags: comment; the rest is:
//
//   16  import uvm_pkg::*;
//   17  `include "uvm_macros.svh"
//   18
//   19  class a;
//   20      function int unsigned do_urandom_range(int unsigned maxval, int unsigned minval);
//   21          int unsigned val;
//   22          val = $urandom_range(maxval, minval);
//   23          return val;
//   24      endfunction
//   25  endclass
//   26
//   27  class env extends uvm_env;
//   28
//   29    a obj = new;
//   30    int unsigned max = 10, min = 1, ret;
//   31
//   32    function new(string name, uvm_component parent = null);
//   33      super.new(name, parent);
//   34    endfunction
//   35
//   36    task run_phase(uvm_phase phase);
//   37      phase.raise_objection(this);
//   38      begin
//   39        /* If max is less than min, then arguments should be automatically reversed */
//   40        ret = obj.do_urandom_range(min, max);
//   41        if(ret >= min && ret <= max) begin
//   42          `uvm_info("RESULT", $sformatf("ret = %0d SUCCESS", ret), UVM_LOW);
//   43        end else begin
//   44          `uvm_error("RESULT", $sformatf("ret = %0d FAILED", ret));
//   45        end
//   46      end
//   47      phase.drop_objection(this);
//   48    endtask: run_phase
//   49
//   50  endclass
//   51
//   52  module top;
//   53
//   54    env environment;
//   55
//   56    initial begin
//   57      environment = new("env");
//   58      run_test();
//   59    end
//   60
//   61  endmodule
//
// The point of the fixture is a $urandom_range() call (IEEE 1800-2023
// 18.13.2) wrapped in a class method, driven from a UVM test. Unlike its
// sibling urandom_range_1, the env calls the method with (min, max), so the
// SMALLER value (min = 1) lands in the 'maxval' position and the larger
// (max = 10) in 'minval'. 18.13.2 says $urandom_range swaps its arguments
// when maxval is less than minval, which is what the comment on line 39
// refers to. That swap happens when the call executes; what is static, and
// asserted here, is which actual sits in which argument position.
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
//   class a (8.3) -- same shape as urandom_range_0, except the return type
//     - exactly one class named "a"; user-defined; no 'extends' (8.13), no
//       parameters (8.25), no end label; no properties (8.5)
//     - exactly one declared method, the Function do_urandom_range: a class
//       method, public (8.18), not virtual (8.20), automatic (8.6), no end
//       label
//     - return type 'int unsigned': an IntTypespec that is NOT signed
//       (6.11.3)
//     - formals, in order: 'maxval' then 'minval', each an input (13.4) of
//       type 'int unsigned'
//     - one local, 'val': unsigned int, no initializer, automatic (6.21)
//     - body executes an Assignment then a ReturnStmt
//     - 'val = $urandom_range(maxval, minval);' is a blocking Assignment
//       (10.4.1) to 'val' whose RHS is the SysFuncCall "$urandom_range" with
//       2 arguments, maxval then minval, each bound to the method's formal
//     - 'return val;' returns a RefObj bound to 'val' (12.8, 13.4.1)
//   class env extends uvm_env (8.3, 8.13)
//     - exactly one class named "env"; user-defined; not virtual; no
//       parameters; no end label
//     - its Extends holds exactly one class typespec, "uvm_env", resolving
//       to uvm_pkg's own uvm_env class
//     - exactly 4 properties, in declaration order: obj, max, min, ret.
//       Each is public (8.18), not rand/randc (18.4) and not const (8.19).
//     - 'a obj = new;' -- obj is a handle to class a whose initializer is a
//       'new' call with no arguments (8.7)
//     - 'int unsigned max = 10, min = 1, ret;' -- all three are unsigned
//       ints; max is initialized to 10, min to 1, ret has no initializer
//     - exactly 2 methods: the Function 'new' and the Task 'run_phase'
//   function new(string name, uvm_component parent = null); (8.7)
//     - a Function flagged as a class method
//     - formals, in order: 'name' (input, string, 6.16) and 'parent'
//       (input, a handle to uvm_pkg's uvm_component)
//     - the body is the single statement 'super.new(name, parent);' (8.15):
//       a hierarchical RefObj whose prefix 'super' binds to uvm_env, and
//       whose MethodFuncCall 'new' binds to uvm_env's own constructor. Its
//       2 arguments, name then parent, bind to THIS constructor's formals.
//   task run_phase(uvm_phase phase); ... endtask: run_phase (13.3)
//     - a Task flagged as a class method; public; the end label 'run_phase'
//     - one formal, 'phase': an input, a handle to uvm_pkg's uvm_phase
//     - the body executes 3 statements, in order: phase.raise_objection
//       (this), a begin-end, and phase.drop_objection(this)
//     - each objection call is a hierarchical RefObj: prefix 'phase' bound
//       to the formal, then a MethodFuncCall of that name that resolves to
//       a Function of the same name, with exactly one argument, 'this',
//       bound to class env (8.11)
//   begin ... end (line 38)
//     - exactly 2 statements: an Assignment, then an IfElse. The block
//       comment on line 39 is not a statement.
//   ret = obj.do_urandom_range(min, max);
//     - blocking Assignment whose LHS is bound to env's property 'ret'
//     - RHS is a hierarchical RefObj: prefix 'obj' bound to env's property,
//       then a MethodFuncCall 'do_urandom_range' that resolves by object
//       identity to class a's method
//     - exactly 2 arguments, min then max, bound to env's properties.
//       Arguments bind by position (13.5), so min feeds 'maxval' and max
//       feeds 'minval' -- the reverse of urandom_range_1.
//   if (ret >= min && ret <= max) begin ... end else begin ... end (12.4)
//     - an IfElse. Its condition is vpiLogAndOp over vpiGeOp(ret, min) and
//       vpiLeOp(ret, max): relational operators bind tighter than '&&'
//       (11.3.2). Every operand is bound to env's properties.
//     - both branches are explicit begin-end blocks
//   `uvm_info("RESULT", $sformatf("ret = %0d SUCCESS", ret), UVM_LOW);
//     - the then-branch starts with the macro's own begin-end, holding
//       exactly one statement: an IfStmt (no else)
//     - its condition is the call uvm_report_enabled(UVM_LOW, UVM_INFO,
//       "RESULT"), exactly 3 arguments
//     - its action is the call uvm_report_info whose first 3 arguments are
//       "RESULT", $sformatf("ret = %0d SUCCESS", ret) and UVM_LOW
//     - $sformatf is a SysFuncCall with 2 arguments: the string literal
//       (vpiStringConst) and a RefObj bound to env's 'ret'
//     - every UVM_* name binds to an EnumConst of the same name
//   `uvm_error("RESULT", $sformatf("ret = %0d FAILED", ret));
//     - the else-branch has the same shape, with
//       uvm_report_enabled(UVM_NONE, UVM_ERROR, "RESULT") and
//       uvm_report_error("RESULT", $sformatf("ret = %0d FAILED", ret),
//       UVM_NONE, ...)
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
//       declares, and no COMP_FAILED_TO_BIND for any name it calls or
//       extends
//     - no COMP_ILLEGAL_RETURN_VALUE for do_urandom_range
//     - the file is legal: zero fatal, syntax and error diagnostics. See
//       KNOWN COMPILER BUG (UVM library does not compile cleanly) below.
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no parameter, no constant expression beyond the literal property
// initializers (already constants), no module instance and no class
// specialization. Every binding asserted is name resolution. No check is
// gated on getElaborated().
//
// KNOWN COMPILER BUG (class-method lifetime), not a defect in this test:
// IEEE 1800-2023 8.6 says "The lifetime of methods declared as part of a
// class type shall be automatic." HLC instead gives do_urandom_range the
// static default that 13.4.2 applies to subroutines declared outside a
// class, so Function::getAutomatic() returns false. The local 'val' takes
// its lifetime from the method (6.21), so Variable::getAutomatic() is false
// as well. getAutomatic() is the lifetime after defaults are applied (HLDB
// model gap #8), so false is a wrong value, not a different meaning of the
// flag. DoUrandomRangeIsPublicNonVirtualAutomaticMethod and
// DoUrandomRangeDeclaresOnlyValAsAnAutomaticUnsignedInt assert the LRM value
// and fail until HLC is fixed; they are intentionally not skipped or
// relaxed.
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
// Actual" references HLC leaves unresolved inside the library. Separately,
// this fixture's .hlc passes -nobuiltin, which leaves out built-in classes
// UVM relies on (for example mailbox) and adds about 270 more errors; that
// is a consequence of the option, not of this bug, and again none is in
// this file. NoFatalSyntaxOrErrorDiagnostics asserts zero errors, as legal
// source requires, and is expected to fail until HLC is fixed. The same
// unresolved library references make the 'new' entry of
// EveryCalledOrExtendedNameBinds fail: findError() searches the whole
// design, and every failed bind it finds for 'new' is in UVM's own source,
// not in this file. Neither is skipped or relaxed.
//
// What is NOT checked, and why:
//   - The value $urandom_range returns, the swap of its arguments when
//     maxval (here min = 1) is less than minval (here max = 10), whether the
//     result lies in [min, max], which branch of the if executes, the text
//     the report prints (including '%0d' formatting), objection counting,
//     phase scheduling and which test run_test starts all only exist while
//     simulation runs. Permanently out of scope. The static half is covered
//     by: UrandomRangeArgumentsAreMaxvalThenMinval,
//     RetAssignmentCallsDoUrandomRange, DoUrandomRangeArgumentsAreMinThenMax,
//     IfConditionIsRetWithinMinAndMax, ThenBranchIsUvmInfoExpansion,
//     ElseBranchIsUvmErrorExpansion and InitialCallsRunTestFromUvmPkg.
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
//   - Which scope owns 'val', and whether the implicit variable named after
//     do_urandom_range (13.4.1) is materialized, are tool conventions (see
//     urandom_range_0).
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
//   - The license and metadata comments, and the block comment on line 39,
//     are comments, not design objects.

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
#include <hldb/return_stmt.h>
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

#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class UrandomRange2Test : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "18.13.2--urandom_range_2.hlc"});
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

  static const hldb::Function *getDoUrandomRange() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("do_urandom_range", cls->getMethods());
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

  static const hldb::IODecl *getFormal(std::string_view name) {
    const hldb::Function *const fn = getDoUrandomRange();
    if (fn == nullptr) return nullptr;
    return hldb::findByName<hldb::IODecl>(name, fn->getIODecls());
  }

  static const hldb::Begin *getDoUrandomRangeBody() {
    const hldb::Function *const fn = getDoUrandomRange();
    if (fn == nullptr) return nullptr;
    return fn->getStmt<hldb::Begin>();
  }

  // 'int unsigned val;' may be owned by the Function itself or by the Begin
  // wrapping its body. Which scope owns it is a tool convention, so both are
  // searched.
  static const hldb::Variable *getVal() {
    const hldb::Function *const fn = getDoUrandomRange();
    if (fn == nullptr) return nullptr;
    if (const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", fn->getVariables())) return val;
    const hldb::Begin *const body = getDoUrandomRangeBody();
    if (body == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("val", body->getVariables());
  }

  // Names of the locals declared across both candidate scopes. 13.4.1 also
  // declares an implicit variable named after the function; whether HLC
  // materializes it is a tool convention, so it is left out.
  static std::vector<std::string> getDeclaredLocalNames() {
    std::vector<std::string> names;
    const hldb::Function *const fn = getDoUrandomRange();
    if (fn == nullptr) return names;
    if (fn->getVariables() != nullptr) {
      for (const hldb::Variable *const var : *fn->getVariables()) {
        if (var->getName() != fn->getName()) names.emplace_back(var->getName());
      }
    }
    const hldb::Begin *const body = getDoUrandomRangeBody();
    if (body != nullptr && body->getVariables() != nullptr) {
      for (const hldb::Variable *const var : *body->getVariables()) {
        if (var->getName() != fn->getName()) names.emplace_back(var->getName());
      }
    }
    return names;
  }

  // do_urandom_range's body statements with bare Variable declarations
  // removed.
  static std::vector<const hldb::Any *> getDoUrandomRangeStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Begin *const body = getDoUrandomRangeBody();
    if (body == nullptr || body->getStmts() == nullptr) return stmts;
    for (const hldb::Any *const stmt : *body->getStmts()) {
      if (any_cast<hldb::Variable>(stmt) == nullptr) stmts.emplace_back(stmt);
    }
    return stmts;
  }

  static const hldb::Assignment *getValAssign() {
    const std::vector<const hldb::Any *> stmts = getDoUrandomRangeStmts();
    if (stmts.empty()) return nullptr;
    return any_cast<hldb::Assignment>(stmts[0]);
  }

  static const hldb::ReturnStmt *getReturnStmt() {
    const std::vector<const hldb::Any *> stmts = getDoUrandomRangeStmts();
    if (stmts.size() < 2) return nullptr;
    return any_cast<hldb::ReturnStmt>(stmts[1]);
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

  // The explicit 'begin ... end' on line 38.
  static const hldb::Begin *getInnerBlock() { return any_cast<hldb::Begin>(getRunPhaseStmt(1)); }

  static const hldb::Any *getInnerStmt(size_t index) {
    const hldb::Begin *const block = getInnerBlock();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() <= index) return nullptr;
    return block->getStmts()->at(index);
  }

  static const hldb::Assignment *getRetAssign() { return any_cast<hldb::Assignment>(getInnerStmt(0)); }

  static const hldb::IfElse *getIfElse() { return any_cast<hldb::IfElse>(getInnerStmt(1)); }

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
      if (sf->getName().find("18.13.2--urandom_range_2.sv") != std::string_view::npos) return sf;
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

  // Verifies 'type' is 'int unsigned': an IntTypespec that is not signed.
  static void ExpectUnsignedInt(const hldb::RefTypespec *type, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::IntTypespec *const ts = type->getActual<hldb::IntTypespec>();
    ASSERT_NE(ts, nullptr) << what << " is declared 'int unsigned'";
    EXPECT_FALSE(ts->getSigned()) << "6.11.3: the 'unsigned' keyword makes " << what << " unsigned";
  }

  // Verifies 'type' is a class handle type resolving to 'cls'.
  static void ExpectClassHandle(const hldb::RefTypespec *type, const hldb::ClassDefn *cls, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::ClassTypespec *ct = nullptr;
    if (const hldb::TypedefTypespec *const tt = type->getActual<hldb::TypedefTypespec>()) {
      if (const hldb::Typedef *const t = tt->getTypedef()) {
        if (const hldb::RefTypespec *const rt = t->getAlias()) {
          ct = rt->getActual<hldb::ClassTypespec>();
        }
      }
    } else {
      ct = type->getActual<hldb::ClassTypespec>();
    }
    ASSERT_NE(ct, nullptr) << what << " is declared with a class type";
    ASSERT_NE(cls, nullptr) << "the class " << what << " refers to was not found";
    EXPECT_EQ(ct->getClassDefn(), cls) << what << " must resolve to its class";
  }

  // Verifies 'formal' is the input 'name' of type 'int unsigned'.
  static void ExpectUnsignedIntInput(const hldb::IODecl *formal, std::string_view name) {
    ASSERT_NE(formal, nullptr) << "formal '" << name << "' not found";
    EXPECT_EQ(formal->getName(), name);
    EXPECT_EQ(formal->getDirection(), vpiInput) << "13.4: '" << name << "' writes no direction, so it is an input";
    ExpectUnsignedInt(formal->getTypespec(), name);
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
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    const hldb::RefObj *const self = arg0->getHighConn<hldb::RefObj>();
    ASSERT_NE(self, nullptr) << "'this' should be a RefObj";
    EXPECT_EQ(self->getName(), "this");
    ASSERT_NE(getEnv(), nullptr);
    EXPECT_EQ(self->getActual<hldb::ClassDefn>(), getEnv()) << "8.11: 'this' is the enclosing class env";
  }

  // Verifies 'expr' is '$sformatf(<format>, ret)' with 'ret' bound to env's
  // property.
  static void ExpectSformatfOfRet(const hldb::Any *expr, std::string_view format) {
    const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(expr);
    ASSERT_NE(call, nullptr) << "the message argument should be a $sformatf SysFuncCall";
    EXPECT_EQ(call->getName(), "$sformatf");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 2u) << "a format string and 'ret'";
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    ExpectStringLiteral(arg0->getHighConn(), format);
    const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
    ASSERT_NE(arg1, nullptr);
    ExpectBoundRef(arg1->getHighConn(), "ret", getProperty("ret"));
  }

  // Verifies 'branch' is a begin-end starting with the expansion of a UVM
  // report macro:
  //   begin
  //     if (uvm_report_enabled(<verbosity>, <severity>, "RESULT"))
  //       <reportFn>("RESULT", $sformatf(<format>, ret), <verbosity>, ...);
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
    const hldb::NamedArgument *const enabledArg0 = enabled->getArguments()->at(0);
    ASSERT_NE(enabledArg0, nullptr);
    ExpectEnumConstRef(enabledArg0->getHighConn(), verbosity);
    const hldb::NamedArgument *const enabledArg1 = enabled->getArguments()->at(1);
    ASSERT_NE(enabledArg1, nullptr);
    ExpectEnumConstRef(enabledArg1->getHighConn(), severity);
    const hldb::NamedArgument *const arg2 = enabled->getArguments()->at(2);
    ASSERT_NE(arg2, nullptr);
    ExpectStringLiteral(arg2->getHighConn(), "\"RESULT\"");

    const hldb::TFCall *const report = guard->getStmt<hldb::TFCall>();
    ASSERT_NE(report, nullptr) << "the if action is a call";
    EXPECT_EQ(report->getName(), reportFn);
    ASSERT_NE(report->getArguments(), nullptr);
    ASSERT_GE(report->getArguments()->size(), 3u);
    const hldb::NamedArgument *const reportArg0 = report->getArguments()->at(0);
    ASSERT_NE(reportArg0, nullptr);
    ExpectStringLiteral(reportArg0->getHighConn(), "\"RESULT\"");
    const hldb::NamedArgument *const reportArg1 = report->getArguments()->at(1);
    ASSERT_NE(reportArg1, nullptr);
    ExpectSformatfOfRet(reportArg1->getHighConn(), format);
    const hldb::NamedArgument *const reportArg2 = report->getArguments()->at(2);
    ASSERT_NE(reportArg2, nullptr);
    ExpectEnumConstRef(reportArg2->getHighConn(), verbosity);
  }
};

// ---------------------------------------------------------------------------
// import uvm_pkg::*;  `include "uvm_macros.svh"
// ---------------------------------------------------------------------------

TEST_F(UrandomRange2Test, SourceFileIncludesUvmMacros) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr) << "18.13.2--urandom_range_2.sv not found in Design::getSourceFiles()";
  ASSERT_NE(sf->getIncludes(), nullptr) << "22.4: the file includes uvm_macros.svh";
  size_t count = 0;
  for (const hldb::SourceFile *const inc : *sf->getIncludes()) {
    if (inc->getName().find("uvm_macros.svh") != std::string_view::npos) ++count;
  }
  EXPECT_EQ(count, 1u) << "'`include \"uvm_macros.svh\"' is written once";
}

TEST_F(UrandomRange2Test, FileUsesUvmInfoOnceAndUvmErrorOnce) {
  const std::vector<const hldb::PreprocMacroInstance *> infos = getMacroInstances("uvm_info");
  ASSERT_EQ(infos.size(), 1u);
  const std::vector<const hldb::PreprocMacroInstance *> errors = getMacroInstances("uvm_error");
  ASSERT_EQ(errors.size(), 1u);
}

TEST_F(UrandomRange2Test, UvmPkgProvidesTheNamesTheFileUses) {
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
// class a; ... endclass
// ---------------------------------------------------------------------------

TEST_F(UrandomRange2Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
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

TEST_F(UrandomRange2Test, ClassAHasNoBaseClassParametersPropertiesOrEndLabel) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getName(), "a");
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_TRUE(cls->getVariables() == nullptr || cls->getVariables()->empty())
      << "8.5: nothing is declared directly in the class body; 'val' is local to the method (13.4)";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
}

TEST_F(UrandomRange2Test, ClassAHasExactlyOneDeclaredMethodDoUrandomRange) {
  const std::vector<const hldb::TaskFunc *> methods = getClassADeclaredMethods();
  ASSERT_EQ(methods.size(), 1u) << "the class body declares exactly one method";
  EXPECT_EQ(methods[0]->getName(), "do_urandom_range");
  const hldb::Function *const fn = any_cast<hldb::Function>(methods[0]);
  ASSERT_NE(fn, nullptr) << "13.4: declared with 'function', so it is a Function, not a Task";
  EXPECT_EQ(fn, getDoUrandomRange());
  EXPECT_EQ(fn->getEndLabelObj(), nullptr) << "'endfunction' is written without ': do_urandom_range'";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (class-method
// lifetime) in the file header.
TEST_F(UrandomRange2Test, DoUrandomRangeIsPublicNonVirtualAutomaticMethod) {
  const hldb::Function *const fn = getDoUrandomRange();
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getMethod()) << "8.6: 'do_urandom_range' is declared in the body of class 'a'";
  EXPECT_EQ(fn->getVisibility(), vpiPublicVis) << "8.18: a member with no 'local' or 'protected' qualifier is public";
  EXPECT_FALSE(fn->getVirtual()) << "8.20: no 'virtual' qualifier is written and class a has no base class";
  EXPECT_TRUE(fn->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
}

TEST_F(UrandomRange2Test, DoUrandomRangeReturnsUnsignedInt) {
  const hldb::Function *const fn = getDoUrandomRange();
  ASSERT_NE(fn, nullptr);
  ExpectUnsignedInt(fn->getReturn(), "the return type");
}

TEST_F(UrandomRange2Test, DoUrandomRangeHasFormalsMaxvalThenMinval) {
  const hldb::Function *const fn = getDoUrandomRange();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 2u);
  EXPECT_EQ(fn->getIODecls()->at(0)->getName(), "maxval");
  EXPECT_EQ(fn->getIODecls()->at(1)->getName(), "minval");
  ExpectUnsignedIntInput(getFormal("maxval"), "maxval");
  ExpectUnsignedIntInput(getFormal("minval"), "minval");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (class-method
// lifetime) in the file header.
TEST_F(UrandomRange2Test, DoUrandomRangeDeclaresOnlyValAsAnAutomaticUnsignedInt) {
  ASSERT_NE(getDoUrandomRange(), nullptr);
  EXPECT_EQ(getDeclaredLocalNames(), std::vector<std::string>{"val"})
      << "'int unsigned val;' is the only declaration in the method body";
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr);
  ExpectUnsignedInt(val->getTypespec(), "'val'");
  EXPECT_EQ(val->getValue(), nullptr) << "'int unsigned val;' has no initializer";
  EXPECT_TRUE(val->getAutomatic()) << "6.21: a variable declared in an automatic method is automatic";
}

TEST_F(UrandomRange2Test, DoUrandomRangeBodyExecutesAssignmentThenReturn) {
  ASSERT_NE(getDoUrandomRangeBody(), nullptr) << "a body with more than one statement should be wrapped in a Begin";
  const std::vector<const hldb::Any *> stmts = getDoUrandomRangeStmts();
  ASSERT_EQ(stmts.size(), 2u) << "the body executes exactly two statements";
  EXPECT_NE(any_cast<hldb::Assignment>(stmts[0]), nullptr) << "statement 0 is 'val = $urandom_range(...);'";
  EXPECT_NE(any_cast<hldb::ReturnStmt>(stmts[1]), nullptr) << "statement 1 is 'return val;'";
}

TEST_F(UrandomRange2Test, ValIsAssignedUrandomRangeResult) {
  const hldb::Assignment *const assign = getValAssign();
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "val", getVal());
  const hldb::SysFuncCall *const call = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr) << "18.13.2: $urandom_range is a system function and its value is assigned, so the "
                              "RHS is a SysFuncCall";
  EXPECT_EQ(call->getName(), "$urandom_range");
}

TEST_F(UrandomRange2Test, UrandomRangeArgumentsAreMaxvalThenMinval) {
  const hldb::Assignment *const assign = getValAssign();
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const call = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "both arguments are written; minval's default of 0 is not used";
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  ExpectBoundRef(arg0->getHighConn(), "maxval", getFormal("maxval"));
  const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  ExpectBoundRef(arg1->getHighConn(), "minval", getFormal("minval"));
}

TEST_F(UrandomRange2Test, DoUrandomRangeReturnsVal) {
  const hldb::ReturnStmt *const ret = getReturnStmt();
  ASSERT_NE(ret, nullptr);
  ExpectBoundRef(ret->getCondition(), "val", getVal());
}

// ---------------------------------------------------------------------------
// class env extends uvm_env; ... endclass
// ---------------------------------------------------------------------------

TEST_F(UrandomRange2Test, ClassEnvIsDeclaredOnceAsUserDefinedClass) {
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

TEST_F(UrandomRange2Test, ClassEnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  const hldb::Extends *const ext = env->getExtends();
  ASSERT_NE(ext, nullptr) << "8.13: 'class env extends uvm_env' names a base class";
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  const hldb::RefTypespec *const base = ext->getClassTypespecs()->at(0);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "uvm_env");
  const hldb::TypedefTypespec *const tt = base->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tt, nullptr);
  const hldb::Typedef *const t = tt->getTypedef();
  ASSERT_NE(t, nullptr);
  const hldb::RefTypespec *const rt = t->getAlias();
  ASSERT_NE(rt, nullptr);
  const hldb::ClassTypespec *const ct = rt->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  ASSERT_NE(getUvmClass("uvm_env"), nullptr);
  EXPECT_EQ(ct->getClassDefn(), getUvmClass("uvm_env")) << "26.3: 'uvm_env' is imported from uvm_pkg";
}

TEST_F(UrandomRange2Test, ClassEnvHasFourPropertiesInDeclarationOrder) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getVariables(), nullptr);
  ASSERT_EQ(env->getVariables()->size(), 4u) << "'a obj' plus 'int unsigned max, min, ret'";
  const std::vector<std::string_view> expected = {"obj", "max", "min", "ret"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = env->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getIsRandomized()) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(UrandomRange2Test, ObjIsClassAHandleInitializedWithNew) {
  const hldb::Variable *const obj = getProperty("obj");
  ASSERT_NE(obj, nullptr);
  ExpectClassHandle(obj->getTypespec(), getClassA(), "'obj'");
  const hldb::MethodFuncCall *const init = obj->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(init, nullptr) << "8.7: 'a obj = new;' initializes obj with a constructor call";
  EXPECT_EQ(init->getName(), "new");
  EXPECT_TRUE(init->getArguments() == nullptr || init->getArguments()->empty()) << "'new' is written with no arguments";
}

TEST_F(UrandomRange2Test, MaxMinRetAreUnsignedIntsWithTheirInitializers) {
  const hldb::Variable *const max = getProperty("max");
  const hldb::Variable *const min = getProperty("min");
  const hldb::Variable *const ret = getProperty("ret");
  ASSERT_NE(max, nullptr);
  ASSERT_NE(min, nullptr);
  ASSERT_NE(ret, nullptr);
  ExpectUnsignedInt(max->getTypespec(), "'max'");
  ExpectUnsignedInt(min->getTypespec(), "'min'");
  ExpectUnsignedInt(ret->getTypespec(), "'ret'");

  const hldb::Constant *const maxInit = max->getValue<hldb::Constant>();
  ASSERT_NE(maxInit, nullptr) << "'max = 10'";
  EXPECT_EQ(maxInit->getDecompile(), "10");
  const hldb::Constant *const minInit = min->getValue<hldb::Constant>();
  ASSERT_NE(minInit, nullptr) << "'min = 1'";
  EXPECT_EQ(minInit->getDecompile(), "1");
  EXPECT_EQ(ret->getValue(), nullptr) << "'ret' is declared with no initializer";
}

TEST_F(UrandomRange2Test, ClassEnvHasMethodsNewAndRunPhase) {
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

// ---------------------------------------------------------------------------
// function new(string name, uvm_component parent = null);
//   super.new(name, parent);
// endfunction
// ---------------------------------------------------------------------------

TEST_F(UrandomRange2Test, EnvNewHasFormalsNameAndParent) {
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

TEST_F(UrandomRange2Test, EnvNewBodyIsSuperNewOfNameAndParent) {
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
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  ExpectBoundRef(arg0->getHighConn(), "name", hldb::findByName<hldb::IODecl>("name", ctor->getIODecls()));
  const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  ExpectBoundRef(arg1->getHighConn(), "parent", hldb::findByName<hldb::IODecl>("parent", ctor->getIODecls()));
}

// ---------------------------------------------------------------------------
// task run_phase(uvm_phase phase); ... endtask: run_phase
// ---------------------------------------------------------------------------

TEST_F(UrandomRange2Test, RunPhaseHasOneUvmPhaseFormalAndEndLabel) {
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

TEST_F(UrandomRange2Test, RunPhaseBodyIsRaiseBlockDrop) {
  const hldb::Task *const task = getRunPhase();
  ASSERT_NE(task, nullptr);
  const hldb::Begin *const body = task->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a body with more than one statement should be wrapped in a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u) << "raise_objection, begin-end, drop_objection";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(0)), nullptr);
  EXPECT_NE(any_cast<hldb::Begin>(body->getStmts()->at(1)), nullptr) << "line 38 opens an explicit begin-end";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(2)), nullptr);
}

TEST_F(UrandomRange2Test, RunPhaseRaisesObjectionOnThis) { ExpectObjectionCall(getRunPhaseStmt(0), "raise_objection"); }

TEST_F(UrandomRange2Test, RunPhaseDropsObjectionOnThis) { ExpectObjectionCall(getRunPhaseStmt(2), "drop_objection"); }

TEST_F(UrandomRange2Test, InnerBlockHoldsAssignmentThenIfElse) {
  const hldb::Begin *const block = getInnerBlock();
  ASSERT_NE(block, nullptr);
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 2u);
  EXPECT_NE(getRetAssign(), nullptr) << "statement 0 is 'ret = obj.do_urandom_range(min, max);' (the line-39 "
                                        "comment is not a statement)";
  EXPECT_NE(getIfElse(), nullptr) << "12.4: statement 1 is an if with an else, so it is an IfElse";
}

// ---------------------------------------------------------------------------
// ret = obj.do_urandom_range(min, max);
// ---------------------------------------------------------------------------

TEST_F(UrandomRange2Test, RetAssignmentCallsDoUrandomRange) {
  const hldb::Assignment *const assign = getRetAssign();
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "ret", getProperty("ret"));

  const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'obj.do_urandom_range(...)' should be a hierarchical RefObj";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  ExpectBoundRef(path->getPathElems()->at(0), "obj", getProperty("obj"));
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "a function called through a class handle is a MethodFuncCall";
  EXPECT_EQ(call->getName(), "do_urandom_range");
  ASSERT_NE(getDoUrandomRange(), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getDoUrandomRange())
      << "'obj' is a handle to class a, so the call binds to a's method";
}

TEST_F(UrandomRange2Test, DoUrandomRangeArgumentsAreMinThenMax) {
  const hldb::Assignment *const assign = getRetAssign();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr);
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "one actual per formal";
  // 13.5: arguments bind by position, so 'min' feeds 'maxval' and 'max'
  // feeds 'minval' -- the reverse of urandom_range_1. The swap 18.13.2
  // applies when maxval < minval happens only when the call executes.
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  ExpectBoundRef(arg0->getHighConn(), "min", getProperty("min"));
  const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  ExpectBoundRef(arg1->getHighConn(), "max", getProperty("max"));
}

// ---------------------------------------------------------------------------
// if(ret >= min && ret <= max) begin `uvm_info(...) end else begin `uvm_error(...) end
// ---------------------------------------------------------------------------

TEST_F(UrandomRange2Test, IfConditionIsRetWithinMinAndMax) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Operation *const land = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(land, nullptr);
  EXPECT_EQ(land->getOpType(), vpiLogAndOp) << "11.3.2: '&&' binds looser than '>=' and '<='";
  ASSERT_NE(land->getOperands(), nullptr);
  ASSERT_EQ(land->getOperands()->size(), 2u);

  const hldb::Operation *const ge = any_cast<hldb::Operation>(land->getOperands()->at(0));
  ASSERT_NE(ge, nullptr) << "'ret >= min' should be an Operation";
  EXPECT_EQ(ge->getOpType(), vpiGeOp);
  ASSERT_NE(ge->getOperands(), nullptr);
  ASSERT_EQ(ge->getOperands()->size(), 2u);
  ExpectBoundRef(ge->getOperands()->at(0), "ret", getProperty("ret"));
  ExpectBoundRef(ge->getOperands()->at(1), "min", getProperty("min"));

  const hldb::Operation *const le = any_cast<hldb::Operation>(land->getOperands()->at(1));
  ASSERT_NE(le, nullptr) << "'ret <= max' should be an Operation";
  EXPECT_EQ(le->getOpType(), vpiLeOp);
  ASSERT_NE(le->getOperands(), nullptr);
  ASSERT_EQ(le->getOperands()->size(), 2u);
  ExpectBoundRef(le->getOperands()->at(0), "ret", getProperty("ret"));
  ExpectBoundRef(le->getOperands()->at(1), "max", getProperty("max"));
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (wildcard
// import resolves types only) in the file header.
TEST_F(UrandomRange2Test, ThenBranchIsUvmInfoExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getStmt(), "uvm_report_info", "UVM_INFO", "UVM_LOW", "\"ret = %0d SUCCESS\"");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (wildcard
// import resolves types only) in the file header.
TEST_F(UrandomRange2Test, ElseBranchIsUvmErrorExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getElseStmt(), "uvm_report_error", "UVM_ERROR", "UVM_NONE", "\"ret = %0d FAILED\"");
}

// ---------------------------------------------------------------------------
// module top; env environment; initial begin ... end endmodule
// ---------------------------------------------------------------------------

TEST_F(UrandomRange2Test, TopHasNoPortsAndOneEnvHandle) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top;' has no port list";
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "'env environment;' is the module's only declaration";
  const hldb::Variable *const environment = getEnvironment();
  ASSERT_NE(environment, nullptr) << "a class-typed data declaration is a variable (6.8)";
  ExpectClassHandle(environment->getTypespec(), getEnv(), "'environment'");
}

TEST_F(UrandomRange2Test, TopHasOneInitialWithTwoStatements) {
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

TEST_F(UrandomRange2Test, InitialConstructsEnvironment) {
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
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  ExpectStringLiteral(arg0->getHighConn(), "\"env\"");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (wildcard
// import resolves types only) in the file header.
TEST_F(UrandomRange2Test, InitialCallsRunTestFromUvmPkg) {
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

TEST_F(UrandomRange2Test, EveryDeclaredVariableAndFormalIsFound) {
  for (std::string_view name :
       {"val", "maxval", "minval", "obj", "max", "min", "ret", "name", "parent", "phase", "environment"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "'" << name << "' is declared in this file, so it is not undefined";
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (wildcard
// import resolves types only) for 'run_test', and KNOWN COMPILER BUG (UVM
// library does not compile cleanly) for 'new', in the file header.
TEST_F(UrandomRange2Test, EveryCalledOrExtendedNameBinds) {
  for (std::string_view name : {"do_urandom_range", "raise_objection", "drop_objection", "run_test", "new", "uvm_env",
                                "uvm_component", "uvm_phase", "a", "env"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "'" << name << "' is declared in this file or in uvm_pkg, so it must bind";
  }
}

TEST_F(UrandomRange2Test, ReturnWithValueInNonVoidFunctionIsNotReported) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_RETURN_VALUE, "do_urandom_range"), nullptr)
      << "12.8, 13.4.1: a function declared to return 'int unsigned' returns with an expression";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
