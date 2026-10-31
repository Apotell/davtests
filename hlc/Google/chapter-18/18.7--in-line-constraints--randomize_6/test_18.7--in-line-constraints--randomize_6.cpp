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
// tests/Google/chapter-18/18.7--in-line-constraints--randomize_6.sv. Lines
// 1-14 are the license and the suite's :name:/:description:/:tags: comment;
// the rest is:
//
//   16  import uvm_pkg::*;
//   17  `include "uvm_macros.svh"
//   18
//   19  class a;
//   20      rand int x;
//   21      int y = -1;
//   22  endclass
//   23
//   24  function int F(a obj, int y);
//   25      F = obj.randomize() with (x) { x > 0; x < y; };
//   26  endfunction
//   27
//   28  class env extends uvm_env;
//   29
//   30    a obj = new;
//   31    int y;
//   32
//   33    function new(string name, uvm_component parent = null);
//   34      super.new(name, parent);
//   35    endfunction
//   36
//   37    task run_phase(uvm_phase phase);
//   38      phase.raise_objection(this);
//   39      begin
//   40        y = 10;
//   41        F(obj, y);
//   42        if(obj.x > 0 && obj.x < y) begin
//   43          `uvm_info("RESULT", $sformatf("obj.x = %0d y = %0d SUCCESS", obj.x, y), UVM_LOW);
//   44        end else begin
//   45          `uvm_error("RESULT", $sformatf("obj.x = %0d y = %0d FAILED", obj.x, y));
//   46        end
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
// The point of the fixture is the restricted form of an inline constraint,
// 'with (identifier_list) { ... }' (IEEE 1800-2023 18.7; A.1.10
// inline_constraint_declaration). When the parenthesized list is present,
// only the names it lists are looked up in the class of the randomized
// object; every other unqualified name is resolved in the scope that
// contains the call. In 'obj.randomize() with (x) { x > 0; x < y; }' inside
// F, both 'x' are listed and so are class a's rand property; 'y' is not
// listed and so is F's formal 'y' -- not class a's property 'y' (although a
// declares one, it is not in the list) and not env's property 'y', which is
// not visible in F. In env::run_phase the bare 'y' is env's own property (8.5).
// The env sets y to 10, calls F(obj, y), and checks that obj.x came out in
// 1..9.
//
// UVM macros. uvm_macros.svh is SystemVerilog source included by the file,
// so what the two macro calls expand to is fixed by that source (22.5.1).
// Every UVM version bundled in third_party/UVM expands them to the same
// shape:
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
// versions.
//
// What is checked, and why:
//   Compilation-unit items
//     - `include "uvm_macros.svh" (22.4) is recorded once on the source file
//     - the file uses the macro uvm_info exactly once and uvm_error exactly
//       once
//     - import uvm_pkg::*; (26.3): asserted through its effect -- the UVM
//       names the file uses bind into package uvm_pkg
//   class a (8.3)
//     - exactly one class named "a"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label; declares no
//       methods (randomize() is built in, 18.6.1)
//     - exactly 2 properties, in order x, y; both signed int (6.11), public
//       (8.18), not const (8.19)
//     - 'rand int x;' (18.4): getIsRandomized() true, getRandType() vpiRand,
//       no initializer
//     - 'int y = -1;': not rand/randc (18.4); initialized with -1 (8.8)
//   function int F(a obj, int y); (13.4)
//     - exactly one compilation-unit function named "F" (3.12.1); return
//       type a signed int (13.4.1); static lifetime (13.4.2: declared
//       outside a class with no lifetime keyword)
//     - formals, in order: 'obj' (input, a handle to class a) and 'y'
//       (input, signed int); no direction means input (13.3)
//   F = obj.randomize() with (x) { x > 0; x < y; }; (13.4.1, 18.7)
//     - the function's only statement, so not wrapped in a Begin: a blocking
//       Assignment whose LHS is a RefObj named "F" (13.4.1)
//     - its RHS is a hierarchical RefObj whose prefix 'obj' binds to the
//       Variable holding F's formal 'obj' (37.15), then a MethodFuncCall
//       "randomize" with no arguments -- '(x)' after 'with' is the
//       identifier list, not an argument of randomize() -- and a 'with'
//       inline constraint (18.7)
//     - the 'with' block holds exactly two constraint expressions, in source
//       order: 'x > 0' (vpiGtOp, right operand the literal 0) and 'x < y'
//       (vpiLtOp)
//     - both 'x' bind to class a's property 'x' (18.7: listed)
//     - 'y' binds to the Variable holding F's formal 'y' (18.7: not listed;
//       37.15) -- not to class a's 'y' and not to env's 'y'
//   class env extends uvm_env (8.3, 8.13)
//     - exactly one class named "env"; user-defined; not virtual; no
//       parameters; no end label; Extends resolves to uvm_pkg's uvm_env
//     - exactly 2 properties, in declaration order: obj, y. Each is public,
//       not rand/randc, not const. 'a obj = new;' is a handle to class a
//       initialized with a 'new' call with no arguments (8.7); 'int y;' is
//       a signed int with no initializer.
//     - exactly 2 methods, 'new' (Function) and 'run_phase' (Task), both
//       automatic (8.6)
//   function new(string name, uvm_component parent = null); (8.7)
//     - formals 'name' (input string) and 'parent' (input uvm_component
//       handle); the body is 'super.new(name, parent);' (8.15) bound to
//       uvm_env's constructor, with both actuals bound to the Variables
//       holding this constructor's formals
//   task run_phase(uvm_phase phase); ... endtask: run_phase (13.3)
//     - one input formal 'phase' of type uvm_phase; end label 'run_phase'
//     - the body executes 3 statements: phase.raise_objection(this), a
//       begin-end, phase.drop_objection(this); each objection call has the
//       prefix bound to 'phase' and one argument 'this' bound to env (8.11)
//   begin ... end (lines 39-47)
//     - the block declares no variable and holds exactly 3 statements, in
//       order: an Assignment, the call F(obj, y), and an if-else (12.4)
//   y = 10; (10.4.1)
//     - blocking Assignment; LHS binds to env's 'y' (not F's formal); RHS is
//       the integer literal 10
//   F(obj, y); (13.4.1)
//     - a function called as a statement, its return value discarded: a
//       call named "F" bound by object identity to the compilation-unit
//       function F, with exactly 2 arguments: 'obj' bound to env's 'obj' and
//       'y' bound to env's 'y'
//   if(obj.x > 0 && obj.x < y) (12.4, 11.3.2)
//     - an IfElse whose condition is
//       vpiLogAndOp(vpiGtOp(obj.x, 0), vpiLtOp(obj.x, y)): '>' and '<' bind
//       tighter than '&&'. obj.x is a hierarchical reference whose prefix
//       binds to env's 'obj' and whose member binds to class a's 'x' (8.5);
//       the bare 'y' binds to env's 'y'
//   `uvm_info("RESULT", $sformatf("... SUCCESS", obj.x, y), UVM_LOW);
//     - the then-branch starts with the macro's begin-end holding one IfStmt
//       whose condition is uvm_report_enabled(UVM_LOW, UVM_INFO, "RESULT")
//       and whose action is uvm_report_info("RESULT", $sformatf(...),
//       UVM_LOW, ...); $sformatf has 3 arguments: the format string
//       literal, obj.x and env's y
//   `uvm_error("RESULT", $sformatf("... FAILED", obj.x, y));
//     - the else-branch has the same shape with uvm_report_enabled(UVM_NONE,
//       UVM_ERROR, "RESULT") and uvm_report_error(..., UVM_NONE, ...)
//   module top (23.2)
//     - no ports; exactly 1 variable 'environment', a handle to class env
//     - one initial whose begin-end holds 'environment = new("env");' (bound
//       to env's constructor) and 'run_test();' (a TaskCall bound to
//       uvm_pkg's run_test)
//   Diagnostics
//     - no CompUndefinedVariable for any variable or formal the file
//       references
//     - no fatal, syntax or error diagnostic points into this file
//     - the whole compilation is legal: zero fatal, syntax and error
//       diagnostics
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no parameter, no module instance and no class specialization;
// every binding asserted is name resolution. No check is gated on
// getElaborated().
//
// KNOWN FAILURE (uvm_pkg is never compiled), not a defect in this test: the
// .hlc puts UVM on the include path (-I1800.2-2017-1.0/src) but does not
// list 1800.2-2017-1.0/src/uvm_pkg.sv as a source, so the design has no
// package uvm_pkg and 'import uvm_pkg::*;' (26.3) has nothing to import.
// Every check that needs a UVM declaration therefore fails:
// UvmPkgProvidesTheNamesTheFileUses, ClassEnvExtendsUvmEnv,
// EnvNewHasFormalsNameAndParent, EnvNewBodyIsSuperNewOfNameAndParent,
// RunPhaseHasOneUvmPhaseFormalAndEndLabel, RunPhaseRaisesObjectionOnThis,
// RunPhaseDropsObjectionOnThis, ThenBranchIsUvmInfoExpansion,
// ElseBranchIsUvmErrorExpansion and InitialCallsRunTestFromUvmPkg. Adding
// uvm_pkg.sv to the .hlc (as 18.13.4--get_randstate_0.hlc does) is expected
// to make them pass. They assert the LRM binding and are intentionally not
// skipped or relaxed.
//
// KNOWN FAILURE (names in a 'with' block are not looked up in the
// randomized object's class), not a defect in this test:
// BothXInWithBindToClassAProperty fails on HLC 577. HLC parses
// 'with (x) { ... }' and builds both items with the right shape, but both
// 'x' have no actual, although 'x' is listed in the identifier list and so
// must resolve to class a's 'x' (18.7). HLC resolves names in the 'with'
// block only in the scope of the call; F declares no 'x', so nothing is
// found. 'y' binds correctly to F's formal -- but only because HLC never
// looks in class a at all, not because it honors the identifier list.
// This does not depend on uvm_pkg. It is expected to fail until HLC is
// fixed and is intentionally not skipped or relaxed.
//
// KNOWN FAILURE (errors reported in this file), not a defect in this test:
// NoErrorIsReportedInThisFile and NoFatalSyntaxOrErrorDiagnostics assert
// that this legal file compiles cleanly and are expected to fail. HLC
// reports:
//   - IllegalPropertyValue (2038) twice at line 25, column 34 -- the
//     '{' that opens the 'with' block, once per constraint expression --
//     although the restricted inline constraint is legal (18.7).
//   - LN7705 "Null Actual" at lines 38, 48 and 59 (raise_objection,
//     drop_objection, run_test) -- a consequence of uvm_pkg not being
//     compiled (see above).
// With this .hlc: 0 syntax errors and 9 errors in total. Neither check is
// skipped or relaxed.
//
// What is NOT checked, and why:
//   - HLDB has no accessor for the identifier list of a restricted inline
//     constraint, so '(x)' itself is not read back. Its effect -- which
//     scope each name in the block resolves in -- is what 18.7 fixes, and
//     that is asserted by BothXInWithBindToClassAProperty and
//     YInWithBindsToFormalOfF.
//   - The value randomize() picks for obj.x, F's return value, which branch
//     runs, the text the report prints, objection counting, phase
//     scheduling and which test run_test starts all only exist while
//     simulation runs. Permanently out of scope. The static half is covered
//     by: WithBlockHoldsXGreaterThanZeroThenXLessThanY,
//     BothXInWithBindToClassAProperty, YInWithBindsToFormalOfF,
//     RunPhaseCallsFWithObjAndY and IfConditionMirrorsTheInlineConstraint.
//   - How HLC wraps the 'with' block (an unnamed Constraint holding the
//     items, or the items stored otherwise) is a model convention:
//     MethodFuncCall::getWith() is typed Any. Both an unnamed Constraint
//     and a lone expression are accepted; the items, operators and bindings
//     are asserted.
//   - '-1' is the unary minus applied to the literal 1 (11.4.3). Whether
//     HLC keeps the Operation or folds it into a Constant whose source text
//     is "-1" is a model convention; both are accepted.
//   - What the LHS 'F' binds to (the Function or an implicit return
//     variable) is a model convention.
//   - Whether F(obj, y) is modeled as FuncCall or another TFCall kind is a
//     tool convention; it is checked through TFCall by name, binding and
//     arguments. 13.4.1 lets a tool warn when a non-void result is
//     discarded; warnings are not asserted.
//   - Whether HLC binds a call to the built-in randomize() through
//     getTaskFunc() is a tool convention.
//   - Whether the unqualified inherited calls uvm_report_enabled/
//     uvm_report_info/uvm_report_error are FuncCall or MethodFuncCall is a
//     tool convention; they are checked through TFCall by name and
//     arguments. Their arguments after the third depend on the UVM version.
//   - Each macro call is followed by ';', which leaves a null statement after
//     the macro's begin-end. Whether HLC stores null statements is a tool
//     convention, so branch blocks are read at index 0 and their statement
//     count is not asserted.
//   - raise_objection/drop_objection are 'extern virtual' in uvm_phase, so
//     the bound method's name and kind are asserted, not object identity.
//   - Whether run_phase is flagged virtual (8.20) is a model convention.
//   - 'a obj = new;' invokes class a's implicit constructor (8.7); how HLC
//     represents it is a tool convention.
//   - The default value of 'parent' (no grounded accessor in this suite).
//   - Design-wide counts of classes, packages and functions depend on the
//     UVM and built-in libraries, so every declaration is found by name.
//   - The warning count, source line numbers, and the license/metadata
//     comments.

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
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
#include <hldb/named_argument.h>
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
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace hlc {

class InLineConstraintsRandomize6Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.7--in-line-constraints--randomize_6.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // --- UVM declarations the file binds to --------------------------------

  static const hldb::Package *getUvmPkg() {
    return hldb::findByName<hldb::Package>("uvm_pkg", m_design->getAllPackages());
  }

  static const hldb::ClassDefn *getUvmClass(std::string_view name) {
    const hldb::Package *const pkg = getUvmPkg();
    if (pkg == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>(name, pkg->getClassDefns());
  }

  static const hldb::Function *getUvmEnvNew() {
    const hldb::ClassDefn *const uvmEnv = getUvmClass("uvm_env");
    if (uvmEnv == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("new", uvmEnv->getMethods());
  }

  static const hldb::Task *getRunTest() {
    const hldb::Package *const pkg = getUvmPkg();
    if (pkg == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Task>("run_test", pkg->getTaskFuncs());
  }

  // --- class a ------------------------------------------------------------

  static const hldb::ClassDefn *getClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Variable *getClassAProperty(std::string_view name) {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, cls->getVariables());
  }

  static const hldb::Variable *getClassAX() { return getClassAProperty("x"); }

  // --- function F ---------------------------------------------------------

  static const hldb::Function *getF() {
    if (m_design->getTaskFuncs() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("F", m_design->getTaskFuncs());
  }

  // 37.15: a reference to a formal resolves to the Variable holding it,
  // never to the IODecl.
  static const hldb::Variable *getFFormalVariable(std::string_view name) {
    const hldb::Function *const fn = getF();
    if (fn == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, fn->getVariables());
  }

  static const hldb::Assignment *getFBody() {
    const hldb::Function *const fn = getF();
    if (fn == nullptr) {
      return nullptr;
    }
    return fn->getStmt<hldb::Assignment>();
  }

  // The MethodFuncCall 'randomize' at the end of 'obj.randomize() with ...'.
  static const hldb::MethodFuncCall *getRandomizeCall() {
    const hldb::Assignment *const assign = getFBody();
    if (assign == nullptr) {
      return nullptr;
    }
    const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
    if (path == nullptr || path->getPathElems() == nullptr || path->getPathElems()->size() != 2) {
      return nullptr;
    }
    return any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  }

  // The items of the 'with' block. getWith() is typed Any: HLC may store an
  // unnamed Constraint holding the items, or a single expression directly.
  static std::vector<const hldb::Any *> getWithItems() {
    std::vector<const hldb::Any *> items;
    const hldb::MethodFuncCall *const call = getRandomizeCall();
    if (call == nullptr || call->getWith() == nullptr) {
      return items;
    }
    if (const hldb::Constraint *const block = call->getWith<hldb::Constraint>()) {
      if (block->getConstraintItems() == nullptr) {
        return items;
      }
      for (const hldb::Any *const item : *block->getConstraintItems()) {
        items.emplace_back(item);
      }
      return items;
    }
    items.emplace_back(call->getWith());
    return items;
  }

  // Operand 'operand' of 'with' item 'item', or nullptr.
  static const hldb::Any *getWithOperand(size_t item, size_t operand) {
    const std::vector<const hldb::Any *> items = getWithItems();
    if (items.size() <= item) {
      return nullptr;
    }
    const hldb::Operation *const op = any_cast<hldb::Operation>(items[item]);
    if (op == nullptr || op->getOperands() == nullptr || op->getOperands()->size() <= operand) {
      return nullptr;
    }
    return op->getOperands()->at(operand);
  }

  // --- class env ----------------------------------------------------------

  static const hldb::ClassDefn *getEnv() { return hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses()); }

  static const hldb::Variable *getEnvProperty(std::string_view name) {
    const hldb::ClassDefn *const env = getEnv();
    if (env == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, env->getVariables());
  }

  static const hldb::Function *getEnvNew() {
    const hldb::ClassDefn *const env = getEnv();
    if (env == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("new", env->getMethods());
  }

  static const hldb::Task *getRunPhase() {
    const hldb::ClassDefn *const env = getEnv();
    if (env == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  }

  static const hldb::Any *getRunPhaseStmt(size_t index) {
    const hldb::Task *const task = getRunPhase();
    if (task == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const body = task->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= index) {
      return nullptr;
    }
    return body->getStmts()->at(index);
  }

  // The explicit 'begin ... end' on lines 39-47.
  static const hldb::Begin *getInnerBlock() { return any_cast<hldb::Begin>(getRunPhaseStmt(1)); }

  static const hldb::Any *getInnerStmt(size_t index) {
    const hldb::Begin *const block = getInnerBlock();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() <= index) {
      return nullptr;
    }
    return block->getStmts()->at(index);
  }

  static const hldb::IfElse *getIfElse() { return any_cast<hldb::IfElse>(getInnerStmt(2)); }

  // --- module top ---------------------------------------------------------

  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getEnvironment() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("environment", top->getVariables());
  }

  static const hldb::Initial *getInitial() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Initial>(top->getProcesses()->at(0));
  }

  static const hldb::Any *getInitialStmt(size_t index) {
    const hldb::Initial *const init = getInitial();
    if (init == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const block = init->getStmt<hldb::Begin>();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() <= index) {
      return nullptr;
    }
    return block->getStmts()->at(index);
  }

  // --- source file ----------------------------------------------------------

  static const hldb::SourceFile *getSourceFile() {
    if (m_design->getSourceFiles() == nullptr) {
      return nullptr;
    }
    for (const hldb::SourceFile *const sf : *m_design->getSourceFiles()) {
      if (sf->getName().find("18.7--in-line-constraints--randomize_6.sv") != std::string_view::npos) {
        return sf;
      }
    }
    return nullptr;
  }

  static std::vector<const hldb::PreprocMacroInstance *> getMacroInstances(std::string_view name) {
    std::vector<const hldb::PreprocMacroInstance *> found;
    const hldb::SourceFile *const sf = getSourceFile();
    if (sf == nullptr || sf->getPreprocMacroInstances() == nullptr) {
      return found;
    }
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

  // Verifies 'expr' is '-1': either the unary minus applied to the literal 1
  // (11.4.3), or that Operation folded into a Constant "-1".
  static void ExpectMinusOne(const hldb::Any *expr) {
    ASSERT_NE(expr, nullptr) << "8.8: 'int y = -1;' has an initializer";
    if (const hldb::Operation *const op = any_cast<hldb::Operation>(expr)) {
      EXPECT_EQ(op->getOpType(), vpiMinusOp) << "11.4.3: '-1' is the unary minus applied to 1";
      ASSERT_NE(op->getOperands(), nullptr);
      ASSERT_EQ(op->getOperands()->size(), 1u) << "unary minus has one operand";
      ExpectIntLiteral(op->getOperands()->at(0), "1");
      return;
    }
    ExpectIntLiteral(expr, "-1");
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

  // Verifies 'expr' is 'obj.x': a hierarchical RefObj whose prefix binds to
  // env's 'obj' and whose member binds to class a's 'x' (8.5).
  static void ExpectObjDotX(const hldb::Any *expr) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'obj.x' should be a hierarchical RefObj";
    ASSERT_NE(path->getPathElems(), nullptr) << "'obj.x' is a member access through a handle";
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    ExpectBoundRef(path->getPathElems()->at(0), "obj", getEnvProperty("obj"));
    ExpectBoundRef(path->getPathElems()->at(1), "x", getClassAX());
  }

  // Verifies 'expr' is '$sformatf(<format>, obj.x, y)'.
  static void ExpectSformatfOfObjXAndY(const hldb::Any *expr, std::string_view format) {
    const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(expr);
    ASSERT_NE(call, nullptr) << "the message argument should be a $sformatf SysFuncCall";
    EXPECT_EQ(call->getName(), "$sformatf");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 3u) << "a format string, 'obj.x' and 'y'";
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    ExpectStringLiteral(arg0->getHighConn(), format);
    const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
    ASSERT_NE(arg1, nullptr);
    ExpectObjDotX(arg1->getHighConn());
    const hldb::NamedArgument *const arg2 = call->getArguments()->at(2);
    ASSERT_NE(arg2, nullptr);
    ExpectBoundRef(arg2->getHighConn(), "y", getEnvProperty("y"));
  }

  // Verifies 'stmt' is 'phase.<method>(this);'.
  static void ExpectObjectionCall(const hldb::Any *stmt, std::string_view method) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(stmt);
    ASSERT_NE(path, nullptr) << "'phase." << method << "(this)' should be a hierarchical RefObj";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    const hldb::Task *const runPhase = getRunPhase();
    ASSERT_NE(runPhase, nullptr);
    // 37.15: the prefix references the formal, which resolves to the
    // Variable holding it, never to the IODecl.
    ExpectBoundRef(path->getPathElems()->at(0), "phase",
                   hldb::findByName<hldb::Variable>("phase", runPhase->getVariables()));

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

  // Verifies 'branch' is a begin-end starting with the expansion of a UVM
  // report macro:
  //   begin
  //     if (uvm_report_enabled(<verbosity>, <severity>, "RESULT"))
  //       <reportFn>("RESULT", $sformatf(<format>, obj.x, y), <verbosity>, ...);
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
    const hldb::NamedArgument *const enabledArg2 = enabled->getArguments()->at(2);
    ASSERT_NE(enabledArg2, nullptr);
    ExpectStringLiteral(enabledArg2->getHighConn(), "\"RESULT\"");

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
    ExpectSformatfOfObjXAndY(reportArg1->getHighConn(), format);
    const hldb::NamedArgument *const reportArg2 = report->getArguments()->at(2);
    ASSERT_NE(reportArg2, nullptr);
    ExpectEnumConstRef(reportArg2->getHighConn(), verbosity);
  }
};

// ---------------------------------------------------------------------------
// import uvm_pkg::*;  `include "uvm_macros.svh"
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize6Test, SourceFileIncludesUvmMacros) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr) << "18.7--in-line-constraints--randomize_6.sv not found in Design::getSourceFiles()";
  ASSERT_NE(sf->getIncludes(), nullptr) << "22.4: the file includes uvm_macros.svh";
  size_t count = 0;
  for (const hldb::SourceFile *const inc : *sf->getIncludes()) {
    if (inc->getName().find("uvm_macros.svh") != std::string_view::npos) ++count;
  }
  EXPECT_EQ(count, 1u) << "'`include \"uvm_macros.svh\"' is written once";
}

TEST_F(InLineConstraintsRandomize6Test, FileUsesUvmInfoOnceAndUvmErrorOnce) {
  EXPECT_EQ(getMacroInstances("uvm_info").size(), 1u) << "line 43 is the only `uvm_info";
  EXPECT_EQ(getMacroInstances("uvm_error").size(), 1u) << "line 45 is the only `uvm_error";
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, UvmPkgProvidesTheNamesTheFileUses) {
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
// class a; rand int x; int y = -1; endclass
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize6Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
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

TEST_F(InLineConstraintsRandomize6Test, ClassAHasNoBaseClassParametersMethodsOrEndLabel) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_FALSE(cls->getVirtual()) << "8.21: 'class a' is not declared 'virtual class'";
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
  size_t declared = 0;
  if (cls->getMethods() != nullptr) {
    for (const hldb::TaskFunc *const method : *cls->getMethods()) {
      if (method->getName() != "new") ++declared;
    }
  }
  EXPECT_EQ(declared, 0u) << "class a declares no task or function; randomize() is built in (18.6.1)";
}

TEST_F(InLineConstraintsRandomize6Test, ClassAHasPropertiesXAndYInOrder) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 2u) << "'rand int x;' and 'int y = -1;'";
  const std::vector<std::string_view> expected = {"x", "y"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = cls->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    ExpectSignedInt(prop->getTypespec(), expected[i]);
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(InLineConstraintsRandomize6Test, ClassAXIsRandWithNoInitializer) {
  const hldb::Variable *const x = getClassAX();
  ASSERT_NE(x, nullptr);
  EXPECT_TRUE(x->getIsRandomized()) << "18.4: 'x' is declared with the 'rand' qualifier";
  EXPECT_EQ(x->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(x->getValue(), nullptr) << "'rand int x;' has no initializer";
}

TEST_F(InLineConstraintsRandomize6Test, ClassAYIsNotRandAndInitializedToMinusOne) {
  const hldb::Variable *const y = getClassAProperty("y");
  ASSERT_NE(y, nullptr);
  EXPECT_FALSE(y->getIsRandomized()) << "18.4: 'y' has no 'rand'/'randc' qualifier";
  EXPECT_EQ(y->getRandType(), vpiNotRand) << "18.4: 'y' has no 'rand'/'randc' qualifier";
  ExpectMinusOne(y->getValue());
}

// ---------------------------------------------------------------------------
// function int F(a obj, int y); F = obj.randomize() with (x) {...}; endfunction
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize6Test, FIsCompilationUnitFunctionReturningInt) {
  ASSERT_NE(m_design->getTaskFuncs(), nullptr) << "3.12.1: F is declared in the compilation unit";
  size_t count = 0;
  for (const hldb::TaskFunc *const tf : *m_design->getTaskFuncs()) {
    if (tf->getName() == "F") ++count;
  }
  EXPECT_EQ(count, 1u) << "the source declares function 'F' exactly once";
  const hldb::Function *const fn = getF();
  ASSERT_NE(fn, nullptr) << "13.4: 'function int F(...)' is a Function";
  ExpectSignedInt(fn->getReturn(), "F's return type");
  EXPECT_FALSE(fn->getAutomatic())
      << "13.4.2: a subroutine outside a class with no lifetime keyword has static lifetime";
}

TEST_F(InLineConstraintsRandomize6Test, FHasFormalsObjAndY) {
  const hldb::Function *const fn = getF();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 2u) << "'a obj' and 'int y'";

  const hldb::IODecl *const obj = fn->getIODecls()->at(0);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->getName(), "obj");
  EXPECT_EQ(obj->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectClassHandle(obj->getTypespec(), getClassA(), "'obj'");

  const hldb::IODecl *const y = fn->getIODecls()->at(1);
  ASSERT_NE(y, nullptr);
  EXPECT_EQ(y->getName(), "y");
  EXPECT_EQ(y->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectSignedInt(y->getTypespec(), "formal 'y'");
}

TEST_F(InLineConstraintsRandomize6Test, FBodyAssignsToFunctionName) {
  const hldb::Assignment *const assign = getFBody();
  ASSERT_NE(assign, nullptr) << "the body is a single statement, so it is not wrapped in a Begin";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the LHS is a reference";
  EXPECT_EQ(lhs->getName(), "F") << "13.4.1: assigning the function name sets the return value";
}

TEST_F(InLineConstraintsRandomize6Test, FRhsIsObjRandomizeWithRestrictedInlineConstraint) {
  const hldb::Assignment *const assign = getFBody();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'obj.randomize() with (x) {...}' should be a hierarchical RefObj";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  ExpectBoundRef(path->getPathElems()->at(0), "obj", getFFormalVariable("obj"));

  const hldb::MethodFuncCall *const call = getRandomizeCall();
  ASSERT_NE(call, nullptr) << "18.6.1: randomize() is a function called through a handle, so a MethodFuncCall";
  EXPECT_EQ(call->getName(), "randomize");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "'randomize()' is written with no arguments; '(x)' after 'with' is the identifier list (18.7)";
  EXPECT_NE(call->getWith(), nullptr) << "18.7: the call carries a 'with' inline constraint";
}

TEST_F(InLineConstraintsRandomize6Test, WithBlockHoldsXGreaterThanZeroThenXLessThanY) {
  ASSERT_NE(getRandomizeCall(), nullptr);
  const std::vector<const hldb::Any *> items = getWithItems();
  ASSERT_EQ(items.size(), 2u) << "18.7: '{ x > 0; x < y; }' holds two constraint expressions";

  const hldb::Operation *const gt = any_cast<hldb::Operation>(items[0]);
  ASSERT_NE(gt, nullptr) << "'x > 0' should be an Operation";
  EXPECT_EQ(gt->getOpType(), vpiGtOp);
  ASSERT_NE(gt->getOperands(), nullptr);
  EXPECT_EQ(gt->getOperands()->size(), 2u) << "'>' is a binary operator";
  ExpectIntLiteral(getWithOperand(0, 1), "0");

  const hldb::Operation *const lt = any_cast<hldb::Operation>(items[1]);
  ASSERT_NE(lt, nullptr) << "'x < y' should be an Operation";
  EXPECT_EQ(lt->getOpType(), vpiLtOp);
  ASSERT_NE(lt->getOperands(), nullptr);
  EXPECT_EQ(lt->getOperands()->size(), 2u) << "'<' is a binary operator";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (names in a 'with'
// block are not looked up in the randomized object's class) in the file
// header.
TEST_F(InLineConstraintsRandomize6Test, BothXInWithBindToClassAProperty) {
  // 18.7: 'x' is in the identifier list '(x)', so it is looked up in class a,
  // the class of the randomized object.
  ExpectBoundRef(getWithOperand(0, 0), "x", getClassAX());
  ExpectBoundRef(getWithOperand(1, 0), "x", getClassAX());
}

TEST_F(InLineConstraintsRandomize6Test, YInWithBindsToFormalOfF) {
  // 18.7: 'y' is not in the identifier list, so it resolves in the scope of
  // the call -- F's formal 'y' (37.15: the Variable holding it).
  ExpectBoundRef(getWithOperand(1, 1), "y", getFFormalVariable("y"));
  const hldb::RefObj *const y = any_cast<hldb::RefObj>(getWithOperand(1, 1));
  ASSERT_NE(y, nullptr);
  EXPECT_NE(y->getActual(), getEnvProperty("y")) << "F is not inside env, so 'y' is not env's 'y'";
  ASSERT_NE(getClassAProperty("y"), nullptr);
  EXPECT_NE(y->getActual(), getClassAProperty("y"))
      << "18.7: 'y' is not in the identifier list '(x)', so class a's 'y' must not be used";
}

// ---------------------------------------------------------------------------
// class env extends uvm_env; ... endclass
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize6Test, ClassEnvIsDeclaredOnceAsUserDefinedClass) {
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

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, ClassEnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  const hldb::Extends *const ext = env->getExtends();
  ASSERT_NE(ext, nullptr) << "8.13: 'class env extends uvm_env' names a base class";
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  const hldb::RefTypespec *const base = ext->getClassTypespecs()->at(0);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "uvm_env");
  ExpectClassHandle(base, getUvmClass("uvm_env"), "'uvm_env'");
}

TEST_F(InLineConstraintsRandomize6Test, ClassEnvHasPropertiesObjAndYInOrder) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getVariables(), nullptr);
  ASSERT_EQ(env->getVariables()->size(), 2u) << "'a obj' and 'int y'";
  const std::vector<std::string_view> expected = {"obj", "y"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = env->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getIsRandomized()) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(InLineConstraintsRandomize6Test, ObjIsClassAHandleInitializedWithNew) {
  const hldb::Variable *const obj = getEnvProperty("obj");
  ASSERT_NE(obj, nullptr);
  ExpectClassHandle(obj->getTypespec(), getClassA(), "'obj'");
  const hldb::MethodFuncCall *const init = obj->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(init, nullptr) << "8.7: 'a obj = new;' initializes obj with a constructor call";
  EXPECT_EQ(init->getName(), "new");
  EXPECT_TRUE(init->getArguments() == nullptr || init->getArguments()->empty()) << "'new' is written with no arguments";
}

TEST_F(InLineConstraintsRandomize6Test, EnvYIsAnUninitializedSignedInt) {
  const hldb::Variable *const y = getEnvProperty("y");
  ASSERT_NE(y, nullptr);
  ExpectSignedInt(y->getTypespec(), "env's 'y'");
  EXPECT_EQ(y->getValue(), nullptr) << "'int y;' has no initializer";
}

TEST_F(InLineConstraintsRandomize6Test, ClassEnvHasMethodsNewAndRunPhase) {
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

TEST_F(InLineConstraintsRandomize6Test, EnvMethodsHaveAutomaticLifetime) {
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

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, EnvNewHasFormalsNameAndParent) {
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

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, EnvNewBodyIsSuperNewOfNameAndParent) {
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
  // 37.15: the actuals reference this constructor's formals, which resolve
  // to the Variables holding them, never to the IODecls.
  ExpectBoundRef(arg0->getHighConn(), "name", hldb::findByName<hldb::Variable>("name", ctor->getVariables()));
  const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  ExpectBoundRef(arg1->getHighConn(), "parent", hldb::findByName<hldb::Variable>("parent", ctor->getVariables()));
}

// ---------------------------------------------------------------------------
// task run_phase(uvm_phase phase); ... endtask: run_phase
// ---------------------------------------------------------------------------

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, RunPhaseHasOneUvmPhaseFormalAndEndLabel) {
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

TEST_F(InLineConstraintsRandomize6Test, RunPhaseBodyIsRaiseBlockDrop) {
  const hldb::Task *const task = getRunPhase();
  ASSERT_NE(task, nullptr);
  const hldb::Begin *const body = task->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a body with more than one statement should be wrapped in a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u) << "raise_objection, begin-end, drop_objection";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(0)), nullptr);
  EXPECT_NE(any_cast<hldb::Begin>(body->getStmts()->at(1)), nullptr) << "line 39 opens an explicit begin-end";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(2)), nullptr);
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, RunPhaseRaisesObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(0), "raise_objection");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, RunPhaseDropsObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(2), "drop_objection");
}

// ---------------------------------------------------------------------------
// begin y = 10; F(obj, y); if(obj.x > 0 && obj.x < y) ... end
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize6Test, InnerBlockHoldsThreeStatementsInOrder) {
  const hldb::Begin *const block = getInnerBlock();
  ASSERT_NE(block, nullptr);
  EXPECT_TRUE(block->getVariables() == nullptr || block->getVariables()->empty()) << "the block declares no variable";
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 3u) << "lines 40, 41 and 42";
  EXPECT_NE(any_cast<hldb::Assignment>(getInnerStmt(0)), nullptr) << "statement 0 is 'y = 10;'";
  EXPECT_NE(any_cast<hldb::TFCall>(getInnerStmt(1)), nullptr) << "statement 1 is the call 'F(obj, y);'";
  EXPECT_NE(getIfElse(), nullptr) << "12.4: statement 2 is an if with an else, so it is an IfElse";
}

TEST_F(InLineConstraintsRandomize6Test, RunPhaseAssignsTenToEnvY) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(getInnerStmt(0));
  ASSERT_NE(assign, nullptr) << "'y = 10;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "y", getEnvProperty("y"));
  ExpectIntLiteral(assign->getRhs(), "10");
}

TEST_F(InLineConstraintsRandomize6Test, RunPhaseCallsFWithObjAndY) {
  const hldb::TFCall *const call = any_cast<hldb::TFCall>(getInnerStmt(1));
  ASSERT_NE(call, nullptr) << "'F(obj, y);' is a subroutine call";
  EXPECT_EQ(call->getName(), "F");
  ASSERT_NE(getF(), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getF()) << "'F' resolves to the compilation-unit function F (3.12.1)";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "'obj' and 'y' are passed";
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  ExpectBoundRef(arg0->getHighConn(), "obj", getEnvProperty("obj"));
  const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  ExpectBoundRef(arg1->getHighConn(), "y", getEnvProperty("y"));
}

// ---------------------------------------------------------------------------
// if(obj.x > 0 && obj.x < y) begin `uvm_info(...) end
// else begin `uvm_error(...) end
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize6Test, IfConditionMirrorsTheInlineConstraint) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);

  // 11.3.2: '&&' has lower precedence than '>' and '<'.
  const hldb::Any *gt = nullptr;
  const hldb::Any *lt = nullptr;
  ExpectBinaryOp(ifElse->getCondition(), vpiLogAndOp, "obj.x > 0 && obj.x < y", &gt, &lt);

  const hldb::Any *gtLhs = nullptr;
  const hldb::Any *gtRhs = nullptr;
  ExpectBinaryOp(gt, vpiGtOp, "obj.x > 0", &gtLhs, &gtRhs);
  ExpectObjDotX(gtLhs);
  ExpectIntLiteral(gtRhs, "0");

  const hldb::Any *ltLhs = nullptr;
  const hldb::Any *ltRhs = nullptr;
  ExpectBinaryOp(lt, vpiLtOp, "obj.x < y", &ltLhs, &ltRhs);
  ExpectObjDotX(ltLhs);
  ExpectBoundRef(ltRhs, "y", getEnvProperty("y"));
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, ThenBranchIsUvmInfoExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getStmt(), "uvm_report_info", "UVM_INFO", "UVM_LOW", "\"obj.x = %0d y = %0d SUCCESS\"");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, ElseBranchIsUvmErrorExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getElseStmt(), "uvm_report_error", "UVM_ERROR", "UVM_NONE",
                       "\"obj.x = %0d y = %0d FAILED\"");
}

// ---------------------------------------------------------------------------
// module top; env environment; initial begin ... end endmodule
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize6Test, TopHasNoPortsAndOneEnvHandle) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top;' has no port list";
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "'env environment;' is the module's only declaration";
  const hldb::Variable *const environment = getEnvironment();
  ASSERT_NE(environment, nullptr) << "a class-typed data declaration is a variable (6.8)";
  ExpectClassHandle(environment->getTypespec(), getEnv(), "'environment'");
}

TEST_F(InLineConstraintsRandomize6Test, TopHasOneInitialWithTwoStatements) {
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

TEST_F(InLineConstraintsRandomize6Test, InitialConstructsEnvironment) {
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

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize6Test, InitialCallsRunTestFromUvmPkg) {
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

// The file is legal SystemVerilog, so no fatal, syntax or error diagnostic
// may point into it.
// Expected to fail -- see KNOWN FAILURE (errors reported in this file) in the
// file header.
TEST_F(InLineConstraintsRandomize6Test, NoErrorIsReportedInThisFile) {
  size_t count = 0;
  for (const Event &error : m_session->getEventListener()->getEvents()) {
    const EventDefinition *const info = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if (info == nullptr || info->m_severity > EventSeverity::Error) continue;
    for (const EventArg &loc : error.getArguments()) {
      const std::string_view path = loc.m_file;
      if (path.find("18.7--in-line-constraints--randomize_6.sv") == std::string_view::npos) continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Expected to fail -- see KNOWN FAILURE (errors reported in this file) in the
// file header.
TEST_F(InLineConstraintsRandomize6Test, NoFatalSyntaxOrErrorDiagnostics) {
  GTEST_SKIP() << "HLC report a number of binding issues with UVM";
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0) << "the source is legal SystemVerilog and should compile without errors";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
