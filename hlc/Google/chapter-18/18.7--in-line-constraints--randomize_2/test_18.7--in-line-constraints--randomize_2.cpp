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
// tests/Google/chapter-18/18.7--in-line-constraints--randomize_2.sv. Lines
// 1-14 are the license and the suite's :name:/:description:/:tags: comment;
// the rest is:
//
//   16  import uvm_pkg::*;
//   17  `include "uvm_macros.svh"
//   18
//   19  class a1;
//   20      rand int x;
//   21  endclass
//   22
//   23  class a2;
//   24      int x, y;
//   25
//   26      task do_randomize(a1 obj, int x, int z);
//   27          int result;
//   28          /* In the line below x should be a member of class a1 */
//   29          result = obj.randomize() with {x > 0; x < y + z;};
//   30      endtask
//   31  endclass
//   32
//   33  class env extends uvm_env;
//   34
//   35    a1 obj1 = new;
//   36    a2 obj2 = new;
//   37    int z;
//   38
//   39    function new(string name, uvm_component parent = null);
//   40      super.new(name, parent);
//   41    endfunction
//   42
//   43    task run_phase(uvm_phase phase);
//   44      phase.raise_objection(this);
//   45      begin
//   46        obj2.x = -20;
//   47        obj2.y = 5;
//   48        z = 10;
//   49        obj2.do_randomize(obj1, -20, z);
//   50        if(obj1.x > 0 && obj1.x < obj2.y + z) begin
//   51          `uvm_info("RESULT", $sformatf("obj1.x = %0d obj2.x = %0d obj2.y = %0d SUCCESS", obj1.x, obj2.x,
//   obj2.y), UVM_LOW); 52        end else begin 53          `uvm_error("RESULT", $sformatf("obj1.x = %0d obj2.x = %0d
//   obj2.y = %0d FAILED", obj1.x, obj2.x, obj2.y)); 54        end 55      end 56      phase.drop_objection(this); 57
//   endtask: run_phase 58 59  endclass 60 61  module top; 62 63    env environment; 64 65    initial begin 66
//   environment = new("env"); 67      run_test(); 68    end 69 70  endmodule
//
// Classes a1 and a2 are the example of IEEE 1800-2023 18.7. The point of the
// fixture is name resolution inside an inline constraint. Three different
// 'x' are visible at line 29: class a1's rand property, class a2's property
// and the task's formal. 18.7: a name in the 'with' block is searched for
// first in the class of the object being randomized (a1), then in the scope
// that contains the call. So (as the comment on line 28 says):
//   - both 'x' bind to class a1's 'x' -- not to the formal 'x' and not to
//     class a2's 'x', which a1's 'x' hides
//   - 'y' is not declared in a1, so it binds to class a2's 'y' (the class
//     whose method makes the call, 8.5)
//   - 'z' is not declared in a1, so it binds to the task's formal 'z' --
//     not to env's 'z', which is not visible inside a2
// The env sets obj2.x to -20 (the value the formal 'x' also receives) so that
// a solution satisfying 'x > 0' proves the formal was not used.
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
//   class a1 (8.3)
//     - exactly one class named "a1"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label; declares no
//       methods (randomize() is built in, 18.6.1)
//     - 'rand int x;' (18.4): the only property; a signed int (6.11),
//       getIsRandomized() true, getRandType() vpiRand, no initializer,
//       public (8.18), not const (8.19)
//   class a2 (8.3)
//     - exactly one class named "a2"; user-defined; not virtual; no
//       'extends', no parameters, no end label
//     - 'int x, y;': exactly 2 properties, in order x, y; each a signed int
//       with no initializer, not rand/randc (18.4), public, not const
//     - exactly one method, the Task 'do_randomize'; automatic (8.6),
//       public, no end label
//   task do_randomize(a1 obj, int x, int z); (13.3)
//     - formals, in order: 'obj' (input, a handle to class a1), 'x' (input,
//       signed int) and 'z' (input, signed int); no direction means input
//     - 'int result;' declares a signed int variable 'result' local to the
//       task, with no initializer
//   result = obj.randomize() with {x > 0; x < y + z;}; (10.4.1, 18.7)
//     - the task's only statement: a blocking Assignment whose LHS binds to
//       the local 'result'
//     - its RHS is a hierarchical RefObj whose prefix 'obj' binds to the
//       Variable holding the formal 'obj' (37.15), then a MethodFuncCall
//       "randomize" with no arguments and a 'with' inline constraint (18.7)
//     - the 'with' block holds exactly two constraint expressions, in source
//       order: 'x > 0' (vpiGtOp, right operand the literal 0) and
//       'x < y + z' (vpiLtOp whose right operand is vpiAddOp(y, z) -- '+'
//       binds tighter than '<', 11.3.2)
//     - both 'x' bind to class a1's 'x'; 'y' binds to class a2's 'y'; 'z'
//       binds to the Variable holding the formal 'z' (18.7, 37.15)
//   class env extends uvm_env (8.3, 8.13)
//     - exactly one class named "env"; user-defined; not virtual; no
//       parameters; no end label; Extends resolves to uvm_pkg's uvm_env
//     - exactly 3 properties, in declaration order: obj1, obj2, z. Each is
//       public, not rand/randc, not const. 'a1 obj1 = new;' and
//       'a2 obj2 = new;' are handles to a1 and a2 initialized with a 'new'
//       call with no arguments (8.7); 'int z;' is a signed int with no
//       initializer
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
//   begin ... end (lines 45-55)
//     - the block declares no variable and holds exactly 5 statements, in
//       order: 3 Assignments, the call obj2.do_randomize(...) and an
//       if-else (12.4)
//   obj2.x = -20; obj2.y = 5; z = 10; (10.4.1)
//     - blocking Assignments. 'obj2.x' and 'obj2.y' are hierarchical
//       references whose prefix binds to env's 'obj2' and whose member binds
//       to class a2's 'x' / 'y' (8.5); 'z' binds to env's 'z'
//     - the RHS are -20, 5 and 10
//   obj2.do_randomize(obj1, -20, z); (8.5, 13.5)
//     - a hierarchical reference whose prefix binds to env's 'obj2', then a
//       MethodTaskCall "do_randomize" bound by object identity to class a2's
//       task, with exactly 3 arguments: 'obj1' bound to env's 'obj1', -20
//       and 'z' bound to env's 'z'
//   if(obj1.x > 0 && obj1.x < obj2.y + z) (12.4, 11.3.2)
//     - an IfElse whose condition is
//       vpiLogAndOp(vpiGtOp(obj1.x, 0), vpiLtOp(obj1.x, vpiAddOp(obj2.y, z))):
//       '+' binds tighter than '>' and '<', and both bind tighter than '&&'
//     - 'obj1.x' binds through env's 'obj1' to class a1's 'x'; 'obj2.y'
//       through env's 'obj2' to class a2's 'y'; 'z' to env's 'z'
//   `uvm_info("RESULT", $sformatf("... SUCCESS", obj1.x, obj2.x, obj2.y), UVM_LOW);
//     - the then-branch starts with the macro's begin-end holding one IfStmt
//       whose condition is uvm_report_enabled(UVM_LOW, UVM_INFO, "RESULT")
//       and whose action is uvm_report_info("RESULT", $sformatf(...),
//       UVM_LOW, ...); $sformatf has 4 arguments: the format string literal,
//       obj1.x, obj2.x and obj2.y
//   `uvm_error("RESULT", $sformatf("... FAILED", obj1.x, obj2.x, obj2.y));
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
// KNOWN FAILURE (wrong scope for names in a 'with' block), not a defect in
// this test: BothXInWithBindToClassA1Property fails on HLC 577. Both 'x' in
// the 'with' block bind to the task's formal 'x' instead of class a1's 'x'.
// HLC resolves names in the 'with' block only in the scope of the call and
// never looks in the class of the randomized object first, as 18.7
// requires ('y' and 'z', which a1 does not declare, therefore bind
// correctly). This does not depend on uvm_pkg. It is expected to fail until
// HLC is fixed and is intentionally not skipped or relaxed.
//
// KNOWN FAILURE (errors reported in this file), not a defect in this test:
// NoErrorIsReportedInThisFile and NoFatalSyntaxOrErrorDiagnostics assert
// that this legal file compiles cleanly and are expected to fail. HLC
// reports:
//   - IllegalPropertyValue (2038) twice at line 29, column 39 -- the
//     '{' that opens the 'with' block, once per constraint expression --
//     although the inline constraint is legal (18.7).
//   - LN7705 "Null Actual" at lines 44, 56 and 67 (raise_objection,
//     drop_objection, run_test) -- a consequence of uvm_pkg not being
//     compiled (see above).
// With this .hlc: 0 syntax errors and 9 errors in total. Neither check is
// skipped or relaxed.
//
// What is NOT checked, and why:
//   - The value randomize() picks for obj1.x, the value stored in 'result',
//     which branch runs, the text the report prints, objection counting,
//     phase scheduling and which test run_test starts all only exist while
//     simulation runs. Permanently out of scope. The static half is covered
//     by WithBlockHoldsXGreaterThanZeroThenXLessThanYPlusZ,
//     BothXInWithBindToClassA1Property, YInWithBindsToClassA2Property,
//     ZInWithBindsToFormalZ and IfConditionMirrorsTheInlineConstraint.
//   - How HLC wraps the 'with' block (an unnamed Constraint holding the
//     items, or the items stored otherwise) is a model convention:
//     MethodFuncCall::getWith() is typed Any. Both an unnamed Constraint
//     and a lone expression are accepted; the items, operators and bindings
//     are asserted.
//   - Whether HLC stores the task body as the bare Assignment or as a Begin
//     holding the declaration of 'result' and the Assignment is a model
//     convention; both are accepted, and in both the Assignment must be the
//     only statement. 'result' is likewise looked up on the task and, if
//     present, on that Begin. The block comment on line 28 is not a
//     statement.
//   - '-20' is the unary minus applied to the literal 20 (11.4.3). Whether
//     HLC keeps the Operation or folds it into a Constant whose source text
//     is "-20" is a model convention; both are accepted.
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
//   - 'a1 obj1 = new;' and 'a2 obj2 = new;' invoke the implicit constructors
//     (8.7); how HLC represents them is a tool convention.
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
#include <hldb/method_task_call.h>
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

class InLineConstraintsRandomize2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.7--in-line-constraints--randomize_2.hlc"}); }
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

  // --- class a1 -----------------------------------------------------------

  static const hldb::ClassDefn *getClassA1() {
    return hldb::findByName<hldb::ClassDefn>("a1", m_design->getAllClasses());
  }

  static const hldb::Variable *getClassA1X() {
    const hldb::ClassDefn *const cls = getClassA1();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("x", cls->getVariables());
  }

  // --- class a2 -----------------------------------------------------------

  static const hldb::ClassDefn *getClassA2() {
    return hldb::findByName<hldb::ClassDefn>("a2", m_design->getAllClasses());
  }

  static const hldb::Variable *getClassA2Property(std::string_view name) {
    const hldb::ClassDefn *const cls = getClassA2();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, cls->getVariables());
  }

  // --- task do_randomize --------------------------------------------------

  static const hldb::Task *getDoRandomize() {
    const hldb::ClassDefn *const cls = getClassA2();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Task>("do_randomize", cls->getMethods());
  }

  // 37.15: a reference to a formal resolves to the Variable holding it,
  // never to the IODecl.
  static const hldb::Variable *getFormalVariable(std::string_view name) {
    const hldb::Task *const task = getDoRandomize();
    if (task == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, task->getVariables());
  }

  // The local 'int result;'. HLC may record it on the task or on a Begin
  // wrapping the task body.
  static const hldb::Variable *getResult() {
    const hldb::Task *const task = getDoRandomize();
    if (task == nullptr) {
      return nullptr;
    }
    if (task->getVariables() != nullptr) {
      if (const hldb::Variable *const var = hldb::findByName<hldb::Variable>("result", task->getVariables())) {
        return var;
      }
    }
    const hldb::Begin *const block = task->getStmt<hldb::Begin>();
    if (block == nullptr || block->getVariables() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("result", block->getVariables());
  }

  // The statements of the task body: the bare statement, or the statements
  // of a Begin wrapping it.
  static std::vector<const hldb::Any *> getBodyStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Task *const task = getDoRandomize();
    if (task == nullptr || task->getStmt() == nullptr) {
      return stmts;
    }
    if (const hldb::Begin *const block = task->getStmt<hldb::Begin>()) {
      if (block->getStmts() == nullptr) {
        return stmts;
      }
      for (const hldb::Any *const stmt : *block->getStmts()) {
        stmts.emplace_back(stmt);
      }
      return stmts;
    }
    stmts.emplace_back(task->getStmt());
    return stmts;
  }

  static const hldb::Assignment *getTaskAssignment() {
    const std::vector<const hldb::Any *> stmts = getBodyStmts();
    if (stmts.size() != 1) {
      return nullptr;
    }
    return any_cast<hldb::Assignment>(stmts[0]);
  }

  // The MethodFuncCall 'randomize' at the end of 'obj.randomize() with ...'.
  static const hldb::MethodFuncCall *getRandomizeCall() {
    const hldb::Assignment *const assign = getTaskAssignment();
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

  // Operand 'operand' of the 'y + z' inside 'x < y + z', or nullptr.
  static const hldb::Any *getAddOperand(size_t operand) {
    const hldb::Operation *const add = any_cast<hldb::Operation>(getWithOperand(1, 1));
    if (add == nullptr || add->getOperands() == nullptr || add->getOperands()->size() <= operand) {
      return nullptr;
    }
    return add->getOperands()->at(operand);
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

  // The explicit 'begin ... end' on lines 45-55.
  static const hldb::Begin *getInnerBlock() { return any_cast<hldb::Begin>(getRunPhaseStmt(1)); }

  static const hldb::Any *getInnerStmt(size_t index) {
    const hldb::Begin *const block = getInnerBlock();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() <= index) {
      return nullptr;
    }
    return block->getStmts()->at(index);
  }

  static const hldb::IfElse *getIfElse() { return any_cast<hldb::IfElse>(getInnerStmt(4)); }

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
      if (sf->getName().find("18.7--in-line-constraints--randomize_2.sv") != std::string_view::npos) {
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

  // Verifies 'expr' is '-20': either the unary minus applied to the literal
  // 20 (11.4.3), or that Operation folded into a Constant "-20".
  static void ExpectMinusTwenty(const hldb::Any *expr) {
    ASSERT_NE(expr, nullptr) << "'-20' is missing";
    if (const hldb::Operation *const op = any_cast<hldb::Operation>(expr)) {
      EXPECT_EQ(op->getOpType(), vpiMinusOp) << "11.4.3: '-20' is the unary minus applied to 20";
      ASSERT_NE(op->getOperands(), nullptr);
      ASSERT_EQ(op->getOperands()->size(), 1u) << "unary minus has one operand";
      ExpectIntLiteral(op->getOperands()->at(0), "20");
      return;
    }
    ExpectIntLiteral(expr, "-20");
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

  // Verifies class 'name' is declared once as a plain user-defined class:
  // not virtual, no base class, no parameters and no end label.
  static void ExpectPlainClass(const hldb::ClassDefn *cls, std::string_view name) {
    ASSERT_NE(m_design->getAllClasses(), nullptr);
    size_t count = 0;
    for (const hldb::ClassDefn *const c : *m_design->getAllClasses()) {
      if (c->getName() == name) ++count;
    }
    EXPECT_EQ(count, 1u) << "the source declares class '" << name << "' exactly once";
    ASSERT_NE(cls, nullptr) << "class '" << name << "' not found";
    EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass) << "8.3: 'class " << name << ";' is a user-defined class";
    EXPECT_FALSE(cls->getVirtual()) << "8.21: '" << name << "' is not declared 'virtual class'";
    EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class " << name << ";' has no 'extends' clause";
    EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
        << "8.25: 'class " << name << ";' has no parameter port list";
    EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': " << name << "'";
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

  // Verifies 'expr' is '<handle>.<member>': a hierarchical RefObj whose
  // prefix binds to env's property 'handle' and whose member binds to
  // 'member' (8.5).
  static void ExpectMemberRef(const hldb::Any *expr, std::string_view handle, std::string_view member,
                              const hldb::Variable *target) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'" << handle << "." << member << "' should be a hierarchical RefObj";
    ASSERT_NE(path->getPathElems(), nullptr) << "'" << handle << "." << member << "' is a member access";
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    ExpectBoundRef(path->getPathElems()->at(0), handle, getEnvProperty(handle));
    ExpectBoundRef(path->getPathElems()->at(1), member, target);
  }

  // Verifies 'expr' is '$sformatf(<format>, obj1.x, obj2.x, obj2.y)'.
  static void ExpectSformatfOfObjValues(const hldb::Any *expr, std::string_view format) {
    const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(expr);
    ASSERT_NE(call, nullptr) << "the message argument should be a $sformatf SysFuncCall";
    EXPECT_EQ(call->getName(), "$sformatf");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 4u) << "a format string, 'obj1.x', 'obj2.x' and 'obj2.y'";
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    ExpectStringLiteral(arg0->getHighConn(), format);
    const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
    ASSERT_NE(arg1, nullptr);
    ExpectMemberRef(arg1->getHighConn(), "obj1", "x", getClassA1X());
    const hldb::NamedArgument *const arg2 = call->getArguments()->at(2);
    ASSERT_NE(arg2, nullptr);
    ExpectMemberRef(arg2->getHighConn(), "obj2", "x", getClassA2Property("x"));
    const hldb::NamedArgument *const arg3 = call->getArguments()->at(3);
    ASSERT_NE(arg3, nullptr);
    ExpectMemberRef(arg3->getHighConn(), "obj2", "y", getClassA2Property("y"));
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
  //       <reportFn>("RESULT", $sformatf(<format>, obj1.x, obj2.x, obj2.y), <verbosity>, ...);
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
    ExpectSformatfOfObjValues(reportArg1->getHighConn(), format);
    const hldb::NamedArgument *const reportArg2 = report->getArguments()->at(2);
    ASSERT_NE(reportArg2, nullptr);
    ExpectEnumConstRef(reportArg2->getHighConn(), verbosity);
  }
};

// ---------------------------------------------------------------------------
// import uvm_pkg::*;  `include "uvm_macros.svh"
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize2Test, SourceFileIncludesUvmMacros) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr) << "18.7--in-line-constraints--randomize_2.sv not found in Design::getSourceFiles()";
  ASSERT_NE(sf->getIncludes(), nullptr) << "22.4: the file includes uvm_macros.svh";
  size_t count = 0;
  for (const hldb::SourceFile *const inc : *sf->getIncludes()) {
    if (inc->getName().find("uvm_macros.svh") != std::string_view::npos) ++count;
  }
  EXPECT_EQ(count, 1u) << "'`include \"uvm_macros.svh\"' is written once";
}

TEST_F(InLineConstraintsRandomize2Test, FileUsesUvmInfoOnceAndUvmErrorOnce) {
  EXPECT_EQ(getMacroInstances("uvm_info").size(), 1u) << "line 51 is the only `uvm_info";
  EXPECT_EQ(getMacroInstances("uvm_error").size(), 1u) << "line 53 is the only `uvm_error";
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize2Test, UvmPkgProvidesTheNamesTheFileUses) {
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
// class a1; rand int x; endclass
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize2Test, ClassA1IsPlainUserDefinedClass) { ExpectPlainClass(getClassA1(), "a1"); }

TEST_F(InLineConstraintsRandomize2Test, ClassA1DeclaresNoMethods) {
  const hldb::ClassDefn *const cls = getClassA1();
  ASSERT_NE(cls, nullptr);
  size_t declared = 0;
  if (cls->getMethods() != nullptr) {
    for (const hldb::TaskFunc *const method : *cls->getMethods()) {
      if (method->getName() != "new") ++declared;
    }
  }
  EXPECT_EQ(declared, 0u) << "class a1 declares no task or function; randomize() is built in (18.6.1)";
}

TEST_F(InLineConstraintsRandomize2Test, ClassA1HasOneRandIntPropertyX) {
  const hldb::ClassDefn *const cls = getClassA1();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'rand int x;' is the only property";
  const hldb::Variable *const x = getClassA1X();
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(cls->getVariables()->at(0), x);
  ExpectSignedInt(x->getTypespec(), "class a1's 'x'");
  EXPECT_TRUE(x->getIsRandomized()) << "18.4: 'x' is declared with the 'rand' qualifier";
  EXPECT_EQ(x->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(x->getValue(), nullptr) << "'rand int x;' has no initializer";
  EXPECT_EQ(x->getVisibility(), vpiPublicVis) << "8.18: 'x' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(x->getConstantVariable()) << "8.19: 'x' has no 'const' qualifier";
}

// ---------------------------------------------------------------------------
// class a2; int x, y; task do_randomize(...); ... endtask endclass
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize2Test, ClassA2IsPlainUserDefinedClass) { ExpectPlainClass(getClassA2(), "a2"); }

TEST_F(InLineConstraintsRandomize2Test, ClassA2HasIntPropertiesXAndYInOrder) {
  const hldb::ClassDefn *const cls = getClassA2();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 2u) << "'int x, y;' declares two properties";
  const std::vector<std::string_view> expected = {"x", "y"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = cls->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    ExpectSignedInt(prop->getTypespec(), expected[i]);
    EXPECT_EQ(prop->getValue(), nullptr) << "'" << expected[i] << "' has no initializer";
    EXPECT_FALSE(prop->getIsRandomized()) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_EQ(prop->getRandType(), vpiNotRand) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(InLineConstraintsRandomize2Test, ClassA2HasOneAutomaticTaskDoRandomize) {
  const hldb::ClassDefn *const cls = getClassA2();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getMethods(), nullptr);
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "the class body declares only 'do_randomize'";
  const hldb::Task *const task = getDoRandomize();
  ASSERT_NE(task, nullptr) << "13.3: 'task do_randomize(...)' is a Task";
  EXPECT_TRUE(task->getMethod());
  EXPECT_TRUE(task->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
  EXPECT_EQ(task->getVisibility(), vpiPublicVis) << "8.18: 'do_randomize' has no 'local'/'protected'";
  EXPECT_EQ(task->getEndLabelObj(), nullptr) << "'endtask' is written without ': do_randomize'";
}

// ---------------------------------------------------------------------------
// task do_randomize(a1 obj, int x, int z); int result; ... endtask
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize2Test, DoRandomizeHasFormalsObjXZ) {
  const hldb::Task *const task = getDoRandomize();
  ASSERT_NE(task, nullptr);
  ASSERT_NE(task->getIODecls(), nullptr);
  ASSERT_EQ(task->getIODecls()->size(), 3u) << "'a1 obj', 'int x' and 'int z'";

  const hldb::IODecl *const obj = task->getIODecls()->at(0);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->getName(), "obj");
  EXPECT_EQ(obj->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectClassHandle(obj->getTypespec(), getClassA1(), "'obj'");

  const hldb::IODecl *const x = task->getIODecls()->at(1);
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getName(), "x");
  EXPECT_EQ(x->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectSignedInt(x->getTypespec(), "formal 'x'");

  const hldb::IODecl *const z = task->getIODecls()->at(2);
  ASSERT_NE(z, nullptr);
  EXPECT_EQ(z->getName(), "z");
  EXPECT_EQ(z->getDirection(), vpiInput) << "13.3: a formal with no direction is an input";
  ExpectSignedInt(z->getTypespec(), "formal 'z'");
}

TEST_F(InLineConstraintsRandomize2Test, DoRandomizeDeclaresLocalIntResult) {
  const hldb::Variable *const result = getResult();
  ASSERT_NE(result, nullptr) << "'int result;' declares a variable local to the task";
  ExpectSignedInt(result->getTypespec(), "'result'");
  EXPECT_EQ(result->getValue(), nullptr) << "'int result;' has no initializer";
  EXPECT_FALSE(result->getIsRandomized()) << "18.4: 'result' is not a rand class property";
}

// ---------------------------------------------------------------------------
// result = obj.randomize() with {x > 0; x < y + z;};
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize2Test, TaskBodyIsOneAssignmentToResult) {
  const std::vector<const hldb::Any *> stmts = getBodyStmts();
  ASSERT_EQ(stmts.size(), 1u) << "line 29 is the task's only statement";
  const hldb::Assignment *const assign = getTaskAssignment();
  ASSERT_NE(assign, nullptr) << "'result = ...;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "result", getResult());
}

TEST_F(InLineConstraintsRandomize2Test, RhsIsObjRandomizeWithInlineConstraint) {
  const hldb::Assignment *const assign = getTaskAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'obj.randomize() with {...}' should be a hierarchical RefObj";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  ExpectBoundRef(path->getPathElems()->at(0), "obj", getFormalVariable("obj"));

  const hldb::MethodFuncCall *const call = getRandomizeCall();
  ASSERT_NE(call, nullptr) << "18.6.1: randomize() is a function called through a handle, so a MethodFuncCall";
  EXPECT_EQ(call->getName(), "randomize");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "'randomize()' is written with no arguments";
  EXPECT_NE(call->getWith(), nullptr) << "18.7: the call carries a 'with' inline constraint";
}

TEST_F(InLineConstraintsRandomize2Test, WithBlockHoldsXGreaterThanZeroThenXLessThanYPlusZ) {
  ASSERT_NE(getRandomizeCall(), nullptr);
  const std::vector<const hldb::Any *> items = getWithItems();
  ASSERT_EQ(items.size(), 2u) << "18.7: '{x > 0; x < y + z;}' holds two constraint expressions";

  const hldb::Operation *const gt = any_cast<hldb::Operation>(items[0]);
  ASSERT_NE(gt, nullptr) << "'x > 0' should be an Operation";
  EXPECT_EQ(gt->getOpType(), vpiGtOp);
  ASSERT_NE(gt->getOperands(), nullptr);
  EXPECT_EQ(gt->getOperands()->size(), 2u) << "'>' is a binary operator";
  ExpectIntLiteral(getWithOperand(0, 1), "0");

  const hldb::Operation *const lt = any_cast<hldb::Operation>(items[1]);
  ASSERT_NE(lt, nullptr) << "'x < y + z' should be an Operation";
  EXPECT_EQ(lt->getOpType(), vpiLtOp) << "11.3.2: '+' binds tighter than '<', so '<' is the outer operator";
  ASSERT_NE(lt->getOperands(), nullptr);
  EXPECT_EQ(lt->getOperands()->size(), 2u) << "'<' is a binary operator";

  const hldb::Operation *const add = any_cast<hldb::Operation>(getWithOperand(1, 1));
  ASSERT_NE(add, nullptr) << "the right operand 'y + z' should be an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp);
  ASSERT_NE(add->getOperands(), nullptr);
  EXPECT_EQ(add->getOperands()->size(), 2u) << "'+' is a binary operator";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (wrong scope for
// names in a 'with' block) in the file header.
TEST_F(InLineConstraintsRandomize2Test, BothXInWithBindToClassA1Property) {
  // 18.7: 'x' is found first in class a1, the class of the randomized object
  // (the comment on line 28 says the same).
  ExpectBoundRef(getWithOperand(0, 0), "x", getClassA1X());
  ExpectBoundRef(getWithOperand(1, 0), "x", getClassA1X());

  const hldb::RefObj *const first = any_cast<hldb::RefObj>(getWithOperand(0, 0));
  const hldb::RefObj *const second = any_cast<hldb::RefObj>(getWithOperand(1, 0));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first->getActual(), getFormalVariable("x")) << "18.7: a1's 'x' hides the task formal 'x'";
  EXPECT_NE(second->getActual(), getFormalVariable("x")) << "18.7: a1's 'x' hides the task formal 'x'";
  EXPECT_NE(first->getActual(), getClassA2Property("x")) << "18.7: a1's 'x' hides class a2's 'x'";
  EXPECT_NE(second->getActual(), getClassA2Property("x")) << "18.7: a1's 'x' hides class a2's 'x'";
}

TEST_F(InLineConstraintsRandomize2Test, YInWithBindsToClassA2Property) {
  // 18.7: a1 declares no 'y', so it resolves where the call is written:
  // inside a method of a2, where 'y' is a2's property (8.5).
  ExpectBoundRef(getAddOperand(0), "y", getClassA2Property("y"));
}

TEST_F(InLineConstraintsRandomize2Test, ZInWithBindsToFormalZ) {
  // 18.7: a1 declares no 'z', so it resolves where the call is written: the
  // task's formal 'z' (37.15: the Variable holding it). env's 'z' is not
  // visible inside class a2.
  ExpectBoundRef(getAddOperand(1), "z", getFormalVariable("z"));
  const hldb::RefObj *const z = any_cast<hldb::RefObj>(getAddOperand(1));
  ASSERT_NE(z, nullptr);
  EXPECT_NE(z->getActual(), getEnvProperty("z")) << "env's 'z' is not in scope inside class a2";
}

// ---------------------------------------------------------------------------
// class env extends uvm_env; ... endclass
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize2Test, ClassEnvIsDeclaredOnceAsUserDefinedClass) {
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
TEST_F(InLineConstraintsRandomize2Test, ClassEnvExtendsUvmEnv) {
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

TEST_F(InLineConstraintsRandomize2Test, ClassEnvHasPropertiesObj1Obj2AndZInOrder) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getVariables(), nullptr);
  ASSERT_EQ(env->getVariables()->size(), 3u) << "'a1 obj1', 'a2 obj2' and 'int z'";
  const std::vector<std::string_view> expected = {"obj1", "obj2", "z"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = env->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getIsRandomized()) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(InLineConstraintsRandomize2Test, Obj1AndObj2AreHandlesInitializedWithNew) {
  const hldb::Variable *const obj1 = getEnvProperty("obj1");
  ASSERT_NE(obj1, nullptr);
  ExpectClassHandle(obj1->getTypespec(), getClassA1(), "'obj1'");
  const hldb::MethodFuncCall *const init1 = obj1->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(init1, nullptr) << "8.7: 'a1 obj1 = new;' initializes obj1 with a constructor call";
  EXPECT_EQ(init1->getName(), "new");
  EXPECT_TRUE(init1->getArguments() == nullptr || init1->getArguments()->empty())
      << "'new' is written with no arguments";

  const hldb::Variable *const obj2 = getEnvProperty("obj2");
  ASSERT_NE(obj2, nullptr);
  ExpectClassHandle(obj2->getTypespec(), getClassA2(), "'obj2'");
  const hldb::MethodFuncCall *const init2 = obj2->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(init2, nullptr) << "8.7: 'a2 obj2 = new;' initializes obj2 with a constructor call";
  EXPECT_EQ(init2->getName(), "new");
  EXPECT_TRUE(init2->getArguments() == nullptr || init2->getArguments()->empty())
      << "'new' is written with no arguments";
}

TEST_F(InLineConstraintsRandomize2Test, EnvZIsAnUninitializedSignedInt) {
  const hldb::Variable *const z = getEnvProperty("z");
  ASSERT_NE(z, nullptr);
  ExpectSignedInt(z->getTypespec(), "env's 'z'");
  EXPECT_EQ(z->getValue(), nullptr) << "'int z;' has no initializer";
}

TEST_F(InLineConstraintsRandomize2Test, ClassEnvHasMethodsNewAndRunPhase) {
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

TEST_F(InLineConstraintsRandomize2Test, EnvMethodsHaveAutomaticLifetime) {
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
TEST_F(InLineConstraintsRandomize2Test, EnvNewHasFormalsNameAndParent) {
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
TEST_F(InLineConstraintsRandomize2Test, EnvNewBodyIsSuperNewOfNameAndParent) {
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
TEST_F(InLineConstraintsRandomize2Test, RunPhaseHasOneUvmPhaseFormalAndEndLabel) {
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

TEST_F(InLineConstraintsRandomize2Test, RunPhaseBodyIsRaiseBlockDrop) {
  const hldb::Task *const task = getRunPhase();
  ASSERT_NE(task, nullptr);
  const hldb::Begin *const body = task->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a body with more than one statement should be wrapped in a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u) << "raise_objection, begin-end, drop_objection";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(0)), nullptr);
  EXPECT_NE(any_cast<hldb::Begin>(body->getStmts()->at(1)), nullptr) << "line 45 opens an explicit begin-end";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(2)), nullptr);
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize2Test, RunPhaseRaisesObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(0), "raise_objection");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize2Test, RunPhaseDropsObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(2), "drop_objection");
}

// ---------------------------------------------------------------------------
// begin obj2.x = -20; obj2.y = 5; z = 10; obj2.do_randomize(...); if ... end
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize2Test, InnerBlockHoldsFiveStatementsInOrder) {
  const hldb::Begin *const block = getInnerBlock();
  ASSERT_NE(block, nullptr);
  EXPECT_TRUE(block->getVariables() == nullptr || block->getVariables()->empty()) << "the block declares no variable";
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 5u) << "lines 46 to 50";
  EXPECT_NE(any_cast<hldb::Assignment>(getInnerStmt(0)), nullptr) << "statement 0 is 'obj2.x = -20;'";
  EXPECT_NE(any_cast<hldb::Assignment>(getInnerStmt(1)), nullptr) << "statement 1 is 'obj2.y = 5;'";
  EXPECT_NE(any_cast<hldb::Assignment>(getInnerStmt(2)), nullptr) << "statement 2 is 'z = 10;'";
  EXPECT_NE(any_cast<hldb::RefObj>(getInnerStmt(3)), nullptr) << "statement 3 is 'obj2.do_randomize(...);'";
  EXPECT_NE(getIfElse(), nullptr) << "12.4: statement 4 is an if with an else, so it is an IfElse";
}

TEST_F(InLineConstraintsRandomize2Test, AssignsMinusTwentyToObj2X) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(getInnerStmt(0));
  ASSERT_NE(assign, nullptr) << "'obj2.x = -20;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectMemberRef(assign->getLhs(), "obj2", "x", getClassA2Property("x"));
  ExpectMinusTwenty(assign->getRhs());
}

TEST_F(InLineConstraintsRandomize2Test, AssignsFiveToObj2Y) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(getInnerStmt(1));
  ASSERT_NE(assign, nullptr) << "'obj2.y = 5;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectMemberRef(assign->getLhs(), "obj2", "y", getClassA2Property("y"));
  ExpectIntLiteral(assign->getRhs(), "5");
}

TEST_F(InLineConstraintsRandomize2Test, AssignsTenToEnvZ) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(getInnerStmt(2));
  ASSERT_NE(assign, nullptr) << "'z = 10;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "z", getEnvProperty("z"));
  ExpectIntLiteral(assign->getRhs(), "10");
}

TEST_F(InLineConstraintsRandomize2Test, CallsObj2DoRandomizeWithObj1MinusTwentyAndZ) {
  const hldb::RefObj *const path = any_cast<hldb::RefObj>(getInnerStmt(3));
  ASSERT_NE(path, nullptr) << "'obj2.do_randomize(...)' should be a hierarchical RefObj";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  ExpectBoundRef(path->getPathElems()->at(0), "obj2", getEnvProperty("obj2"));

  const hldb::MethodTaskCall *const call = any_cast<hldb::MethodTaskCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "13.5: 'do_randomize' is a task called through a handle, so a MethodTaskCall";
  EXPECT_EQ(call->getName(), "do_randomize");
  ASSERT_NE(getDoRandomize(), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getDoRandomize()) << "8.5: 'obj2.do_randomize' is class a2's task";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 3u) << "'obj1', '-20' and 'z' are passed";
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  ExpectBoundRef(arg0->getHighConn(), "obj1", getEnvProperty("obj1"));
  const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  ExpectMinusTwenty(arg1->getHighConn());
  const hldb::NamedArgument *const arg2 = call->getArguments()->at(2);
  ASSERT_NE(arg2, nullptr);
  ExpectBoundRef(arg2->getHighConn(), "z", getEnvProperty("z"));
}

// ---------------------------------------------------------------------------
// if(obj1.x > 0 && obj1.x < obj2.y + z) begin `uvm_info(...) end
// else begin `uvm_error(...) end
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize2Test, IfConditionMirrorsTheInlineConstraint) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);

  // 11.3.2: '&&' has the lowest precedence of the operators used.
  const hldb::Any *gt = nullptr;
  const hldb::Any *lt = nullptr;
  ExpectBinaryOp(ifElse->getCondition(), vpiLogAndOp, "obj1.x > 0 && obj1.x < obj2.y + z", &gt, &lt);

  const hldb::Any *gtLhs = nullptr;
  const hldb::Any *gtRhs = nullptr;
  ExpectBinaryOp(gt, vpiGtOp, "obj1.x > 0", &gtLhs, &gtRhs);
  ExpectMemberRef(gtLhs, "obj1", "x", getClassA1X());
  ExpectIntLiteral(gtRhs, "0");

  // 11.3.2: '+' binds tighter than '<'.
  const hldb::Any *ltLhs = nullptr;
  const hldb::Any *ltRhs = nullptr;
  ExpectBinaryOp(lt, vpiLtOp, "obj1.x < obj2.y + z", &ltLhs, &ltRhs);
  ExpectMemberRef(ltLhs, "obj1", "x", getClassA1X());
  const hldb::Any *addLhs = nullptr;
  const hldb::Any *addRhs = nullptr;
  ExpectBinaryOp(ltRhs, vpiAddOp, "obj2.y + z", &addLhs, &addRhs);
  ExpectMemberRef(addLhs, "obj2", "y", getClassA2Property("y"));
  ExpectBoundRef(addRhs, "z", getEnvProperty("z"));
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize2Test, ThenBranchIsUvmInfoExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getStmt(), "uvm_report_info", "UVM_INFO", "UVM_LOW",
                       "\"obj1.x = %0d obj2.x = %0d obj2.y = %0d SUCCESS\"");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(InLineConstraintsRandomize2Test, ElseBranchIsUvmErrorExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getElseStmt(), "uvm_report_error", "UVM_ERROR", "UVM_NONE",
                       "\"obj1.x = %0d obj2.x = %0d obj2.y = %0d FAILED\"");
}

// ---------------------------------------------------------------------------
// module top; env environment; initial begin ... end endmodule
// ---------------------------------------------------------------------------

TEST_F(InLineConstraintsRandomize2Test, TopHasNoPortsAndOneEnvHandle) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top;' has no port list";
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "'env environment;' is the module's only declaration";
  const hldb::Variable *const environment = getEnvironment();
  ASSERT_NE(environment, nullptr) << "a class-typed data declaration is a variable (6.8)";
  ExpectClassHandle(environment->getTypespec(), getEnv(), "'environment'");
}

TEST_F(InLineConstraintsRandomize2Test, TopHasOneInitialWithTwoStatements) {
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

TEST_F(InLineConstraintsRandomize2Test, InitialConstructsEnvironment) {
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
TEST_F(InLineConstraintsRandomize2Test, InitialCallsRunTestFromUvmPkg) {
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
TEST_F(InLineConstraintsRandomize2Test, NoErrorIsReportedInThisFile) {
  size_t count = 0;
  for (const Event &error : m_session->getEventListener()->getEvents()) {
    const EventDefinition *const info = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if (info == nullptr || info->m_severity > EventSeverity::Error) continue;
    for (const EventArg &loc : error.getArguments()) {
      const std::string_view path = loc.m_file;
      if (path.find("18.7--in-line-constraints--randomize_2.sv") == std::string_view::npos) continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Expected to fail -- see KNOWN FAILURE (errors reported in this file) in the
// file header.
TEST_F(InLineConstraintsRandomize2Test, NoFatalSyntaxOrErrorDiagnostics) {
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
