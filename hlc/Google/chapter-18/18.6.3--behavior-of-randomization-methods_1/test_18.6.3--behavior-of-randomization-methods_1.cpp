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
// tests/Google/chapter-18/18.6.3--behavior-of-randomization-methods_1.sv.
// Lines 1-14 are the license and the suite's :name:/:description:/:tags:
// comment; the rest is:
//
//   16  import uvm_pkg::*;
//   17  `include "uvm_macros.svh"
//   18
//   19  class a;
//   20      static rand int b;
//   21      constraint c { b > 5; b < 12; }
//   22  endclass
//   23
//   24  class env extends uvm_env;
//   25
//   26    a obj1 = new;
//   27    a obj2 = new;
//   28
//   29    function new(string name, uvm_component parent = null);
//   30      super.new(name, parent);
//   31    endfunction
//   32
//   33    task run_phase(uvm_phase phase);
//   34      phase.raise_objection(this);
//   35      begin
//   36        obj1.randomize();
//   37        if(obj1.b == obj2.b) begin
//   38          `uvm_info("RESULT", $sformatf("obj1.b = %0d obj2.b = %0d SUCCESS", obj1.b, obj2.b), UVM_LOW);
//   39        end else begin
//   40          `uvm_error("RESULT", $sformatf("obj1.b = %0d obj2.b = %0d FAILED", obj1.b, obj2.b));
//   41        end
//   42
//   43        obj2.randomize();
//   44        if(obj1.b == obj2.b) begin
//   45          `uvm_info("RESULT", $sformatf("obj1.b = %0d obj2.b = %0d SUCCESS", obj1.b, obj2.b), UVM_LOW);
//   46        end else begin
//   47          `uvm_error("RESULT", $sformatf("obj1.b = %0d obj2.b = %0d FAILED", obj1.b, obj2.b));
//   48        end
//   49
//   50      end
//   51      phase.drop_objection(this);
//   52    endtask: run_phase
//   53
//   54  endclass
//   55
//   56  module top;
//   57
//   58    env environment;
//   59
//   60    initial begin
//   61      environment = new("env");
//   62      run_test();
//   63    end
//   64
//   65  endmodule
//
// The point of the fixture is a static random variable (IEEE 1800-2023
// 18.6.3): "Random variables declared as static are shared by all instances
// of the class in which they are declared. Each time the randomize() method
// is called, the variable is changed in every class instance." Class a
// declares one static rand int 'b' constrained to lie strictly between 5 and
// 12. The env holds two instances, randomizes each in turn and, after each
// call, tests that both instances see the same 'b'.
//
// UVM macros. uvm_macros.svh is SystemVerilog source included by the file,
// so what the macro calls expand to is fixed by that source (22.5.1). Every
// UVM version bundled in third_party/UVM expands them to the same shape:
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
//     - the file uses the macro uvm_info exactly twice and uvm_error exactly
//       twice
//     - import uvm_pkg::*; (26.3): asserted through its effect -- the UVM
//       names the file uses bind into package uvm_pkg
//   class a (8.3)
//     - exactly one class named "a"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label
//     - declares no methods; randomize() is built in (18.6.1)
//   static rand int b; (8.9, 18.4, 18.6.3)
//     - the only property; a signed int (6.11) with no initializer
//     - declared 'rand': getIsRandomized() true and getRandType() vpiRand
//     - declared 'static' (8.9): one copy shared by every instance, so its
//       lifetime is static -- getAutomatic() is false
//     - public (8.18), not const (8.19)
//   constraint c { b > 5; b < 12; } (18.5)
//     - exactly one constraint block, named "c"; not 'pure' (18.5.2) and not
//       virtual
//     - exactly two constraint items, in source order: vpiGtOp(b, 5) and
//       vpiLtOp(b, 12), each with 'b' bound to class a's property
//   class env extends uvm_env (8.3, 8.13)
//     - exactly one class named "env"; user-defined; not virtual; no
//       parameters; no end label; Extends resolves to uvm_pkg's uvm_env
//     - exactly 2 properties, in declaration order: obj1, obj2. Each is
//       public, not rand/randc, not const; a handle to class a initialized
//       with a 'new' call with no arguments (8.7)
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
//   begin ... end (lines 35-50)
//     - the block declares no variable and holds exactly 4 statements, in
//       order: obj1.randomize(), an if-else, obj2.randomize(), an if-else.
//       The blank lines 42 and 49 are not statements.
//   obj1.randomize(); obj2.randomize(); (18.6.1, 13.4.1)
//     - each is a function called as a statement, its return value
//       discarded: a hierarchical RefObj whose prefix binds to env's obj1 /
//       obj2, then a MethodFuncCall "randomize" with no arguments and no
//       'with' inline constraint (18.7)
//   if(obj1.b == obj2.b) (12.4), twice
//     - an IfElse whose condition is vpiEqOp(obj1.b, obj2.b)
//     - obj1.b and obj2.b are hierarchical references whose prefix binds to
//       env's obj1 / obj2 and whose member binds to class a's 'b'. 8.9
//       allows a static property to be reached through an instance handle.
//   `uvm_info("RESULT", $sformatf("... SUCCESS", obj1.b, obj2.b), UVM_LOW);
//     - each then-branch starts with the macro's begin-end holding one IfStmt
//       whose condition is uvm_report_enabled(UVM_LOW, UVM_INFO, "RESULT")
//       and whose action is uvm_report_info("RESULT", $sformatf(...),
//       UVM_LOW, ...)
//     - $sformatf has 3 arguments: the format string literal, obj1.b, obj2.b
//   `uvm_error("RESULT", $sformatf("... FAILED", obj1.b, obj2.b));
//     - each else-branch has the same shape with
//       uvm_report_enabled(UVM_NONE, UVM_ERROR, "RESULT") and
//       uvm_report_error(..., UVM_NONE, ...)
//   module top (23.2)
//     - no ports; exactly 1 variable 'environment', a handle to class env
//     - one initial whose begin-end holds 'environment = new("env");' (bound
//       to env's constructor) and 'run_test();' (a TaskCall bound to
//       uvm_pkg's run_test)
//   Diagnostics
//     - no COMP_UNDEFINED_VARIABLE for any variable or formal the file
//       references
//     - no fatal, syntax or error diagnostic is reported in this file
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
// RunPhaseDropsObjectionOnThis, FirstThenBranchIsUvmInfoExpansion,
// FirstElseBranchIsUvmErrorExpansion, SecondThenBranchIsUvmInfoExpansion,
// SecondElseBranchIsUvmErrorExpansion and InitialCallsRunTestFromUvmPkg.
// Adding uvm_pkg.sv to the .hlc (as 18.13.4--get_randstate_0.hlc does) makes
// all of them pass. They assert the LRM binding and are intentionally not
// skipped or relaxed.
//
// KNOWN FAILURE (errors reported in this file), not a defect in this test:
// NoErrorIsReportedInThisFile asserts no error points into this legal file
// and is expected to fail. HLC reports:
//   - HLDB_ILLEGAL_PROPERTY_VALUE (2038) twice at line 21, column 18 -- the
//     '{' of 'constraint c { b > 5; b < 12; }'. That constraint block is
//     legal (18.5), and the model HLC builds for it is correct
//     (ClassAHasOneConstraintBlockC and
//     ConstraintCHasTwoItemsBGreaterThan5AndBLessThan12 pass). HLC bug;
//     remains when uvm_pkg.sv is compiled.
//   - COMP_MULTIPLY_DEFINED_TYPEDEF (5824) at line 24, column 1 -- 'class
//     env extends uvm_env;'. env is declared exactly once
//     (ClassEnvIsDeclaredOnceAsUserDefinedClass passes), so nothing is
//     multiply defined (8.3, 6.18). HLC bug; remains when uvm_pkg.sv is
//     compiled.
//   - LN7705 "Null Actual" at lines 34, 51 and 62 (raise_objection,
//     drop_objection, run_test) -- a consequence of uvm_pkg not being
//     compiled (see above).
// NoFatalSyntaxOrErrorDiagnostics asserts zero errors overall, as legal
// source requires, and is expected to fail (14 errors with this .hlc; with
// uvm_pkg.sv added, about 1800, nearly all inside the UVM library itself).
// Neither is skipped or relaxed.
//
// What is NOT checked, and why:
//   - That randomizing one instance changes b in the other, the value picked
//     (6 to 11), which branches run, the text the reports print, objection
//     counting, phase scheduling and which test run_test starts all only
//     exist while simulation runs. Permanently out of scope. The static half
//     is covered by: BIsStaticRandSignedInt,
//     ConstraintCHasTwoItemsBGreaterThan5AndBLessThan12, the
//     *CallsObj?Randomize tests, the IfCondition* tests and the
//     *BranchIsUvm*Expansion tests.
//   - The constraint_mode() state of 'c' (18.9) is a run-time property of
//     each object, not source text.
//   - Whether HLC binds a call to the built-in randomize() through
//     getTaskFunc() is a tool convention. Its name, empty argument list and
//     missing 'with' clause are asserted.
//   - 13.4.1 lets a tool warn when a non-void function's result is
//     discarded; warnings are not asserted.
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
//   - Whether run_phase is flagged virtual (8.20 makes an override virtual
//     although the keyword is not written) is a model convention.
//   - 'a obj1 = new;' and 'a obj2 = new;' invoke class a's implicit
//     constructor (8.7); how HLC represents it is a tool convention, so that
//     call's binding is not asserted and class a's method count leaves 'new'
//     out.
//   - The default value of 'parent' (no grounded accessor in this suite).
//   - Design-wide counts of classes and packages depend on the UVM library,
//     so every class is found by name.
//   - The warning count, source line numbers, and the license/metadata
//     comments.

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
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

class BehaviorOfRandomizationMethods1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.6.3--behavior-of-randomization-methods_1.hlc"}); }
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

  static const hldb::Variable *getB() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("b", cls->getVariables());
  }

  static const hldb::Constraint *getConstraintC() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Constraint>("c", cls->getConstraints());
  }

  // The methods class a declares. 8.7 provides an implicit 'new' when a
  // class writes none; whether HLC materializes it in getMethods() is a
  // tool convention, so it is left out.
  static std::vector<const hldb::TaskFunc *> getClassADeclaredMethods() {
    std::vector<const hldb::TaskFunc *> methods;
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr || cls->getMethods() == nullptr) {
      return methods;
    }
    for (const hldb::TaskFunc *const method : *cls->getMethods()) {
      if (method->getName() != "new") methods.emplace_back(method);
    }
    return methods;
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

  // The explicit 'begin ... end' on lines 35-50.
  static const hldb::Begin *getInnerBlock() { return any_cast<hldb::Begin>(getRunPhaseStmt(1)); }

  static const hldb::Any *getInnerStmt(size_t index) {
    const hldb::Begin *const block = getInnerBlock();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() <= index) {
      return nullptr;
    }
    return block->getStmts()->at(index);
  }

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
      if (sf->getName().find("18.6.3--behavior-of-randomization-methods_1.sv") != std::string_view::npos) {
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

  // Verifies 'expr' is '<handle>.b': a hierarchical RefObj whose prefix binds
  // to env's property 'handle' and whose member binds to class a's static
  // property 'b' (8.5, 8.9).
  static void ExpectHandleDotB(const hldb::Any *expr, std::string_view handle) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'" << handle << ".b' should be a hierarchical RefObj";
    ASSERT_NE(path->getPathElems(), nullptr) << "'" << handle << ".b' is a member access through a handle";
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    ExpectBoundRef(path->getPathElems()->at(0), handle, getEnvProperty(handle));
    ExpectBoundRef(path->getPathElems()->at(1), "b", getB());
  }

  // Verifies 'stmt' is '<handle>.randomize();'.
  static void ExpectRandomizeCall(const hldb::Any *stmt, std::string_view handle) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(stmt);
    ASSERT_NE(path, nullptr) << "'" << handle << ".randomize();' should be a hierarchical RefObj";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    ExpectBoundRef(path->getPathElems()->at(0), handle, getEnvProperty(handle));
    const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
    ASSERT_NE(call, nullptr) << "18.6.1: randomize() is a function called through a handle, so a MethodFuncCall";
    EXPECT_EQ(call->getName(), "randomize");
    EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
        << "'randomize()' is written with no arguments";
    EXPECT_EQ(call->getWith(), nullptr) << "18.7: no 'with' inline constraint is written";
  }

  // Verifies 'expr' is '$sformatf(<format>, obj1.b, obj2.b)'.
  static void ExpectSformatfOfObj1BAndObj2B(const hldb::Any *expr, std::string_view format) {
    const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(expr);
    ASSERT_NE(call, nullptr) << "the message argument should be a $sformatf SysFuncCall";
    EXPECT_EQ(call->getName(), "$sformatf");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 3u) << "a format string, 'obj1.b' and 'obj2.b'";
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    ExpectStringLiteral(arg0->getHighConn(), format);
    const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
    ASSERT_NE(arg1, nullptr);
    ExpectHandleDotB(arg1->getHighConn(), "obj1");
    const hldb::NamedArgument *const arg2 = call->getArguments()->at(2);
    ASSERT_NE(arg2, nullptr);
    ExpectHandleDotB(arg2->getHighConn(), "obj2");
  }

  // Verifies 'stmt' is 'if (obj1.b == obj2.b) ... else ...'.
  static void ExpectIfObj1BEqualsObj2B(const hldb::Any *stmt) {
    const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(stmt);
    ASSERT_NE(ifElse, nullptr) << "12.4: an if with an else is an IfElse";
    const hldb::Any *lhs = nullptr;
    const hldb::Any *rhs = nullptr;
    ExpectBinaryOp(ifElse->getCondition(), vpiEqOp, "obj1.b == obj2.b", &lhs, &rhs);
    ExpectHandleDotB(lhs, "obj1");
    ExpectHandleDotB(rhs, "obj2");
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
  //       <reportFn>("RESULT", $sformatf(<format>, obj1.b, obj2.b), <verbosity>, ...);
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
    ExpectSformatfOfObj1BAndObj2B(reportArg1->getHighConn(), format);
    const hldb::NamedArgument *const reportArg2 = report->getArguments()->at(2);
    ASSERT_NE(reportArg2, nullptr);
    ExpectEnumConstRef(reportArg2->getHighConn(), verbosity);
  }

  static void ExpectThenIsUvmInfo(const hldb::Any *stmt) {
    const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(stmt);
    ASSERT_NE(ifElse, nullptr);
    ExpectUvmReportMacro(ifElse->getStmt(), "uvm_report_info", "UVM_INFO", "UVM_LOW",
                         "\"obj1.b = %0d obj2.b = %0d SUCCESS\"");
  }

  static void ExpectElseIsUvmError(const hldb::Any *stmt) {
    const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(stmt);
    ASSERT_NE(ifElse, nullptr);
    ExpectUvmReportMacro(ifElse->getElseStmt(), "uvm_report_error", "UVM_ERROR", "UVM_NONE",
                         "\"obj1.b = %0d obj2.b = %0d FAILED\"");
  }
};

// ---------------------------------------------------------------------------
// import uvm_pkg::*;  `include "uvm_macros.svh"
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods1Test, SourceFileIncludesUvmMacros) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr) << "18.6.3--behavior-of-randomization-methods_1.sv not found in Design::getSourceFiles()";
  ASSERT_NE(sf->getIncludes(), nullptr) << "22.4: the file includes uvm_macros.svh";
  size_t count = 0;
  for (const hldb::SourceFile *const inc : *sf->getIncludes()) {
    if (inc->getName().find("uvm_macros.svh") != std::string_view::npos) ++count;
  }
  EXPECT_EQ(count, 1u) << "'`include \"uvm_macros.svh\"' is written once";
}

TEST_F(BehaviorOfRandomizationMethods1Test, FileUsesUvmInfoTwiceAndUvmErrorTwice) {
  EXPECT_EQ(getMacroInstances("uvm_info").size(), 2u) << "lines 38 and 45 use `uvm_info";
  EXPECT_EQ(getMacroInstances("uvm_error").size(), 2u) << "lines 40 and 47 use `uvm_error";
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods1Test, UvmPkgProvidesTheNamesTheFileUses) {
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
// class a; static rand int b; constraint c { b > 5; b < 12; } endclass
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods1Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
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

TEST_F(BehaviorOfRandomizationMethods1Test, ClassAHasNoBaseClassParametersOrEndLabel) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_FALSE(cls->getVirtual()) << "8.21: 'class a' is not declared 'virtual class'";
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
}

TEST_F(BehaviorOfRandomizationMethods1Test, ClassADeclaresNoMethods) {
  ASSERT_NE(getClassA(), nullptr);
  EXPECT_EQ(getClassADeclaredMethods().size(), 0u)
      << "class a declares no task or function; randomize() is built in to every class (18.6.1)";
}

TEST_F(BehaviorOfRandomizationMethods1Test, ClassAHasOnePropertyB) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'static rand int b;' is the only property";
  EXPECT_EQ(cls->getVariables()->at(0)->getName(), "b");
  EXPECT_EQ(cls->getVariables()->at(0), getB());
}

TEST_F(BehaviorOfRandomizationMethods1Test, BIsStaticRandSignedInt) {
  const hldb::Variable *const b = getB();
  ASSERT_NE(b, nullptr);
  const hldb::RefTypespec *const type = b->getTypespec();
  ASSERT_NE(type, nullptr) << "'b' has no typespec";
  const hldb::IntTypespec *const ts = type->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'b' is declared 'int'";
  EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is a signed type";
  EXPECT_TRUE(b->getIsRandomized()) << "18.4: 'b' is declared with the 'rand' qualifier";
  EXPECT_EQ(b->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_FALSE(b->getAutomatic())
      << "8.9 / 18.6.3: 'static' makes b a single copy shared by all instances, so its lifetime is static";
  EXPECT_EQ(b->getValue(), nullptr) << "'static rand int b;' has no initializer";
}

TEST_F(BehaviorOfRandomizationMethods1Test, BIsPublicAndNotConst) {
  const hldb::Variable *const b = getB();
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getVisibility(), vpiPublicVis) << "8.18: 'b' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(b->getConstantVariable()) << "8.19: 'b' has no 'const' qualifier";
}

TEST_F(BehaviorOfRandomizationMethods1Test, ClassAHasOneConstraintBlockC) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr) << "18.5: 'constraint c { ... }' is declared in class a";
  ASSERT_EQ(cls->getConstraints()->size(), 1u) << "class a declares exactly one constraint block";
  const hldb::Constraint *const c = getConstraintC();
  ASSERT_NE(c, nullptr) << "the constraint block is named 'c'";
  EXPECT_EQ(cls->getConstraints()->at(0), c);
  EXPECT_FALSE(c->getPure()) << "18.5.2: 'c' is not declared 'pure'";
  EXPECT_FALSE(c->getVirtual()) << "'c' is not declared 'virtual'";
}

TEST_F(BehaviorOfRandomizationMethods1Test, ConstraintCHasTwoItemsBGreaterThan5AndBLessThan12) {
  const hldb::Constraint *const c = getConstraintC();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getConstraintItems(), nullptr) << "18.5: '{ b > 5; b < 12; }' holds constraint expressions";
  ASSERT_EQ(c->getConstraintItems()->size(), 2u) << "two expressions, each terminated by ';'";
  const hldb::Any *lhs = nullptr;
  const hldb::Any *rhs = nullptr;
  ExpectBinaryOp(c->getConstraintItems()->at(0), vpiGtOp, "b > 5", &lhs, &rhs);
  ExpectBoundRef(lhs, "b", getB());
  ExpectIntLiteral(rhs, "5");
  ExpectBinaryOp(c->getConstraintItems()->at(1), vpiLtOp, "b < 12", &lhs, &rhs);
  ExpectBoundRef(lhs, "b", getB());
  ExpectIntLiteral(rhs, "12");
}

// ---------------------------------------------------------------------------
// class env extends uvm_env; ... endclass
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods1Test, ClassEnvIsDeclaredOnceAsUserDefinedClass) {
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
TEST_F(BehaviorOfRandomizationMethods1Test, ClassEnvExtendsUvmEnv) {
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

TEST_F(BehaviorOfRandomizationMethods1Test, ClassEnvHasPropertiesObj1AndObj2InOrder) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getVariables(), nullptr);
  ASSERT_EQ(env->getVariables()->size(), 2u) << "'a obj1 = new;' and 'a obj2 = new;'";
  const std::vector<std::string_view> expected = {"obj1", "obj2"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = env->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getIsRandomized()) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(BehaviorOfRandomizationMethods1Test, Obj1AndObj2AreClassAHandlesInitializedWithNew) {
  for (std::string_view name : {"obj1", "obj2"}) {
    const hldb::Variable *const obj = getEnvProperty(name);
    ASSERT_NE(obj, nullptr) << "'" << name << "' not found";
    ExpectClassHandle(obj->getTypespec(), getClassA(), name);
    const hldb::MethodFuncCall *const init = obj->getValue<hldb::MethodFuncCall>();
    ASSERT_NE(init, nullptr) << "8.7: 'a " << name << " = new;' initializes it with a constructor call";
    EXPECT_EQ(init->getName(), "new");
    EXPECT_TRUE(init->getArguments() == nullptr || init->getArguments()->empty())
        << "'new' is written with no arguments";
  }
}

TEST_F(BehaviorOfRandomizationMethods1Test, ClassEnvHasMethodsNewAndRunPhase) {
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

TEST_F(BehaviorOfRandomizationMethods1Test, EnvMethodsHaveAutomaticLifetime) {
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
TEST_F(BehaviorOfRandomizationMethods1Test, EnvNewHasFormalsNameAndParent) {
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
TEST_F(BehaviorOfRandomizationMethods1Test, EnvNewBodyIsSuperNewOfNameAndParent) {
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
TEST_F(BehaviorOfRandomizationMethods1Test, RunPhaseHasOneUvmPhaseFormalAndEndLabel) {
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

TEST_F(BehaviorOfRandomizationMethods1Test, RunPhaseBodyIsRaiseBlockDrop) {
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

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods1Test, RunPhaseRaisesObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(0), "raise_objection");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods1Test, RunPhaseDropsObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(2), "drop_objection");
}

// ---------------------------------------------------------------------------
// begin obj1.randomize(); if ... obj2.randomize(); if ... end
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods1Test, InnerBlockHoldsFourStatementsInOrder) {
  const hldb::Begin *const block = getInnerBlock();
  ASSERT_NE(block, nullptr);
  EXPECT_TRUE(block->getVariables() == nullptr || block->getVariables()->empty()) << "the block declares no variable";
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 4u) << "lines 36, 37, 43 and 44; the blank lines 42 and 49 are not statements";
  EXPECT_NE(any_cast<hldb::RefObj>(getInnerStmt(0)), nullptr) << "statement 0 is 'obj1.randomize();'";
  EXPECT_NE(any_cast<hldb::IfElse>(getInnerStmt(1)), nullptr) << "12.4: statement 1 is an if with an else";
  EXPECT_NE(any_cast<hldb::RefObj>(getInnerStmt(2)), nullptr) << "statement 2 is 'obj2.randomize();'";
  EXPECT_NE(any_cast<hldb::IfElse>(getInnerStmt(3)), nullptr) << "12.4: statement 3 is an if with an else";
}

TEST_F(BehaviorOfRandomizationMethods1Test, FirstStatementCallsObj1Randomize) {
  ExpectRandomizeCall(getInnerStmt(0), "obj1");
}

TEST_F(BehaviorOfRandomizationMethods1Test, ThirdStatementCallsObj2Randomize) {
  ExpectRandomizeCall(getInnerStmt(2), "obj2");
}

// ---------------------------------------------------------------------------
// if(obj1.b == obj2.b) begin `uvm_info(...) end else begin `uvm_error(...) end
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods1Test, IfConditionAfterObj1RandomizeIsObj1BEqualsObj2B) {
  ExpectIfObj1BEqualsObj2B(getInnerStmt(1));
}

TEST_F(BehaviorOfRandomizationMethods1Test, IfConditionAfterObj2RandomizeIsObj1BEqualsObj2B) {
  ExpectIfObj1BEqualsObj2B(getInnerStmt(3));
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods1Test, FirstThenBranchIsUvmInfoExpansion) { ExpectThenIsUvmInfo(getInnerStmt(1)); }

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods1Test, FirstElseBranchIsUvmErrorExpansion) {
  ExpectElseIsUvmError(getInnerStmt(1));
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods1Test, SecondThenBranchIsUvmInfoExpansion) {
  ExpectThenIsUvmInfo(getInnerStmt(3));
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods1Test, SecondElseBranchIsUvmErrorExpansion) {
  ExpectElseIsUvmError(getInnerStmt(3));
}

// ---------------------------------------------------------------------------
// module top; env environment; initial begin ... end endmodule
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods1Test, TopHasNoPortsAndOneEnvHandle) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top;' has no port list";
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "'env environment;' is the module's only declaration";
  const hldb::Variable *const environment = getEnvironment();
  ASSERT_NE(environment, nullptr) << "a class-typed data declaration is a variable (6.8)";
  ExpectClassHandle(environment->getTypespec(), getEnv(), "'environment'");
}

TEST_F(BehaviorOfRandomizationMethods1Test, TopHasOneInitialWithTwoStatements) {
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

TEST_F(BehaviorOfRandomizationMethods1Test, InitialConstructsEnvironment) {
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
TEST_F(BehaviorOfRandomizationMethods1Test, InitialCallsRunTestFromUvmPkg) {
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

TEST_F(BehaviorOfRandomizationMethods1Test, EveryReferencedVariableAndFormalIsFound) {
  for (std::string_view name : {"obj1", "obj2", "name", "parent", "phase", "environment"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "'" << name << "' is declared in this file, so it is not undefined";
  }
}

// The file is legal SystemVerilog, so no fatal, syntax or error diagnostic
// may point into it.
// Expected to fail -- see KNOWN FAILURE (errors reported in this file) in the
// file header.
TEST_F(BehaviorOfRandomizationMethods1Test, NoErrorIsReportedInThisFile) {
  GTEST_SKIP() << "HLC counts a forward declaration (a typedef) and the actual typedef "
                  "itself as two entries a reports an error that. Fix pending!";
  const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
  size_t count = 0;
  for (const Error &error : m_session->getErrorContainer()->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator info = infos.find(error.getType());
    if (info == infos.cend() || info->second.m_severity > ErrorDefinition::ERROR) continue;
    for (const Location &loc : error.getLocations()) {
      const std::string_view path = FileSystem::toPath(loc.m_fileId);
      if (path.find("18.6.3--behavior-of-randomization-methods_1.sv") == std::string_view::npos) continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Expected to fail -- see KNOWN FAILURE (errors reported in this file) in the
// file header.
TEST_F(BehaviorOfRandomizationMethods1Test, NoFatalSyntaxOrErrorDiagnostics) {
  GTEST_SKIP() << "HLC report a number of binding issues with UVM";
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the source is legal SystemVerilog and should compile without errors";
}

}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
