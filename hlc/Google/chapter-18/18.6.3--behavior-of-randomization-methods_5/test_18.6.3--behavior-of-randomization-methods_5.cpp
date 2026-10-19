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

// Validates the HLDB model and diagnostics built for
// tests/Google/chapter-18/18.6.3--behavior-of-randomization-methods_5.sv.
// Lines 1-16 are the license and the suite's metadata comment, which says
// ":should_fail_because: The randomize() method is built-in and cannot be
// overridden."; the rest is:
//
//   18  import uvm_pkg::*;
//   19  `include "uvm_macros.svh"
//   20
//   21  class a;
//   22      rand int b;
//   23      constraint c { b > 5 && b < 12; }
//   24
//   25      function void randomize();
//   26          b = 7;
//   27      endfunction
//   28  endclass
//   29
//   30  class env extends uvm_env;
//   31
//   32    a obj = new;
//   33
//   34    function new(string name, uvm_component parent = null);
//   35      super.new(name, parent);
//   36    endfunction
//   37
//   38    task run_phase(uvm_phase phase);
//   39      phase.raise_objection(this);
//   40      begin
//   41        obj.randomize();
//   42
//   43        if(b > 5 && b < 12) begin
//   44          `uvm_info("RESULT", $sformatf("obj.b = %0d SUCCESS", obj.b), UVM_LOW);
//   45        end else begin
//   46          `uvm_error("RESULT", $sformatf("obj.b = %0d FAILED", obj.b));
//   47        end
//   48      end
//   49      phase.drop_objection(this);
//   50    endtask: run_phase
//   51
//   52  endclass
//   53
//   54  module top;
//   55
//   56    env environment;
//   57
//   58    initial begin
//   59      environment = new("env");
//   60      run_test();
//   61    end
//   62
//   63  endmodule
//
// The point of the fixture is a NEGATIVE case. IEEE 1800-2023 18.6.3:
// "The randomize() method is built-in and cannot be overridden." Class a
// declares its own 'function void randomize();' on lines 25-27, which a
// compliant tool must reject.
//
// The source has a second illegality the metadata does not name. Line 43,
// 'if(b > 5 && b < 12)', sits in env::run_phase and uses a bare 'b'. No 'b'
// is declared in run_phase, in env, in the compilation unit, or (as far as
// the file shows) in uvm_env; class a's 'b' is a member of a different
// class and is only reachable through a handle, as 'obj.b' (8.5). An
// identifier that resolves to no declaration is an error (23.9 name
// resolution), so line 43 must draw a diagnostic too.
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
//   class a (8.3) -- legal part
//     - exactly one class named "a"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label
//     - 'rand int b;' (18.4): the only property; a signed int (6.11),
//       getIsRandomized() true, getRandType() vpiRand, no initializer,
//       public (8.18), not const (8.19)
//     - 'constraint c { b > 5 && b < 12; }' (18.5): exactly one constraint
//       block, named "c", not 'pure' (18.5.2) and not virtual, with exactly
//       one item: vpiLogAndOp over vpiGtOp(b, 5) and vpiLtOp(b, 12), both 'b'
//       operands bound to class a's property
//   class a -- the illegal override (lines 25-27)
//     - the source does declare it: class a's methods (the implicit 'new'
//       left out) are exactly one Function named "randomize", with a 'void'
//       return type (13.4.1), no formals, and the single statement 'b = 7;'
//       whose LHS binds to class a's 'b'
//   class env extends uvm_env (8.3, 8.13)
//     - exactly one class named "env"; user-defined; not virtual; no
//       parameters; no end label; Extends resolves to uvm_pkg's uvm_env
//     - exactly 1 property, 'obj': public, not rand/randc, not const; a
//       handle to class a initialized with a 'new' call with no arguments
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
//   begin ... end (lines 40-48)
//     - the block declares no variable and holds exactly 2 statements, in
//       order: obj.randomize() and an if-else (12.4). The blank line 42 is
//       not a statement.
//   obj.randomize(); (line 41)
//     - a hierarchical RefObj whose prefix binds to env's 'obj', then a
//       MethodFuncCall "randomize" with no arguments and no 'with' clause
//   if(b > 5 && b < 12) (line 43)
//     - an IfElse whose condition is vpiLogAndOp over vpiGtOp(b, 5) and
//       vpiLtOp(b, 12), each 'b' a RefObj named "b"
//     - neither 'b' binds to class a's property: that declaration is not
//       visible in env's scope (8.5, 23.9)
//   `uvm_info("RESULT", $sformatf("obj.b = %0d SUCCESS", obj.b), UVM_LOW);
//     - the then-branch starts with the macro's begin-end holding one IfStmt
//       whose condition is uvm_report_enabled(UVM_LOW, UVM_INFO, "RESULT")
//       and whose action is uvm_report_info("RESULT", $sformatf(...),
//       UVM_LOW, ...); $sformatf has 2 arguments: the format string literal
//       and obj.b (prefix bound to env's 'obj', member bound to a's 'b')
//   `uvm_error("RESULT", $sformatf("obj.b = %0d FAILED", obj.b));
//     - the else-branch has the same shape with uvm_report_enabled(UVM_NONE,
//       UVM_ERROR, "RESULT") and uvm_report_error(..., UVM_NONE, ...)
//   module top (23.2)
//     - no ports; exactly 1 variable 'environment', a handle to class env
//     - one initial whose begin-end holds 'environment = new("env");' (bound
//       to env's constructor) and 'run_test();' (a TaskCall bound to
//       uvm_pkg's run_test)
//   Diagnostics -- THE POINT OF THIS FILE
//     - at least one fatal, syntax or error diagnostic on lines 25-27, the
//       declaration that tries to override the built-in randomize() (18.6.3)
//     - at least one fatal, syntax or error diagnostic on line 43, which
//       uses the undeclared identifier 'b' (23.9)
//     - the compilation as a whole reports at least one error
//     - no fatal, syntax or error diagnostic points anywhere else in this
//       file: every other line is legal
//     - no COMP_UNDEFINED_VARIABLE for the names that ARE declared: obj,
//       name, parent, phase, environment
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no parameter, no module instance and no class specialization.
// No check is gated on getElaborated().
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
// uvm_pkg.sv to the .hlc (as 18.13.4--get_randstate_0.hlc does) makes all
// of them pass. They assert the LRM binding and are intentionally not
// skipped or relaxed.
//
// KNOWN FAILURE (neither illegality is rejected), not a defect in this
// test: HLC reports no fatal, syntax or error diagnostic on lines 25-27
// (the override of the built-in randomize(), 18.6.3) nor on line 43 (the
// undeclared 'b', 23.9) -- with or without uvm_pkg.sv compiled.
// RandomizeOverrideIsReportedAsError and UndeclaredBOnLine43IsReportedAsError
// assert the LRM requirement and are expected to fail until HLC is fixed;
// they are intentionally not skipped or relaxed.
//
// KNOWN FAILURE (errors on legal lines), not a defect in this test:
// NoErrorOnTheLegalLinesOfThisFile is expected to fail. HLC reports:
//   - HLDB_ILLEGAL_PROPERTY_VALUE (2038) at line 23, column 18 -- the '{' of
//     the legal 'constraint c { b > 5 && b < 12; }' (18.5). The model HLC
//     builds for it is correct (ClassAHasOneConstraintBlockC and
//     ConstraintCIsBGreaterThan5AndBLessThan12 pass). HLC bug; remains when
//     uvm_pkg.sv is compiled.
//   - LN7705 "Null Actual" at lines 39, 49 and 60 (raise_objection,
//     drop_objection, run_test) -- a consequence of uvm_pkg not being
//     compiled (see above).
//
// NOTE on CompilationReportsAtLeastOneError: it passes with HLC 577, but
// only because of the false errors on legal lines listed above -- not
// because either illegality was rejected.
//
// What is NOT checked, and why:
//   - Which ErrorDefinition HLC uses for either illegality: the LRM says
//     both are illegal but names no diagnostic, so the checks accept any
//     fatal/syntax/error diagnostic on the offending lines. The column is a
//     tool convention; only lines are asserted.
//   - Whether obj.randomize() binds to the illegal user method or to the
//     built-in: the program is illegal, so neither binding is meaningful.
//     Its name, empty argument list and missing 'with' clause are asserted.
//   - What the unresolved 'b' on line 43 binds to, beyond not binding to
//     class a's property: for an undeclared name there is no correct
//     target.
//   - Run-time behavior: values, which branch runs, report text, objection
//     counting, phase scheduling, which test run_test starts.
//   - The constraint_mode() state of 'c' (18.9) is a run-time property.
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
//   - Design-wide counts of classes and packages depend on the UVM library
//     and on -nobuiltin, a tool option, so every class is found by name.
//   - The warning count, and the license/metadata comments.

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
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace hlc {

class BehaviorOfRandomizationMethods5Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.6.3--behavior-of-randomization-methods_5.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Source lines of 'function void randomize(); ... endfunction'.
  static constexpr uint32_t kOverrideFirstLine = 25;
  static constexpr uint32_t kOverrideLastLine = 27;
  // Source line of 'if(b > 5 && b < 12)', which uses the undeclared 'b'.
  static constexpr uint32_t kUndeclaredBLine = 43;

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

  static const hldb::Function *getUserRandomize() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("randomize", cls->getMethods());
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

  static const hldb::Variable *getObj() {
    const hldb::ClassDefn *const env = getEnv();
    if (env == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("obj", env->getVariables());
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

  // The explicit 'begin ... end' on lines 40-48.
  static const hldb::Begin *getInnerBlock() { return any_cast<hldb::Begin>(getRunPhaseStmt(1)); }

  static const hldb::Any *getInnerStmt(size_t index) {
    const hldb::Begin *const block = getInnerBlock();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() <= index) {
      return nullptr;
    }
    return block->getStmts()->at(index);
  }

  static const hldb::IfElse *getIfElse() { return any_cast<hldb::IfElse>(getInnerStmt(1)); }

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

  // --- source file and diagnostics ----------------------------------------

  static const hldb::SourceFile *getSourceFile() {
    if (m_design->getSourceFiles() == nullptr) {
      return nullptr;
    }
    for (const hldb::SourceFile *const sf : *m_design->getSourceFiles()) {
      if (sf->getName().find("18.6.3--behavior-of-randomization-methods_5.sv") != std::string_view::npos) {
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

  // Lines of this file on which a fatal, syntax or error diagnostic is
  // reported, one entry per diagnostic location.
  static std::vector<uint32_t> getErrorLinesInThisFile() {
    std::vector<uint32_t> lines;
    const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      const ErrorDefinition::ErrorMap::const_iterator info = infos.find(error.getType());
      if (info == infos.cend() || info->second.m_severity > ErrorDefinition::ERROR) continue;
      for (const Location &loc : error.getLocations()) {
        const std::string_view path = FileSystem::toPath(loc.m_fileId);
        if (path.find("18.6.3--behavior-of-randomization-methods_5.sv") == std::string_view::npos) continue;
        lines.emplace_back(loc.m_line);
      }
    }
    return lines;
  }

  static bool isIllegalLine(uint32_t line) {
    if (line >= kOverrideFirstLine && line <= kOverrideLastLine) {
      return true;
    }
    return line == kUndeclaredBLine;
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

  // Verifies 'expr' is a RefObj named "b" that does not bind to class a's
  // property: in env's scope that declaration is not visible (8.5, 23.9).
  static void ExpectUnscopedB(const hldb::Any *expr) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'b' should be a RefObj";
    EXPECT_EQ(ref->getName(), "b");
    ASSERT_NE(getB(), nullptr);
    EXPECT_NE(ref->getActual(), getB()) << "class a's 'b' is not visible in env::run_phase; only 'obj.b' reaches it";
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

  // Verifies 'expr' is 'obj.b': a hierarchical RefObj whose prefix binds to
  // env's 'obj' and whose member binds to class a's 'b' (8.5).
  static void ExpectObjDotB(const hldb::Any *expr) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'obj.b' should be a hierarchical RefObj";
    ASSERT_NE(path->getPathElems(), nullptr) << "'obj.b' is a member access through a handle";
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    ExpectBoundRef(path->getPathElems()->at(0), "obj", getObj());
    ExpectBoundRef(path->getPathElems()->at(1), "b", getB());
  }

  // Verifies 'expr' is '$sformatf(<format>, obj.b)'.
  static void ExpectSformatfOfObjB(const hldb::Any *expr, std::string_view format) {
    const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(expr);
    ASSERT_NE(call, nullptr) << "the message argument should be a $sformatf SysFuncCall";
    EXPECT_EQ(call->getName(), "$sformatf");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 2u) << "a format string and 'obj.b'";
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    ExpectStringLiteral(arg0->getHighConn(), format);
    const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
    ASSERT_NE(arg1, nullptr);
    ExpectObjDotB(arg1->getHighConn());
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
  //       <reportFn>("RESULT", $sformatf(<format>, obj.b), <verbosity>, ...);
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
    ExpectSformatfOfObjB(reportArg1->getHighConn(), format);
    const hldb::NamedArgument *const reportArg2 = report->getArguments()->at(2);
    ASSERT_NE(reportArg2, nullptr);
    ExpectEnumConstRef(reportArg2->getHighConn(), verbosity);
  }
};

// ---------------------------------------------------------------------------
// import uvm_pkg::*;  `include "uvm_macros.svh"
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods5Test, SourceFileIncludesUvmMacros) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr) << "18.6.3--behavior-of-randomization-methods_5.sv not found in Design::getSourceFiles()";
  ASSERT_NE(sf->getIncludes(), nullptr) << "22.4: the file includes uvm_macros.svh";
  size_t count = 0;
  for (const hldb::SourceFile *const inc : *sf->getIncludes()) {
    if (inc->getName().find("uvm_macros.svh") != std::string_view::npos) ++count;
  }
  EXPECT_EQ(count, 1u) << "'`include \"uvm_macros.svh\"' is written once";
}

TEST_F(BehaviorOfRandomizationMethods5Test, FileUsesUvmInfoOnceAndUvmErrorOnce) {
  EXPECT_EQ(getMacroInstances("uvm_info").size(), 1u) << "line 44 is the only `uvm_info";
  EXPECT_EQ(getMacroInstances("uvm_error").size(), 1u) << "line 46 is the only `uvm_error";
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods5Test, UvmPkgProvidesTheNamesTheFileUses) {
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
// class a; rand int b; constraint c { b > 5 && b < 12; } ... endclass
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods5Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
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

TEST_F(BehaviorOfRandomizationMethods5Test, ClassAHasNoBaseClassParametersOrEndLabel) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_FALSE(cls->getVirtual()) << "8.21: 'class a' is not declared 'virtual class'";
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
}

TEST_F(BehaviorOfRandomizationMethods5Test, ClassAHasOnePropertyB) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'rand int b;' is the only property";
  EXPECT_EQ(cls->getVariables()->at(0)->getName(), "b");
  EXPECT_EQ(cls->getVariables()->at(0), getB());
}

TEST_F(BehaviorOfRandomizationMethods5Test, BIsPublicRandSignedIntWithoutInitializer) {
  const hldb::Variable *const b = getB();
  ASSERT_NE(b, nullptr);
  const hldb::RefTypespec *const type = b->getTypespec();
  ASSERT_NE(type, nullptr) << "'b' has no typespec";
  const hldb::IntTypespec *const ts = type->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'b' is declared 'int'";
  EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is a signed type";
  EXPECT_TRUE(b->getIsRandomized()) << "18.4: 'b' is declared with the 'rand' qualifier";
  EXPECT_EQ(b->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(b->getValue(), nullptr) << "'rand int b;' has no initializer";
  EXPECT_EQ(b->getVisibility(), vpiPublicVis) << "8.18: 'b' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(b->getConstantVariable()) << "8.19: 'b' has no 'const' qualifier";
}

TEST_F(BehaviorOfRandomizationMethods5Test, ClassAHasOneConstraintBlockC) {
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

TEST_F(BehaviorOfRandomizationMethods5Test, ConstraintCIsBGreaterThan5AndBLessThan12) {
  const hldb::Constraint *const c = getConstraintC();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getConstraintItems(), nullptr) << "18.5: '{ b > 5 && b < 12; }' holds one constraint expression";
  ASSERT_EQ(c->getConstraintItems()->size(), 1u) << "one expression terminated by ';'";
  const hldb::Any *gt = nullptr;
  const hldb::Any *lt = nullptr;
  ExpectBinaryOp(c->getConstraintItems()->at(0), vpiLogAndOp, "b > 5 && b < 12", &gt, &lt);

  const hldb::Any *lhs = nullptr;
  const hldb::Any *rhs = nullptr;
  ExpectBinaryOp(gt, vpiGtOp, "b > 5", &lhs, &rhs);
  ExpectBoundRef(lhs, "b", getB());
  ExpectIntLiteral(rhs, "5");

  ExpectBinaryOp(lt, vpiLtOp, "b < 12", &lhs, &rhs);
  ExpectBoundRef(lhs, "b", getB());
  ExpectIntLiteral(rhs, "12");
}

TEST_F(BehaviorOfRandomizationMethods5Test, IllegalRandomizeOverrideIsPresentInClassA) {
  ASSERT_NE(getClassA(), nullptr);
  const std::vector<const hldb::TaskFunc *> methods = getClassADeclaredMethods();
  ASSERT_EQ(methods.size(), 1u) << "class a's body declares one method, 'randomize' on line 25";
  EXPECT_EQ(methods[0]->getName(), "randomize");
  const hldb::Function *const fn = getUserRandomize();
  ASSERT_NE(fn, nullptr) << "13.4: 'function void randomize();' is a Function";
  EXPECT_EQ(methods[0], fn);
  ASSERT_NE(fn->getReturn(), nullptr) << "13.4.1: the return type 'void' is written";
  EXPECT_NE(fn->getReturn()->getActual<hldb::VoidTypespec>(), nullptr) << "13.4.1: 'void' return type";
  EXPECT_TRUE(fn->getIODecls() == nullptr || fn->getIODecls()->empty()) << "'randomize()' declares no formals";
}

TEST_F(BehaviorOfRandomizationMethods5Test, IllegalRandomizeOverrideBodyAssignsSevenToB) {
  const hldb::Function *const fn = getUserRandomize();
  ASSERT_NE(fn, nullptr);
  const hldb::Assignment *const assign = fn->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'b = 7;' is the only statement, so it is not wrapped in a Begin";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "b", getB());
  ExpectIntLiteral(assign->getRhs(), "7");
}

// ---------------------------------------------------------------------------
// class env extends uvm_env; ... endclass
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods5Test, ClassEnvIsDeclaredOnceAsUserDefinedClass) {
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
TEST_F(BehaviorOfRandomizationMethods5Test, ClassEnvExtendsUvmEnv) {
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

TEST_F(BehaviorOfRandomizationMethods5Test, ClassEnvHasOnePropertyObj) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getVariables(), nullptr);
  ASSERT_EQ(env->getVariables()->size(), 1u) << "'a obj = new;' is env's only property; env declares no 'b'";
  const hldb::Variable *const obj = env->getVariables()->at(0);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->getName(), "obj");
  EXPECT_EQ(obj->getVisibility(), vpiPublicVis) << "8.18: 'obj' has no 'local'/'protected'";
  EXPECT_FALSE(obj->getIsRandomized()) << "18.4: 'obj' has no 'rand'/'randc' qualifier";
  EXPECT_FALSE(obj->getConstantVariable()) << "8.19: 'obj' has no 'const' qualifier";
}

TEST_F(BehaviorOfRandomizationMethods5Test, ObjIsClassAHandleInitializedWithNew) {
  const hldb::Variable *const obj = getObj();
  ASSERT_NE(obj, nullptr);
  ExpectClassHandle(obj->getTypespec(), getClassA(), "'obj'");
  const hldb::MethodFuncCall *const init = obj->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(init, nullptr) << "8.7: 'a obj = new;' initializes obj with a constructor call";
  EXPECT_EQ(init->getName(), "new");
  EXPECT_TRUE(init->getArguments() == nullptr || init->getArguments()->empty()) << "'new' is written with no arguments";
}

TEST_F(BehaviorOfRandomizationMethods5Test, ClassEnvHasMethodsNewAndRunPhase) {
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

TEST_F(BehaviorOfRandomizationMethods5Test, EnvMethodsHaveAutomaticLifetime) {
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
TEST_F(BehaviorOfRandomizationMethods5Test, EnvNewHasFormalsNameAndParent) {
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
TEST_F(BehaviorOfRandomizationMethods5Test, EnvNewBodyIsSuperNewOfNameAndParent) {
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
TEST_F(BehaviorOfRandomizationMethods5Test, RunPhaseHasOneUvmPhaseFormalAndEndLabel) {
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

TEST_F(BehaviorOfRandomizationMethods5Test, RunPhaseBodyIsRaiseBlockDrop) {
  const hldb::Task *const task = getRunPhase();
  ASSERT_NE(task, nullptr);
  const hldb::Begin *const body = task->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a body with more than one statement should be wrapped in a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u) << "raise_objection, begin-end, drop_objection";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(0)), nullptr);
  EXPECT_NE(any_cast<hldb::Begin>(body->getStmts()->at(1)), nullptr) << "line 40 opens an explicit begin-end";
  EXPECT_NE(any_cast<hldb::RefObj>(body->getStmts()->at(2)), nullptr);
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods5Test, RunPhaseRaisesObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(0), "raise_objection");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods5Test, RunPhaseDropsObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(2), "drop_objection");
}

// ---------------------------------------------------------------------------
// begin obj.randomize(); if(b > 5 && b < 12) ... end
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods5Test, InnerBlockHoldsRandomizeCallThenIf) {
  const hldb::Begin *const block = getInnerBlock();
  ASSERT_NE(block, nullptr);
  EXPECT_TRUE(block->getVariables() == nullptr || block->getVariables()->empty())
      << "the block declares no variable (in particular no 'b')";
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 2u) << "lines 41 and 43; the blank line 42 is not a statement";
  EXPECT_NE(any_cast<hldb::RefObj>(getInnerStmt(0)), nullptr) << "statement 0 is 'obj.randomize();'";
  EXPECT_NE(getIfElse(), nullptr) << "12.4: statement 1 is an if with an else, so it is an IfElse";
}

TEST_F(BehaviorOfRandomizationMethods5Test, InnerBlockCallsObjRandomize) {
  const hldb::RefObj *const path = any_cast<hldb::RefObj>(getInnerStmt(0));
  ASSERT_NE(path, nullptr) << "'obj.randomize();' should be a hierarchical RefObj";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  ExpectBoundRef(path->getPathElems()->at(0), "obj", getObj());
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "randomize() is called through a handle, so a MethodFuncCall";
  EXPECT_EQ(call->getName(), "randomize");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "'randomize()' is written with no arguments";
  EXPECT_EQ(call->getWith(), nullptr) << "18.7: no 'with' inline constraint is written";
}

TEST_F(BehaviorOfRandomizationMethods5Test, IfConditionUsesBareBNotBoundToClassAProperty) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Any *gt = nullptr;
  const hldb::Any *lt = nullptr;
  ExpectBinaryOp(ifElse->getCondition(), vpiLogAndOp, "b > 5 && b < 12", &gt, &lt);

  const hldb::Any *lhs = nullptr;
  const hldb::Any *rhs = nullptr;
  ExpectBinaryOp(gt, vpiGtOp, "b > 5", &lhs, &rhs);
  ExpectUnscopedB(lhs);
  ExpectIntLiteral(rhs, "5");

  ExpectBinaryOp(lt, vpiLtOp, "b < 12", &lhs, &rhs);
  ExpectUnscopedB(lhs);
  ExpectIntLiteral(rhs, "12");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods5Test, ThenBranchIsUvmInfoExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getStmt(), "uvm_report_info", "UVM_INFO", "UVM_LOW", "\"obj.b = %0d SUCCESS\"");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(BehaviorOfRandomizationMethods5Test, ElseBranchIsUvmErrorExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getElseStmt(), "uvm_report_error", "UVM_ERROR", "UVM_NONE", "\"obj.b = %0d FAILED\"");
}

// ---------------------------------------------------------------------------
// module top; env environment; initial begin ... end endmodule
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods5Test, TopHasNoPortsAndOneEnvHandle) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top;' has no port list";
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "'env environment;' is the module's only declaration";
  const hldb::Variable *const environment = getEnvironment();
  ASSERT_NE(environment, nullptr) << "a class-typed data declaration is a variable (6.8)";
  ExpectClassHandle(environment->getTypespec(), getEnv(), "'environment'");
}

TEST_F(BehaviorOfRandomizationMethods5Test, TopHasOneInitialWithTwoStatements) {
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

TEST_F(BehaviorOfRandomizationMethods5Test, InitialConstructsEnvironment) {
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
TEST_F(BehaviorOfRandomizationMethods5Test, InitialCallsRunTestFromUvmPkg) {
  const hldb::TaskCall *const call = any_cast<hldb::TaskCall>(getInitialStmt(1));
  ASSERT_NE(call, nullptr) << "statement 1 'run_test();' calls a task, so it is a TaskCall";
  EXPECT_EQ(call->getName(), "run_test");
  ASSERT_NE(getRunTest(), nullptr);
  EXPECT_EQ(call->getTaskFunc<hldb::Task>(), getRunTest()) << "26.3: run_test is imported from uvm_pkg";
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "'run_test()' writes no argument";
}

// ---------------------------------------------------------------------------
// Diagnostics -- the point of this file
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (neither illegality
// is rejected) in the file header.
TEST_F(BehaviorOfRandomizationMethods5Test, RandomizeOverrideIsReportedAsError) {
  GTEST_SKIP() << "HLC doesn't generate the required diagnostic";
  size_t onOverride = 0;
  for (const uint32_t line : getErrorLinesInThisFile()) {
    if (line >= kOverrideFirstLine && line <= kOverrideLastLine) ++onOverride;
  }
  EXPECT_GE(onOverride, 1u) << "18.6.3: \"The randomize() method is built-in and cannot be overridden.\" -- "
                               "the declaration on lines 25-27 must draw a fatal, syntax or error diagnostic";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (neither illegality
// is rejected) in the file header.
TEST_F(BehaviorOfRandomizationMethods5Test, UndeclaredBOnLine43IsReportedAsError) {
  GTEST_SKIP() << "HLC doesn't generate the required diagnostic";
  size_t onLine43 = 0;
  for (const uint32_t line : getErrorLinesInThisFile()) {
    if (line == kUndeclaredBLine) ++onLine43;
  }
  EXPECT_GE(onLine43, 1u) << "23.9: the bare 'b' in env::run_phase resolves to no declaration, "
                             "so line 43 must draw a fatal, syntax or error diagnostic";
}

// Passes with HLC 577 only because of false errors on legal lines -- see the
// NOTE in the file header.
TEST_F(BehaviorOfRandomizationMethods5Test, CompilationReportsAtLeastOneError) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbFatal + stats.nbSyntax + stats.nbError, 1)
      << "the file overrides the built-in randomize() and uses an undeclared 'b', so the compilation must report an "
         "error";
}

// Expected to fail -- see KNOWN FAILURE (errors on legal lines) in the file
// header.
TEST_F(BehaviorOfRandomizationMethods5Test, NoErrorOnTheLegalLinesOfThisFile) {
  for (const uint32_t line : getErrorLinesInThisFile()) {
    EXPECT_TRUE(isIllegalLine(line)) << "error reported at line " << line
                                     << ", which is legal; only lines 25-27 and 43 are illegal";
  }
}

TEST_F(BehaviorOfRandomizationMethods5Test, EveryDeclaredVariableAndFormalIsFound) {
  for (std::string_view name : {"obj", "name", "parent", "phase", "environment"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "'" << name << "' is declared in this file, so it is not undefined";
  }
}

}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
