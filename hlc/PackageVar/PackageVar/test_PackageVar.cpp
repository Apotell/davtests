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

// Validates the HLDB model built for tests/PackageVar/dut.sv:
//
//   package pack;
//   class ovm_printer;
//   endclass
//   class ovm_options_container;
//   ovm_printer     printer;
//   endclass
//   ovm_options_container ovm_auto_options_object = ovm_options_container::init();
//   class ovm_object;
//   extern function void print(ovm_printer printer=null);
//   //ovm_auto_options_object.printer = printer;
//   //endfunction
//   endclass
//   function void ovm_object::print(ovm_printer printer=null);
//     if(printer==null)
//       printer = ovm_default_printer;
//     if(printer.istop()) begin
//       printer.print_object(get_name(), this);
//     end
//     else begin
//       ovm_auto_options_object.printer = printer;
//       m_field_automation(null, OVM_PRINT, "");
//       do_print(printer);
//     end
//   endfunction
//   endpackage
//
// The point of the fixture is a package variable referenced from the body of
// a class method that is defined out of block (IEEE 1800-2023 8.24) in the
// same package: 'ovm_auto_options_object.printer = printer;' must bind the
// package variable, its class member, and the method's formal. The fixture
// is lifted from OVM and is incomplete: init, ovm_default_printer, istop,
// print_object, get_name, m_field_automation, OVM_PRINT and do_print are
// declared nowhere in it. The regression this file exists to catch is HLC
// losing the package-level bindings inside the out-of-block method.
//
// What is checked, and why:
//   Package pack (26.2)
//     - exactly 3 classes: ovm_printer, ovm_options_container, ovm_object
//     - ovm_printer declares no property and no method
//     - ovm_options_container declares exactly 1 property, 'printer', a
//       handle to ovm_printer
//     - exactly 1 package variable, ovm_auto_options_object, a handle to
//       ovm_options_container (8.4), static like every package variable
//       (6.21). Its initializer calls 'init' through the class scope
//       resolution operator (8.23); ovm_options_container declares no init,
//       so the call binds to nothing
//   Method ovm_object::print (8.24)
//     - its definition is a Function named "print" with no return value
//       (13.4.1) and exactly 1 formal, 'printer': an input (13.4) that is a
//       handle to ovm_printer, with the default value null (13.5.3)
//     - a class method is automatic (8.6)
//     - its body holds exactly 2 statements, an if without else (IfStmt) and
//       an if-else (IfElse) (12.4)
//   if(printer==null) printer = ovm_default_printer;
//     - the condition is Operation vpiEqOp over RefObj 'printer', bound to
//       the formal, and the null literal (vpiNullConst)
//     - the statement assigns to the formal 'printer' the RefObj
//       'ovm_default_printer', which is declared nowhere and binds to nothing
//   if(printer.istop()) begin ... end else begin ... end
//     - the condition calls 'istop' on the formal: its prefix is bound to
//       the formal, and the call binds to nothing, because ovm_printer
//       declares no istop
//     - the then-branch calls 'print_object' on the formal with 2 arguments:
//       a call to 'get_name' (declared nowhere) and 'this', bound to the
//       class ovm_object (8.11); print_object binds to nothing
//     - the else-branch holds exactly 3 statements:
//       * ovm_auto_options_object.printer = printer; -- a blocking
//         Assignment whose LHS is a RefObj path with the prefix bound to the
//         package variable ovm_auto_options_object and the member 'printer'
//         bound to ovm_options_container's property, and whose RHS is bound
//         to the formal 'printer'
//       * m_field_automation(null, OVM_PRINT, ""); -- a call that binds to
//         nothing, with the arguments null, RefObj 'OVM_PRINT' (declared
//         nowhere, bound to nothing) and the empty string ""
//       * do_print(printer); -- a call that binds to nothing, with the one
//         argument 'printer' bound to the formal
//   Diagnostics
//     - ovm_default_printer and OVM_PRINT are data references that match no
//       declaration. For a reference other than a subroutine call "it shall
//       be illegal if no identifier can be found that matches the reference"
//       (26.3), so each is reported at error severity, as
//       COMP_UNDEFINED_VARIABLE, ELAB_UNDEF_VARIABLE or COMP_FAILED_TO_BIND
//     - ovm_options_container is declared once; naming it as a type and as a
//       class scope prefix (8.23) declares nothing, so there is no
//       COMP_MULTIPLY_DEFINED_TYPEDEF or COMP_MULTIPLY_DEFINED_CLASS for it
//     - the declared names are not reported: no COMP_UNDEFINED_VARIABLE or
//       COMP_FAILED_TO_BIND for ovm_auto_options_object
//     - there is no syntax error: zero syntax and zero fatal diagnostics
//
// Reduction and elaboration: there is no expression to reduce and no module
// to elaborate, so no check is gated on getElaborated().
//
// KNOWN COMPILER BUG (class-method lifetime), not a defect in this test: 8.6
// says "The lifetime of methods declared as part of a class type shall be
// automatic", but HLC gives class methods the static default of 13.4.2.
// PrintIsAutomatic asserts the LRM value and is expected to fail until HLC
// is fixed; it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (undeclared identifier not reported as an error), not a defect in this test:
// the references are left
// unbound, as they should be, but HLC reports no error for them; 26.3 makes
// each such reference illegal.
// UndeclaredDataReferencesAreReportedAsErrors is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (class reported as a multiply defined typedef), not a defect in this test:
// HLC reports CP5824 "Multiply
// defined typedef" for ovm_options_container, which is declared exactly
// once. Naming the class as a type and as a class scope prefix (8.23)
// declares nothing.
// OptionsContainerIsNotReportedMultiplyDefined is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Which diagnostic, if any, HLC gives for the unresolved calls (init,
//     istop, print_object, get_name, m_field_automation, do_print). The
//     standard says how subroutine names are searched (26.3, 23.8.1) and
//     that items in a package shall not use hierarchical references (26.2),
//     but names no specific diagnostic, so only the fact the source fixes --
//     that none of these calls has a declaration to bind to -- is asserted.
//   - Where HLC keeps the extern prototype and how it links it to the
//     out-of-block definition is a model convention. The definition is
//     found by name, in the class's methods or the package's subroutines.
//   - Whether HLC models a call as FuncCall, TaskCall or MethodFuncCall is
//     a tool convention; calls are checked through TFCall by name,
//     arguments and binding.
//   - How a void function records its return type (no RefTypespec, or one
//     resolving to a VoidTypespec) is a tool convention; either is
//     accepted.
//   - What print does when called only exists while simulation runs.
//     Permanently out of scope; the static half, which declarations each
//     statement binds, is covered by the tests above.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; pack is looked up by
//     name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The '//' comments in the class body are not design objects (5.4).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
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
#include <hldb/function.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/io_decl.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/tf_call.h>
#include <hldb/variable.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

#include <initializer_list>
#include <string_view>

namespace hlc {

class PackageVarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageVar.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("pack", m_design->getAllPackages()); }

  static const hldb::ClassDefn *getClass(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>(name, pkg->getClassDefns());
  }

  static const hldb::Variable *getAutoOptionsObject() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("ovm_auto_options_object", pkg->getVariables());
  }

  static const hldb::Variable *getContainerPrinter() {
    const hldb::ClassDefn *const cls = getClass("ovm_options_container");
    if (cls == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("printer", cls->getVariables());
  }

  // The definition of ovm_object::print: the Function named "print" that
  // has a body, found among ovm_object's methods or the package's
  // subroutines (where HLC keeps it is a model convention).
  static const hldb::Function *getPrint() {
    const hldb::ClassDefn *const cls = getClass("ovm_object");
    if (cls != nullptr && cls->getMethods() != nullptr) {
      for (const hldb::TaskFunc *const tf : *cls->getMethods()) {
        const hldb::Function *const fn = any_cast<hldb::Function>(tf);
        if ((fn != nullptr) && (fn->getName() == "print") && (fn->getStmt() != nullptr)) return fn;
      }
    }
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr || pkg->getTaskFuncs() == nullptr) return nullptr;
    for (const hldb::TaskFunc *const tf : *pkg->getTaskFuncs()) {
      const hldb::Function *const fn = any_cast<hldb::Function>(tf);
      if ((fn != nullptr) && (fn->getName() == "print") && (fn->getStmt() != nullptr)) return fn;
    }
    return nullptr;
  }

  static const hldb::IODecl *getFormal() {
    const hldb::Function *const fn = getPrint();
    if (fn == nullptr || fn->getIODecls() == nullptr || fn->getIODecls()->empty()) return nullptr;
    return fn->getIODecls()->at(0);
  }

  static const hldb::Any *getBodyStmt(size_t index) {
    const hldb::Function *const fn = getPrint();
    if (fn == nullptr) return nullptr;
    const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= index) return nullptr;
    return body->getStmts()->at(index);
  }

  static const hldb::IfElse *getIfElse() { return any_cast<hldb::IfElse>(getBodyStmt(1)); }

  static const hldb::Any *getElseStmt(size_t index) {
    const hldb::IfElse *const ifElse = getIfElse();
    if (ifElse == nullptr) return nullptr;
    const hldb::Begin *const block = ifElse->getElseStmt<hldb::Begin>();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() <= index) return nullptr;
    return block->getStmts()->at(index);
  }

  // Verifies 'type' is a handle to the class 'className' of package pack.
  static void ExpectClassHandle(const hldb::RefTypespec *type, std::string_view className, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::ClassTypespec *const ct = type->getActual<hldb::ClassTypespec>();
    ASSERT_NE(ct, nullptr) << what << " is declared with a class type";
    ASSERT_NE(getClass(className), nullptr) << "class '" << className << "' not found";
    EXPECT_EQ(ct->getClassDefn(), getClass(className)) << what << " is a handle to " << className;
  }

  // Verifies 'expr' is a RefObj named 'printer' bound to print's formal.
  static void ExpectFormalRef(const hldb::Any *expr) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'printer' should be a RefObj";
    EXPECT_EQ(ref->getName(), "printer");
    ASSERT_NE(getFormal(), nullptr);
    EXPECT_EQ(ref->getActual(), getFormal()) << "'printer' is print's own formal";
  }

  // Verifies 'expr' is a RefObj named 'name' that binds to nothing.
  static void ExpectUnboundRef(const hldb::Any *expr, std::string_view name) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    EXPECT_EQ(ref->getActual(), nullptr) << "'" << name << "' is declared nowhere, so it cannot bind";
  }

  // Verifies 'expr' is a call named 'name' that binds to nothing, and stores
  // the call in 'call'.
  static void ExpectUnboundCall(const hldb::Any *expr, std::string_view name, const hldb::TFCall **call) {
    *call = nullptr;
    const hldb::TFCall *const found = any_cast<hldb::TFCall>(expr);
    ASSERT_NE(found, nullptr) << "'" << name << "(...)' should be a call";
    EXPECT_EQ(found->getName(), name);
    EXPECT_EQ(found->getTaskFunc(), nullptr) << "'" << name << "' is declared nowhere, so the call cannot bind";
    *call = found;
  }

  // Verifies 'expr' is 'printer.<method>(...)': a RefObj path whose prefix
  // is bound to the formal and whose call binds to nothing. Stores the call
  // in 'call'.
  static void ExpectUnboundMethodCallOnFormal(const hldb::Any *expr, std::string_view method,
                                              const hldb::TFCall **call) {
    *call = nullptr;
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'printer." << method << "(...)' should be a RefObj path";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    ExpectFormalRef(path->getPathElems()->at(0));
    ExpectUnboundCall(path->getPathElems()->at(1), method, call);
  }

  // Verifies one of 'types' was reported naming 'symbol', at error severity:
  // the standard makes the reference illegal.
  void ExpectReportedAsError(std::initializer_list<ErrorDefinition::ErrorType> types, std::string_view symbol) {
    const Error *found = nullptr;
    for (const ErrorDefinition::ErrorType type : types) {
      found = findError(type, symbol);
      if (found != nullptr) break;
    }
    ASSERT_NE(found, nullptr) << "no diagnostic names '" << symbol << "'";
    const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
    const ErrorDefinition::ErrorMap::const_iterator info = infos.find(found->getType());
    ASSERT_NE(info, infos.end());
    EXPECT_EQ(info->second.m_severity, ErrorDefinition::ERROR)
        << "'" << symbol << "' makes the source illegal, so it is an error, not a warning";
  }
};

// ---------------------------------------------------------------------------
// Classes of package pack
// ---------------------------------------------------------------------------

TEST_F(PackageVarTest, PackageDeclaresThreeClasses) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'pack' not found";
  ASSERT_NE(pkg->getClassDefns(), nullptr);
  EXPECT_EQ(pkg->getClassDefns()->size(), 3u);
  for (std::string_view name : {"ovm_printer", "ovm_options_container", "ovm_object"}) {
    EXPECT_NE(getClass(name), nullptr) << "class '" << name << "' not found in pack";
  }
}

TEST_F(PackageVarTest, OvmPrinterIsEmpty) {
  const hldb::ClassDefn *const cls = getClass("ovm_printer");
  ASSERT_NE(cls, nullptr);
  EXPECT_TRUE(cls->getVariables() == nullptr || cls->getVariables()->empty()) << "ovm_printer declares no property";
  if (cls->getMethods() != nullptr) {
    for (const hldb::TaskFunc *const tf : *cls->getMethods()) {
      EXPECT_EQ(tf->getName(), "new") << "ovm_printer declares no method; only an implicit constructor may exist (8.7)";
    }
  }
}

TEST_F(PackageVarTest, OptionsContainerHasPrinterHandle) {
  const hldb::ClassDefn *const cls = getClass("ovm_options_container");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'ovm_printer printer;' is the only property";
  const hldb::Variable *const printer = getContainerPrinter();
  ASSERT_NE(printer, nullptr);
  ExpectClassHandle(printer->getTypespec(), "ovm_printer", "'printer'");
}

// ---------------------------------------------------------------------------
// ovm_options_container ovm_auto_options_object = ovm_options_container::init();
// ---------------------------------------------------------------------------

TEST_F(PackageVarTest, PackageVariableIsStaticOptionsContainerHandle) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getVariables(), nullptr);
  EXPECT_EQ(pkg->getVariables()->size(), 1u) << "'ovm_auto_options_object' is the only package variable";
  const hldb::Variable *const v = getAutoOptionsObject();
  ASSERT_NE(v, nullptr) << "package variable 'ovm_auto_options_object' not found";
  ExpectClassHandle(v->getTypespec(), "ovm_options_container", "'ovm_auto_options_object'");
  EXPECT_FALSE(v->getAutomatic()) << "6.21: a variable declared in a package is static";
}

TEST_F(PackageVarTest, PackageVariableInitializerCallsUndeclaredInit) {
  const hldb::Variable *const v = getAutoOptionsObject();
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getValue(), nullptr) << "the declaration has an initializer";
  const hldb::Any *callExpr = v->getValue();
  if (const hldb::RefObj *const path = any_cast<hldb::RefObj>(callExpr)) {
    // 'ovm_options_container::init()' modeled as a path: the class, then the
    // call.
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u) << "the class, then the call";
    const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(scope, nullptr);
    EXPECT_EQ(scope->getName(), "ovm_options_container");
    EXPECT_EQ(scope->getActual(), getClass("ovm_options_container")) << "8.23: the prefix names the class";
    callExpr = path->getPathElems()->at(1);
  }
  const hldb::TFCall *const call = any_cast<hldb::TFCall>(callExpr);
  ASSERT_NE(call, nullptr) << "the initializer calls 'init'";
  EXPECT_TRUE(call->getName() == "init" || call->getName() == "ovm_options_container::init")
      << "unexpected call name '" << call->getName() << "'";
  EXPECT_EQ(call->getTaskFunc(), nullptr) << "8.23: ovm_options_container declares no 'init', so the call cannot bind";
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "'init()' writes no argument";
}

// ---------------------------------------------------------------------------
// function void ovm_object::print(ovm_printer printer=null);
// ---------------------------------------------------------------------------

TEST_F(PackageVarTest, PrintIsVoidFunctionWithOneFormalPrinter) {
  const hldb::Function *const fn = getPrint();
  ASSERT_NE(fn, nullptr) << "8.24: the out-of-block definition of ovm_object::print not found";
  EXPECT_TRUE(fn->getReturn() == nullptr || fn->getReturn()->getActual<hldb::VoidTypespec>() != nullptr)
      << "13.4.1: print is declared 'void'";
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u) << "'printer' is the only formal";
  const hldb::IODecl *const formal = getFormal();
  ASSERT_NE(formal, nullptr);
  EXPECT_EQ(formal->getName(), "printer");
  EXPECT_EQ(formal->getDirection(), vpiInput) << "13.4: a formal with no direction is an input";
  ExpectClassHandle(formal->getTypespec(), "ovm_printer", "the formal 'printer'");
}

TEST_F(PackageVarTest, FormalPrinterDefaultsToNull) {
  const hldb::IODecl *const formal = getFormal();
  ASSERT_NE(formal, nullptr);
  const hldb::Constant *const def = formal->getExpr<hldb::Constant>();
  ASSERT_NE(def, nullptr) << "13.5.3: 'printer=null' gives the formal a default value";
  EXPECT_EQ(def->getConstType(), vpiNullConst) << "the default value is the null literal";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (class-method
// lifetime) in the file header.
TEST_F(PackageVarTest, PrintIsAutomatic) {
  const hldb::Function *const fn = getPrint();
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
}

TEST_F(PackageVarTest, PrintBodyIsIfThenIfElse) {
  const hldb::Function *const fn = getPrint();
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a body of two statements is wrapped in a begin-end";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);
  EXPECT_NE(any_cast<hldb::IfStmt>(body->getStmts()->at(0)), nullptr) << "12.4: the first if has no else";
  EXPECT_NE(getIfElse(), nullptr) << "12.4: the second if has an else";
}

// ---------------------------------------------------------------------------
// if(printer==null) printer = ovm_default_printer;
// ---------------------------------------------------------------------------

TEST_F(PackageVarTest, FirstIfTestsFormalAgainstNull) {
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(getBodyStmt(0));
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Operation *const eq = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr) << "'printer==null' is an Operation";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  ExpectFormalRef(eq->getOperands()->at(0));
  const hldb::Constant *const null = any_cast<hldb::Constant>(eq->getOperands()->at(1));
  ASSERT_NE(null, nullptr);
  EXPECT_EQ(null->getConstType(), vpiNullConst) << "'null' is the null literal";
}

TEST_F(PackageVarTest, FirstIfAssignsUndeclaredDefaultPrinter) {
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(getBodyStmt(0));
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Assignment *const assign = ifStmt->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "the if's statement is 'printer = ovm_default_printer;'";
  EXPECT_TRUE(assign->getBlocking());
  ExpectFormalRef(assign->getLhs());
  ExpectUnboundRef(assign->getRhs(), "ovm_default_printer");
}

// ---------------------------------------------------------------------------
// if(printer.istop()) begin printer.print_object(get_name(), this); end
// ---------------------------------------------------------------------------

TEST_F(PackageVarTest, SecondIfCallsUndeclaredIstopOnFormal) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::TFCall *call = nullptr;
  ExpectUnboundMethodCallOnFormal(ifElse->getCondition(), "istop", &call);
  ASSERT_NE(call, nullptr);
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "'istop()' writes no argument";
}

TEST_F(PackageVarTest, ThenBranchCallsUndeclaredPrintObject) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Begin *const block = ifElse->getStmt<hldb::Begin>();
  ASSERT_NE(block, nullptr) << "the then-branch is a begin-end";
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 1u);
  const hldb::TFCall *call = nullptr;
  ExpectUnboundMethodCallOnFormal(block->getStmts()->at(0), "print_object", &call);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "'get_name()' and 'this'";
  const hldb::TFCall *getName = nullptr;
  ExpectUnboundCall(call->getArguments()->at(0), "get_name", &getName);
  const hldb::RefObj *const self = any_cast<hldb::RefObj>(call->getArguments()->at(1));
  ASSERT_NE(self, nullptr) << "'this' should be a RefObj";
  EXPECT_EQ(self->getName(), "this");
  ASSERT_NE(getClass("ovm_object"), nullptr);
  EXPECT_EQ(self->getActual(), getClass("ovm_object")) << "8.11: 'this' is the class the method belongs to";
}

// ---------------------------------------------------------------------------
// else begin ovm_auto_options_object.printer = printer; ... end
// ---------------------------------------------------------------------------

TEST_F(PackageVarTest, ElseBranchHoldsThreeStatements) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Begin *const block = ifElse->getElseStmt<hldb::Begin>();
  ASSERT_NE(block, nullptr) << "the else-branch is a begin-end";
  ASSERT_NE(block->getStmts(), nullptr);
  EXPECT_EQ(block->getStmts()->size(), 3u) << "the assignment, then two calls; the '//' lines are comments";
}

TEST_F(PackageVarTest, ElseAssignsFormalToPackageVariableMember) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(getElseStmt(0));
  ASSERT_NE(assign, nullptr) << "'ovm_auto_options_object.printer = printer;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' is a blocking assignment";
  const hldb::RefObj *const path = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "the LHS should be a RefObj path";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package variable, then its member";
  const hldb::RefObj *const object = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(object, nullptr);
  EXPECT_EQ(object->getName(), "ovm_auto_options_object");
  ASSERT_NE(getAutoOptionsObject(), nullptr);
  EXPECT_EQ(object->getActual(), getAutoOptionsObject())
      << "26.3: the out-of-block method is in package pack, so the name binds to pack's variable";
  const hldb::RefObj *const member = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), "printer");
  ASSERT_NE(getContainerPrinter(), nullptr);
  EXPECT_EQ(member->getActual(), getContainerPrinter()) << "'printer' is ovm_options_container's property";
  ExpectFormalRef(assign->getRhs());
}

TEST_F(PackageVarTest, ElseCallsUndeclaredFieldAutomation) {
  const hldb::TFCall *call = nullptr;
  ExpectUnboundCall(getElseStmt(1), "m_field_automation", &call);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 3u) << "'null', 'OVM_PRINT' and \"\"";
  const hldb::Constant *const null = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(null, nullptr);
  EXPECT_EQ(null->getConstType(), vpiNullConst);
  ExpectUnboundRef(call->getArguments()->at(1), "OVM_PRINT");
  const hldb::Constant *const empty = any_cast<hldb::Constant>(call->getArguments()->at(2));
  ASSERT_NE(empty, nullptr);
  EXPECT_EQ(empty->getConstType(), vpiStringConst) << "5.9: \"\" is a string literal";
  EXPECT_EQ(empty->getDecompile(), "\"\"");
}

TEST_F(PackageVarTest, ElseCallsUndeclaredDoPrintWithFormal) {
  const hldb::TFCall *call = nullptr;
  ExpectUnboundCall(getElseStmt(2), "do_print", &call);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  ExpectFormalRef(call->getArguments()->at(0));
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG
// (undeclared identifier not reported as an error) in the file header.
TEST_F(PackageVarTest, UndeclaredDataReferencesAreReportedAsErrors) {
  for (std::string_view name : {"ovm_default_printer", "OVM_PRINT"}) {
    ExpectReportedAsError({ErrorDefinition::COMP_UNDEFINED_VARIABLE, ErrorDefinition::ELAB_UNDEF_VARIABLE,
                           ErrorDefinition::COMP_FAILED_TO_BIND},
                          name);
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG
// (class reported as a multiply defined typedef) in the file header.
TEST_F(PackageVarTest, OptionsContainerIsNotReportedMultiplyDefined) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_TYPEDEF, "ovm_options_container"), nullptr)
      << "class ovm_options_container is declared exactly once, and using it as a type and as a class scope "
         "prefix (8.23) declares nothing";
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_CLASS, "ovm_options_container"), nullptr)
      << "class ovm_options_container is declared exactly once";
}

TEST_F(PackageVarTest, PackageVariableIsNotReported) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, "ovm_auto_options_object"), nullptr)
      << "'ovm_auto_options_object' is declared in pack";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "ovm_auto_options_object"), nullptr)
      << "'ovm_auto_options_object' is declared in pack";
}

TEST_F(PackageVarTest, NoSyntaxOrFatalDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
