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

// Validates the HLDB model and the diagnostics built for
// tests/Google/chapter-18/18.9--controlling-constraints-with-constraint_mode_2.sv.
// Lines 1-16 are the license and the suite's metadata comment, which says:
//
//   :should_fail_because: The constraint_mode() method is built-in and cannot be
//                         overridden.
//   :tags: uvm-random uvm
//   :type: simulation
//
// The rest is:
//
//   18  import uvm_pkg::*;
//   19  `include "uvm_macros.svh"
//   20
//   21  class a;
//   22      rand int x;
//   23      function int constraint_mode();
//   24          return 1;
//   25      endfunction
//   26  endclass
//   27
//   28  class env extends uvm_env;
//   29
//   30    a obj = new;
//   31    int ret;
//   32
//   33    function new(string name, uvm_component parent = null);
//   34      super.new(name, parent);
//   35    endfunction
//   36
//   37    task run_phase(uvm_phase phase);
//   38      phase.raise_objection(this);
//   39      begin
//   40        ret = obj.constraint_mode();
//   41
//   42        if(ret == 1) begin
//   43          `uvm_info("RESULT", $sformatf("ret = %0d SUCCESS", ret), UVM_LOW);
//   44        end else begin
//   45          `uvm_error("RESULT", $sformatf("ret = %0d FAILED", ret));
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
// IEEE 1800-2023 18.9: "The constraint_mode() method is built-in and cannot be
// overridden." Class a declares its own method named constraint_mode (lines
// 23-25), so the file is illegal. The rule is about a declaration, so a
// tool can and must reject it while compiling, even though the suite tags
// the file ':type: simulation'. HLC defines a dedicated diagnostic for
// exactly this case, COMP_CANNOT_REDEFINE_BUILTIN_METHOD, and the test
// expects it.
//
// Line 40 calls constraint_mode() on the object 'obj' itself, with no
// constraint name. 18.9 gives the built-in two forms:
//   task object[.constraint_identifier]::constraint_mode( bit on_off );
//   function int object.constraint_identifier::constraint_mode();
// The built-in function form needs a constraint name, and the object-level
// built-in is only a task, so 'ret = obj.constraint_mode();' can only call
// class a's own (illegal) function. The test therefore asserts the call as
// written -- 'obj' -> a MethodFuncCall "constraint_mode" with no arguments
// -- but not which declaration it binds to: the LRM gives no meaning to a
// call of an illegal override. Line 40 is otherwise legal; the only illegal
// lines are 23-25.
//
// The .hlc passes -nobuiltin, which stops HLC from loading its built-in
// package. That is a tool option; 18.9 makes constraint_mode() built into
// every class regardless, so it does not change what is expected.
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
//       constraint block (18.5)
//     - 'rand int x;' (18.4): the only property; a signed int (6.11),
//       getIsRandomized() true, getRandType() vpiRand, no initializer,
//       public (8.18), not const (8.19)
//   function int constraint_mode(); return 1; endfunction (13.4)
//     - the class body declares exactly one method: the Function
//       'constraint_mode'; a class method (getMethod() true), automatic (8.6),
//       public (8.18), not virtual, no end label
//     - empty formal list; return type 'int', a signed IntTypespec (6.11)
//     - the body is exactly one statement, 'return 1;': a ReturnStmt whose
//       value is the literal 1 (13.4.1)
//   class env extends uvm_env (8.3, 8.13)
//     - exactly one class named "env"; user-defined; not virtual; no
//       parameters; no end label; Extends resolves to uvm_pkg's uvm_env
//     - exactly 2 properties, in declaration order: obj, ret. Each is
//       public, not rand/randc, not const. 'a obj = new;' is a handle to
//       class a initialized with a 'new' call with no arguments (8.7);
//       'ret' is a signed int with no initializer
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
//     - the block declares no variable and holds exactly 2 statements, in
//       order: an Assignment and an if-else (12.4); the empty line 41 is
//       not a statement
//   ret = obj.constraint_mode(); (10.4.1)
//     - a blocking Assignment whose LHS binds to env's 'ret'
//     - the RHS is the path 'obj' -> 'constraint_mode': 'obj' binds to env's
//       'obj', and 'constraint_mode' is a function call (a MethodFuncCall)
//       with no arguments
//   if(ret == 1) (12.4)
//     - an IfElse whose condition is vpiEqOp(ret, 1); 'ret' binds to env's
//       'ret'
//   `uvm_info("RESULT", $sformatf("ret = %0d SUCCESS", ret), UVM_LOW);
//     - the then-branch starts with the macro's begin-end holding one IfStmt
//       whose condition is uvm_report_enabled(UVM_LOW, UVM_INFO, "RESULT")
//       and whose action is uvm_report_info("RESULT", $sformatf(...),
//       UVM_LOW, ...); $sformatf has 2 arguments: the format string literal
//       and ret
//   `uvm_error("RESULT", $sformatf("ret = %0d FAILED", ret));
//     - the else-branch has the same shape with uvm_report_enabled(UVM_NONE,
//       UVM_ERROR, "RESULT") and uvm_report_error(..., UVM_NONE, ...)
//   module top (23.2)
//     - no ports; exactly 1 variable 'environment', a handle to class env
//     - one initial whose begin-end holds 'environment = new("env");' (bound
//       to env's constructor) and 'run_test();' (a TaskCall bound to
//       uvm_pkg's run_test)
//   Diagnostics (18.9)
//     - no COMP_UNDEFINED_VARIABLE for any variable or formal the file
//       references
//     - COMP_CANNOT_REDEFINE_BUILTIN_METHOD is reported for 'constraint_mode', in
//       this file, on line 23 (the declaration), and HLC classifies it as an
//       error (not a warning or note)
//     - the compilation therefore reports at least one error
//     - every error that points into this file points into the constraint_mode
//       declaration (lines 23-25): the rest of the file is legal
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
// KNOWN FAILURE (illegal override of constraint_mode() accepted), not a defect in
// this test: RedefiningConstraintModeIsReportedOnItsDeclaration fails on HLC 577.
// HLC never reports COMP_CANNOT_REDEFINE_BUILTIN_METHOD for 'constraint_mode',
// although 18.9 forbids overriding it and HLC defines that diagnostic as an
// error. CompilationReportsAtLeastOneError passes only because of the
// unrelated errors listed below, not because the override was caught. It is
// expected to fail until HLC is fixed and is intentionally not skipped or
// relaxed.
//
// KNOWN FAILURE (errors reported on legal lines), not a defect in this
// test: EveryErrorInThisFileIsOnTheConstraintModeDeclaration fails. Instead
// of an error on the override, HLC reports LN7705 "Null Actual" at lines 38,
// 48 and 59 (raise_objection, drop_objection, run_test) -- a consequence of
// uvm_pkg not being compiled (see above). It is expected to pass once
// uvm_pkg.sv is compiled and the override is reported, and is intentionally
// not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The value constraint_mode() returns, which branch runs, the text the report
//     prints, objection counting, phase scheduling and which test run_test
//     starts all only exist while simulation runs. Permanently out of
//     scope.
//   - The column of the redefinition diagnostic: whether HLC points at
//     'function', at the return type or at the name is a tool convention,
//     so only the line is asserted.
//   - Whether HLC still records the illegal constraint_mode as a method of a
//     after diagnosing it is a tool convention. The structural checks
//     describe the source as written; if a tool drops the method after the
//     error, those checks fail and say so.
//   - Whether the function's return value is modeled as an implicit
//     Variable named 'constraint_mode' (13.4.1) is a model convention.
//   - Whether HLC stores the one-statement body of constraint_mode as the bare
//     statement or as a Begin holding it is a model convention; both are
//     accepted, and in both the ReturnStmt must be the only statement.
//   - Whether HLC binds the built-in constraint_mode() call through getTaskFunc()
//     is a tool convention.
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
//   - 'a obj = new;' invokes the implicit constructor (8.7); how HLC
//     represents it is a tool convention.
//   - The default value of 'parent' (no grounded accessor in this suite).
//   - Design-wide counts of classes, packages and functions depend on the
//     UVM and built-in libraries, so every declaration is found by name.
//   - Warnings, notes, source columns, and the license/metadata comments.

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
#include <hldb/return_stmt.h>
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

class ControllingConstraintsWithConstraintMode2Test : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "18.9--controlling-constraints-with-constraint_mode_2.hlc"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static constexpr std::string_view kSourceName = "18.9--controlling-constraints-with-constraint_mode_2.sv";

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

  static const hldb::Variable *getClassAX() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("x", cls->getVariables());
  }

  static const hldb::Function *getConstraintMode() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("constraint_mode", cls->getMethods());
  }

  // The statements of the constraint_mode body: the bare statement, or the
  // statements of a Begin wrapping it.
  static std::vector<const hldb::Any *> getConstraintModeBodyStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Function *const fn = getConstraintMode();
    if (fn == nullptr || fn->getStmt() == nullptr) {
      return stmts;
    }
    if (const hldb::Begin *const block = fn->getStmt<hldb::Begin>()) {
      if (block->getStmts() == nullptr) {
        return stmts;
      }
      for (const hldb::Any *const stmt : *block->getStmts()) {
        stmts.emplace_back(stmt);
      }
      return stmts;
    }
    stmts.emplace_back(fn->getStmt());
    return stmts;
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

  // --- source file ----------------------------------------------------------

  static const hldb::SourceFile *getSourceFile() {
    if (m_design->getSourceFiles() == nullptr) {
      return nullptr;
    }
    for (const hldb::SourceFile *const sf : *m_design->getSourceFiles()) {
      if (sf->getName().find(kSourceName) != std::string_view::npos) {
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
      if (mi->getName() == name) {
        found.emplace_back(mi);
      }
    }
    return found;
  }

  // --- diagnostics --------------------------------------------------------

  // True if 'type' has severity error or worse.
  static bool isErrorSeverity(ErrorDefinition::ErrorType type) {
    const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
    const ErrorDefinition::ErrorMap::const_iterator info = infos.find(type);
    if (info == infos.cend()) {
      return false;
    }
    return info->second.m_severity <= ErrorDefinition::ERROR;
  }

  // The source lines of this file that diagnostics of 'type' point to. When
  // 'errorsOnly' is set, 'type' is ignored and every diagnostic of severity
  // error or worse is collected instead.
  static std::vector<uint32_t> getDiagnosticLines(ErrorDefinition::ErrorType type, bool errorsOnly) {
    std::vector<uint32_t> lines;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      if (errorsOnly) {
        if (!isErrorSeverity(error.getType())) {
          continue;
        }
      } else if (error.getType() != type) {
        continue;
      }
      for (const Location &loc : error.getLocations()) {
        const std::string_view path = FileSystem::toPath(loc.m_fileId);
        if (path.find(kSourceName) == std::string_view::npos) {
          continue;
        }
        lines.emplace_back(loc.m_line);
      }
    }
    return lines;
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

  // Verifies 'expr' is '$sformatf(<format>, ret)'.
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
    ExpectBoundRef(arg1->getHighConn(), "ret", getEnvProperty("ret"));
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
    ExpectSformatfOfRet(reportArg1->getHighConn(), format);
    const hldb::NamedArgument *const reportArg2 = report->getArguments()->at(2);
    ASSERT_NE(reportArg2, nullptr);
    ExpectEnumConstRef(reportArg2->getHighConn(), verbosity);
  }
};

// ---------------------------------------------------------------------------
// import uvm_pkg::*;  `include "uvm_macros.svh"
// ---------------------------------------------------------------------------

TEST_F(ControllingConstraintsWithConstraintMode2Test, SourceFileIncludesUvmMacros) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr) << "18.9--controlling-constraints-with-constraint_mode_2.sv not found in getSourceFiles()";
  ASSERT_NE(sf->getIncludes(), nullptr) << "22.4: the file includes uvm_macros.svh";
  size_t count = 0;
  for (const hldb::SourceFile *const inc : *sf->getIncludes()) {
    if (inc->getName().find("uvm_macros.svh") != std::string_view::npos) {
      ++count;
    }
  }
  EXPECT_EQ(count, 1u) << "'`include \"uvm_macros.svh\"' is written once";
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, FileUsesUvmInfoOnceAndUvmErrorOnce) {
  EXPECT_EQ(getMacroInstances("uvm_info").size(), 1u) << "line 43 is the only `uvm_info";
  EXPECT_EQ(getMacroInstances("uvm_error").size(), 1u) << "line 45 is the only `uvm_error";
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(ControllingConstraintsWithConstraintMode2Test, UvmPkgProvidesTheNamesTheFileUses) {
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
// class a; rand int x; function int constraint_mode(); ... endfunction endclass
// ---------------------------------------------------------------------------

TEST_F(ControllingConstraintsWithConstraintMode2Test, ClassAIsPlainUserDefinedClass) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  size_t count = 0;
  for (const hldb::ClassDefn *const c : *m_design->getAllClasses()) {
    if (c->getName() == "a") {
      ++count;
    }
  }
  EXPECT_EQ(count, 1u) << "the source declares class 'a' exactly once";
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass) << "8.3: 'class a;' is a user-defined class";
  EXPECT_FALSE(cls->getVirtual()) << "8.21: 'a' is not declared 'virtual class'";
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
  EXPECT_TRUE(cls->getConstraints() == nullptr || cls->getConstraints()->empty())
      << "18.5: class a declares no constraint block";
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, ClassAHasOneRandIntPropertyX) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'rand int x;' is the only property";
  const hldb::Variable *const x = getClassAX();
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(cls->getVariables()->at(0), x);
  ExpectSignedInt(x->getTypespec(), "class a's 'x'");
  EXPECT_TRUE(x->getIsRandomized()) << "18.4: 'x' is declared with the 'rand' qualifier";
  EXPECT_EQ(x->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(x->getValue(), nullptr) << "'rand int x;' has no initializer";
  EXPECT_EQ(x->getVisibility(), vpiPublicVis) << "8.18: 'x' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(x->getConstantVariable()) << "8.19: 'x' has no 'const' qualifier";
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, ClassADeclaresOneFunctionConstraintMode) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getMethods(), nullptr) << "the class body declares 'function int constraint_mode()'";
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "'constraint_mode' is the only method written in class a";
  const hldb::Function *const fn = getConstraintMode();
  ASSERT_NE(fn, nullptr) << "13.4: 'function int constraint_mode()' is a Function";
  EXPECT_TRUE(fn->getMethod()) << "8.6: a function declared in a class is a method";
  EXPECT_TRUE(fn->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
  EXPECT_EQ(fn->getVisibility(), vpiPublicVis) << "8.18: 'constraint_mode' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(fn->getVirtual()) << "8.20: 'constraint_mode' is not declared 'virtual'";
  EXPECT_EQ(fn->getEndLabelObj(), nullptr) << "'endfunction' is written without ': constraint_mode'";
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, ConstraintModeHasNoFormalsAndReturnsSignedInt) {
  const hldb::Function *const fn = getConstraintMode();
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getIODecls() == nullptr || fn->getIODecls()->empty()) << "'constraint_mode()' has an empty port list";
  ExpectSignedInt(fn->getReturn(), "the return type of 'constraint_mode'");
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, ConstraintModeBodyIsReturnOne) {
  ASSERT_NE(getConstraintMode(), nullptr);
  const std::vector<const hldb::Any *> stmts = getConstraintModeBodyStmts();
  ASSERT_EQ(stmts.size(), 1u) << "'return 1;' is the function's only statement";
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(stmts[0]);
  ASSERT_NE(ret, nullptr) << "13.4.1: 'return 1;' should be a ReturnStmt";
  ExpectIntLiteral(ret->getCondition(), "1");
}

// ---------------------------------------------------------------------------
// class env extends uvm_env; ... endclass
// ---------------------------------------------------------------------------

TEST_F(ControllingConstraintsWithConstraintMode2Test, ClassEnvIsDeclaredOnceAsUserDefinedClass) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  size_t count = 0;
  for (const hldb::ClassDefn *const cls : *m_design->getAllClasses()) {
    if (cls->getName() == "env") {
      ++count;
    }
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
TEST_F(ControllingConstraintsWithConstraintMode2Test, ClassEnvExtendsUvmEnv) {
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

TEST_F(ControllingConstraintsWithConstraintMode2Test, ClassEnvHasPropertiesObjAndRetInOrder) {
  const hldb::ClassDefn *const env = getEnv();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getVariables(), nullptr);
  ASSERT_EQ(env->getVariables()->size(), 2u) << "'a obj' and 'int ret'";
  const std::vector<std::string_view> expected = {"obj", "ret"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = env->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getIsRandomized()) << "18.4: '" << expected[i] << "' has no 'rand'/'randc' qualifier";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, ObjIsHandleToAInitializedWithNew) {
  const hldb::Variable *const obj = getEnvProperty("obj");
  ASSERT_NE(obj, nullptr);
  ExpectClassHandle(obj->getTypespec(), getClassA(), "'obj'");
  const hldb::MethodFuncCall *const init = obj->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(init, nullptr) << "8.7: 'a obj = new;' initializes obj with a constructor call";
  EXPECT_EQ(init->getName(), "new");
  EXPECT_TRUE(init->getArguments() == nullptr || init->getArguments()->empty()) << "'new' is written with no arguments";
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, RetIsAnUninitializedSignedInt) {
  const hldb::Variable *const ret = getEnvProperty("ret");
  ASSERT_NE(ret, nullptr);
  ExpectSignedInt(ret->getTypespec(), "'ret'");
  EXPECT_EQ(ret->getValue(), nullptr) << "'int ret;' has no initializer";
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, ClassEnvHasMethodsNewAndRunPhase) {
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

TEST_F(ControllingConstraintsWithConstraintMode2Test, EnvMethodsHaveAutomaticLifetime) {
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
TEST_F(ControllingConstraintsWithConstraintMode2Test, EnvNewHasFormalsNameAndParent) {
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
TEST_F(ControllingConstraintsWithConstraintMode2Test, EnvNewBodyIsSuperNewOfNameAndParent) {
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
TEST_F(ControllingConstraintsWithConstraintMode2Test, RunPhaseHasOneUvmPhaseFormalAndEndLabel) {
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

TEST_F(ControllingConstraintsWithConstraintMode2Test, RunPhaseBodyIsRaiseBlockDrop) {
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
TEST_F(ControllingConstraintsWithConstraintMode2Test, RunPhaseRaisesObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(0), "raise_objection");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(ControllingConstraintsWithConstraintMode2Test, RunPhaseDropsObjectionOnThis) {
  ExpectObjectionCall(getRunPhaseStmt(2), "drop_objection");
}

// ---------------------------------------------------------------------------
// begin ret = obj.constraint_mode(); if ... end
// ---------------------------------------------------------------------------

TEST_F(ControllingConstraintsWithConstraintMode2Test, InnerBlockHoldsTwoStatementsInOrder) {
  const hldb::Begin *const block = getInnerBlock();
  ASSERT_NE(block, nullptr);
  EXPECT_TRUE(block->getVariables() == nullptr || block->getVariables()->empty()) << "the block declares no variable";
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 2u) << "lines 40 and 42; the empty line 41 is not a statement";
  EXPECT_NE(any_cast<hldb::Assignment>(getInnerStmt(0)), nullptr) << "statement 0 is 'ret = obj.constraint_mode();'";
  EXPECT_NE(getIfElse(), nullptr) << "12.4: statement 1 is an if with an else, so it is an IfElse";
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, RetIsAssignedFromObjConstraintModeCall) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(getInnerStmt(0));
  ASSERT_NE(assign, nullptr) << "'ret = obj.constraint_mode();' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "ret", getEnvProperty("ret"));

  const hldb::RefObj *const path = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'obj.constraint_mode()' should be a hierarchical RefObj";
  ASSERT_NE(path->getPathElems(), nullptr) << "'obj.constraint_mode()' is a member access";
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "'obj' and 'constraint_mode': no constraint name is written";
  ExpectBoundRef(path->getPathElems()->at(0), "obj", getEnvProperty("obj"));
  // Called on the object alone in an expression, this can only be class a's
  // own (illegal) function: the built-in object-level form is a task (18.9).
  // Which declaration it binds to is not asserted -- see the file header.
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'constraint_mode()' in an expression is a function call, so a MethodFuncCall";
  EXPECT_EQ(call->getName(), "constraint_mode");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "'constraint_mode()' is written with no arguments";
}

// ---------------------------------------------------------------------------
// if(ret == 1) begin `uvm_info(...) end else begin `uvm_error(...) end
// ---------------------------------------------------------------------------

TEST_F(ControllingConstraintsWithConstraintMode2Test, IfConditionIsRetEqOne) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Operation *const eq = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr) << "'ret == 1' should be an Operation";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u) << "'==' is a binary operator";
  ExpectBoundRef(eq->getOperands()->at(0), "ret", getEnvProperty("ret"));
  ExpectIntLiteral(eq->getOperands()->at(1), "1");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(ControllingConstraintsWithConstraintMode2Test, ThenBranchIsUvmInfoExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getStmt(), "uvm_report_info", "UVM_INFO", "UVM_LOW", "\"ret = %0d SUCCESS\"");
}

// Expected to fail with this .hlc -- see KNOWN FAILURE (uvm_pkg is never
// compiled) in the file header.
TEST_F(ControllingConstraintsWithConstraintMode2Test, ElseBranchIsUvmErrorExpansion) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  ExpectUvmReportMacro(ifElse->getElseStmt(), "uvm_report_error", "UVM_ERROR", "UVM_NONE", "\"ret = %0d FAILED\"");
}

// ---------------------------------------------------------------------------
// module top; env environment; initial begin ... end endmodule
// ---------------------------------------------------------------------------

TEST_F(ControllingConstraintsWithConstraintMode2Test, TopHasNoPortsAndOneEnvHandle) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top;' has no port list";
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "'env environment;' is the module's only declaration";
  const hldb::Variable *const environment = getEnvironment();
  ASSERT_NE(environment, nullptr) << "a class-typed data declaration is a variable (6.8)";
  ExpectClassHandle(environment->getTypespec(), getEnv(), "'environment'");
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, TopHasOneInitialWithTwoStatements) {
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

TEST_F(ControllingConstraintsWithConstraintMode2Test, InitialConstructsEnvironment) {
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
TEST_F(ControllingConstraintsWithConstraintMode2Test, InitialCallsRunTestFromUvmPkg) {
  const hldb::TaskCall *const call = any_cast<hldb::TaskCall>(getInitialStmt(1));
  ASSERT_NE(call, nullptr) << "statement 1 'run_test();' calls a task, so it is a TaskCall";
  EXPECT_EQ(call->getName(), "run_test");
  ASSERT_NE(getRunTest(), nullptr);
  EXPECT_EQ(call->getTaskFunc<hldb::Task>(), getRunTest()) << "26.3: run_test is imported from uvm_pkg";
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "'run_test()' writes no argument";
}

// ---------------------------------------------------------------------------
// Diagnostics: constraint_mode() is built in and cannot be overridden (18.9)
// ---------------------------------------------------------------------------

TEST_F(ControllingConstraintsWithConstraintMode2Test, EveryReferencedVariableAndFormalIsFound) {
  for (std::string_view name : {"obj", "ret", "name", "parent", "phase", "environment"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "'" << name << "' is declared in this file, so it is not undefined";
  }
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (illegal override
// of constraint_mode() accepted) in the file header.
TEST_F(ControllingConstraintsWithConstraintMode2Test, RedefiningConstraintModeIsReportedOnItsDeclaration) {
  GTEST_SKIP() << "HLC Gap: Compiler doesn't report this diagnosis yet.";
  EXPECT_NE(findError(ErrorDefinition::COMP_CANNOT_REDEFINE_BUILTIN_METHOD, "constraint_mode"), nullptr)
      << "18.9: 'The constraint_mode() method is built-in and cannot be overridden.'";
  const std::vector<uint32_t> lines = getDiagnosticLines(ErrorDefinition::COMP_CANNOT_REDEFINE_BUILTIN_METHOD, false);
  ASSERT_FALSE(lines.empty()) << "the redefinition must be reported in this file";
  size_t onDeclaration = 0;
  for (const uint32_t line : lines) {
    if (line == 23) {
      ++onDeclaration;
    }
  }
  EXPECT_GE(onDeclaration, 1u) << "line 23 declares 'function int constraint_mode();'";
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, RedefinitionDiagnosticIsAnError) {
  EXPECT_TRUE(isErrorSeverity(ErrorDefinition::COMP_CANNOT_REDEFINE_BUILTIN_METHOD))
      << "18.9: overriding constraint_mode() is illegal, so it is an error, not a warning";
}

TEST_F(ControllingConstraintsWithConstraintMode2Test, CompilationReportsAtLeastOneError) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbFatal + stats.nbSyntax + stats.nbError, 1)
      << "the source is illegal (18.9), so the compilation cannot be clean";
}

// Expected to fail -- see KNOWN FAILURE (errors reported on legal lines) in the
// file header.
TEST_F(ControllingConstraintsWithConstraintMode2Test, EveryErrorInThisFileIsOnTheConstraintModeDeclaration) {
  for (const Error &error : m_session->getErrorContainer()->getErrors()) {
    if (!isErrorSeverity(error.getType())) {
      continue;
    }
    for (const Location &loc : error.getLocations()) {
      if (FileSystem::toPath(loc.m_fileId).find(kSourceName) == std::string_view::npos) {
        continue;
      }
      EXPECT_TRUE(loc.m_line >= 23 && loc.m_line <= 25)
          << "error " << error.getType() << " at line " << loc.m_line << ":" << loc.m_column
          << ": only the constraint_mode declaration (lines 23-25) is illegal";
    }
  }
}

}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
