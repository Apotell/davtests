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

// Validates the HLDB model built for tests/PackageDpi/dut.sv:
//
//   package pack;
//   import "DPI-C" context function int uvm_hdl_check_path(string path);
//   string uvm_aa_string_key;
//   typedef enum {
//   UVM_CORE_UNINITIALIZED,
//   UVM_CORE_PRE_INIT
//   } uvm_core_state;
//   uvm_core_state m_uvm_core_state = UVM_CORE_UNINITIALIZED;
//   uvm_object_wrapper uvm_deferred_init[$];
//   class toto;
//   const static int DO_NOT_CATCH      = 1;
//   endclass
//   const static int DO_NOT_CATCH_2     = 1;
//   endpackage
//
// What is checked, and why:
//   Package pack (26.2) exists; no ': label' after endpackage.
//   import "DPI-C" context function int uvm_hdl_check_path(string path);
//     - the package holds exactly 1 TaskFuncDecl, a FunctionDecl named
//       "uvm_hdl_check_path", and no TaskFunc (35.5.4 -- an import
//       declaration declares a SystemVerilog function whose body is in C)
//     - vpiAccessType is vpiDPIImportAcc (the IEEE 1800 VPI task/function
//       model's access type for DPI imports)
//     - the spec string "DPI-C" is recorded as vpiDPIC, not vpiDPI (35.5.4)
//     - 'context' is recorded, 'pure' is not (35.5.2, 35.5.3)
//     - an import has no SystemVerilog body, so the declaration points to
//       no TaskFunc definition (getTaskFunc() is null)
//     - return type 'int' -> IntTypespec, signed (6.11)
//     - exactly 1 formal 'path': direction defaults to input, type string
//     - no c_identifier is written, so the linkage name, if HLC records one,
//       is the SystemVerilog name itself (35.5.4)
//   Package-level data (6.8, 6.21)
//     - exactly 4 package Variables: uvm_aa_string_key, m_uvm_core_state,
//       uvm_deferred_init and DO_NOT_CATCH_2 (the typedef, the class and
//       the DPI import are not variables)
//     - 'string uvm_aa_string_key;' -> StringTypespec, no initializer
//     - 'uvm_deferred_init[$]' -> ArrayTypespec with vpiQueueArray, which is
//       an unpacked dimension (7.10)
//     - 'const static int DO_NOT_CATCH_2 = 1;' -> const qualifier recorded
//       (6.20.6), static lifetime (6.21: package variables are static, and
//       'static' is written explicitly), signed IntTypespec, initializer
//       Constant "1"
//   typedef enum { ... } uvm_core_state; (6.19)
//     - the package owns exactly 1 TypedefTypespec 'uvm_core_state', aliasing
//       an EnumTypespec with exactly 2 names, in source order
//       UVM_CORE_UNINITIALIZED, UVM_CORE_PRE_INIT
//   uvm_core_state m_uvm_core_state = UVM_CORE_UNINITIALIZED;
//     - typed by the RefTypespec named "uvm_core_state", resolving to the
//       package's typedef (or the enum it aliases -- both are that type)
//     - its initializer is the enum name UVM_CORE_UNINITIALIZED, bound by
//       object identity to the first EnumConst; if HLC folded it instead,
//       the value must be 0 (6.19: the first name defaults to 0)
//   class toto; const static int DO_NOT_CATCH = 1; endclass (8.9, 8.19)
//     - the package holds exactly 1 ClassDefn, 'toto', a user-defined class
//     - it has exactly 1 property 'DO_NOT_CATCH': const recorded, static
//       (Variable::getAutomatic() false, the accessor already used for
//       8.9 static properties), signed IntTypespec, initializer "1"
//   Diagnostics
//     - 'uvm_object_wrapper' is declared nowhere in the design, so using it
//       as a data type is an error: COMP_UNDEFINED_TYPE naming it
//     - that is a semantic error, not a syntax error: zero syntax and zero
//       fatal diagnostics
//     - the names that ARE declared must not be reported: no
//       COMP_UNDEFINED_TYPE for 'uvm_core_state', no COMP_FAILED_TO_BIND
//       for 'UVM_CORE_UNINITIALIZED'
//
// What is NOT checked, and why:
//   - The element typespec of 'uvm_deferred_init'. Its type is undeclared,
//     so what HLC puts there (an unsupported placeholder, an unresolved
//     class typespec, or nothing) is a tool convention the source cannot
//     determine. The queue dimension itself is asserted in
//     QueueUvmDeferredInitIsUnpackedQueue.
//   - The implicit values of the enum names (0 and 1, 6.19) are not written
//     in the source, and whether HLC materializes them as value expressions
//     is a tool convention; they are not asserted.
//   - Whether the enum with no explicit base type records an implicit 'int'
//     base typespec is a tool convention and is not asserted.
//   - Calling uvm_hdl_check_path, and anything the C side does, only exists
//     while simulation runs and is permanently out of scope; the static
//     half -- the import's declaration and flags -- is covered by the
//     DpiImport* tests.
//   - This fixture has nothing that must reduce or elaborate, so no check
//     is gated on Design::getElaborated().

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/function_decl.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PackageDpiTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageDpi.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("pack", m_design->getAllPackages()); }

  // A DPI import declares a function whose body is in C, so HLDB records it
  // as a FunctionDecl (a declaration with no SystemVerilog definition).
  static const hldb::FunctionDecl *getImport() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::FunctionDecl>("uvm_hdl_check_path", pkg->getTaskFuncDecls());
  }

  static const hldb::Variable *getPkgVar(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, pkg->getVariables());
  }

  static const hldb::TypedefTypespec *getTypedef() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::TypedefTypespec>("uvm_core_state", pkg->getTypespecs());
  }

  static const hldb::EnumTypespec *getEnum() {
    const hldb::TypedefTypespec *const td = getTypedef();
    if (td == nullptr || td->getTypedef() == nullptr || td->getTypedef()->getAlias() == nullptr) return nullptr;
    return td->getTypedef()->getAlias()->getActual<hldb::EnumTypespec>();
  }

  // The enum declaration the typespec refers to. It owns the enum constants.
  static const hldb::Enum *getEnumDecl() {
    const hldb::EnumTypespec *const et = getEnum();
    if (et == nullptr) return nullptr;
    return et->getEnum();
  }

  static const hldb::ClassDefn *getToto() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("toto", pkg->getClassDefns());
  }
};

// ---------------------------------------------------------------------------
// package pack
// ---------------------------------------------------------------------------

TEST_F(PackageDpiTest, PackageExistsWithoutEndLabel) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'pack' not found in Design::getAllPackages()";
  EXPECT_EQ(pkg->getName(), "pack");
  EXPECT_EQ(pkg->getEndLabel(), "");
}

// ---------------------------------------------------------------------------
// import "DPI-C" context function int uvm_hdl_check_path(string path);
// ---------------------------------------------------------------------------

TEST_F(PackageDpiTest, DpiImportIsTheOnlyPackageFunction) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTaskFuncDecls(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncDecls()->size(), 1u) << "the only subroutine in 'pack' is the DPI import";
  EXPECT_TRUE(pkg->getTaskFuncs() == nullptr || pkg->getTaskFuncs()->empty())
      << "'pack' defines no subroutine with a SystemVerilog body";
  const hldb::FunctionDecl *const fn = getImport();
  ASSERT_NE(fn, nullptr) << "35.5.4: the import should declare a function 'uvm_hdl_check_path'";
  EXPECT_EQ(fn->getName(), "uvm_hdl_check_path");
}

TEST_F(PackageDpiTest, DpiImportAccessTypeIsDpiImport) {
  const hldb::FunctionDecl *const fn = getImport();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getAccessType(), vpiDPIImportAcc) << "an 'import \"DPI-C\"' function has vpiDPIImportAcc access";
}

TEST_F(PackageDpiTest, DpiImportSpecStringIsDpiC) {
  const hldb::FunctionDecl *const fn = getImport();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getDPICStr(), vpiDPIC) << "35.5.4: the spec string is \"DPI-C\", not the deprecated \"DPI\"";
}

TEST_F(PackageDpiTest, DpiImportIsContextNotPure) {
  const hldb::FunctionDecl *const fn = getImport();
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getDPIContext()) << "35.5.3: the import is declared 'context'";
  EXPECT_FALSE(fn->getDPIPure()) << "35.5.2: the import is not declared 'pure'";
}

TEST_F(PackageDpiTest, DpiImportHasNoBody) {
  const hldb::FunctionDecl *const fn = getImport();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getTaskFunc(), nullptr)
      << "an imported function is implemented in C; there is no SystemVerilog definition for it to point to";
}

TEST_F(PackageDpiTest, DpiImportReturnsSignedInt) {
  const hldb::FunctionDecl *const fn = getImport();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getReturn(), nullptr);
  const hldb::IntTypespec *const ts = fn->getReturn()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "return type 'int' should resolve to IntTypespec";
  EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is signed";
}

TEST_F(PackageDpiTest, DpiImportHasOneInputStringFormalPath) {
  const hldb::FunctionDecl *const fn = getImport();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const path = fn->getIODecls()->at(0);
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->getName(), "path");
  EXPECT_EQ(path->getDirection(), vpiInput) << "a formal with no direction defaults to input";
  ASSERT_NE(path->getTypespec(), nullptr);
  EXPECT_NE(path->getTypespec()->getActual<hldb::StringTypespec>(), nullptr) << "formal 'path' is a string";
}

TEST_F(PackageDpiTest, DpiImportLinkageNameDefaultsToFunctionName) {
  const hldb::FunctionDecl *const fn = getImport();
  ASSERT_NE(fn, nullptr);
  const std::string_view cName = fn->getDPICIdentifier();
  EXPECT_TRUE(cName.empty() || (cName == "uvm_hdl_check_path"))
      << "35.5.4: with no c_identifier the linkage name is the function's own name, got '" << cName << "'";
}

// ---------------------------------------------------------------------------
// Package-level data declarations
// ---------------------------------------------------------------------------

TEST_F(PackageDpiTest, PackageHasExactlyFourVariables) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getVariables(), nullptr);
  EXPECT_EQ(pkg->getVariables()->size(), 4u)
      << "uvm_aa_string_key, m_uvm_core_state, uvm_deferred_init and DO_NOT_CATCH_2";
  EXPECT_NE(getPkgVar("uvm_aa_string_key"), nullptr);
  EXPECT_NE(getPkgVar("m_uvm_core_state"), nullptr);
  EXPECT_NE(getPkgVar("uvm_deferred_init"), nullptr);
  EXPECT_NE(getPkgVar("DO_NOT_CATCH_2"), nullptr);
  EXPECT_EQ(getPkgVar("DO_NOT_CATCH"), nullptr) << "DO_NOT_CATCH belongs to class toto, not to the package";
}

TEST_F(PackageDpiTest, StringUvmAaStringKeyHasNoInitializer) {
  const hldb::Variable *const v = getPkgVar("uvm_aa_string_key");
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  EXPECT_NE(v->getTypespec()->getActual<hldb::StringTypespec>(), nullptr) << "declared 'string'";
  EXPECT_EQ(v->getValue(), nullptr) << "'string uvm_aa_string_key;' has no initializer";
}

TEST_F(PackageDpiTest, QueueUvmDeferredInitIsUnpackedQueue) {
  const hldb::Variable *const v = getPkgVar("uvm_deferred_init");
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = v->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'uvm_deferred_init[$]' should resolve to an ArrayTypespec";
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: '[$]' declares a queue";
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
  EXPECT_EQ(v->getValue(), nullptr) << "the queue has no initializer";
}

TEST_F(PackageDpiTest, ConstStaticDoNotCatch2) {
  const hldb::Variable *const v = getPkgVar("DO_NOT_CATCH_2");
  ASSERT_NE(v, nullptr);
  EXPECT_TRUE(v->getConstantVariable()) << "6.20.6: declared 'const'";
  EXPECT_FALSE(v->getAutomatic()) << "6.21: declared 'static' (and package variables are static)";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::IntTypespec *const ts = v->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "declared 'int'";
  EXPECT_TRUE(ts->getSigned());
  const hldb::Constant *const init = v->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// typedef enum { UVM_CORE_UNINITIALIZED, UVM_CORE_PRE_INIT } uvm_core_state;
// ---------------------------------------------------------------------------

TEST_F(PackageDpiTest, PackageOwnsOneTypedefUvmCoreState) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTypespecs(), nullptr);
  size_t typedefCount = 0;
  for (const hldb::Typespec *const ts : *pkg->getTypespecs()) {
    if (any_cast<hldb::TypedefTypespec>(ts) != nullptr) ++typedefCount;
  }
  EXPECT_EQ(typedefCount, 1u) << "'pack' declares exactly one typedef";
  const hldb::TypedefTypespec *const td = getTypedef();
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getName(), "uvm_core_state");
}

TEST_F(PackageDpiTest, UvmCoreStateHasTwoNamesInOrder) {
  ASSERT_NE(getEnum(), nullptr) << "typedef 'uvm_core_state' should alias an EnumTypespec";
  const hldb::Enum *const decl = getEnumDecl();
  ASSERT_NE(decl, nullptr);
  ASSERT_NE(decl->getEnumConsts(), nullptr);
  ASSERT_EQ(decl->getEnumConsts()->size(), 2u);
  EXPECT_EQ(decl->getEnumConsts()->at(0)->getName(), "UVM_CORE_UNINITIALIZED");
  EXPECT_EQ(decl->getEnumConsts()->at(1)->getName(), "UVM_CORE_PRE_INIT");
}

// ---------------------------------------------------------------------------
// uvm_core_state m_uvm_core_state = UVM_CORE_UNINITIALIZED;
// ---------------------------------------------------------------------------

TEST_F(PackageDpiTest, MUvmCoreStateIsTypedByUvmCoreState) {
  const hldb::Variable *const v = getPkgVar("m_uvm_core_state");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getName(), "uvm_core_state");
  ASSERT_NE(getTypedef(), nullptr);
  const hldb::Typespec *const actual = rts->getActual();
  EXPECT_TRUE((actual == getTypedef()) || (actual == getEnum()))
      << "'m_uvm_core_state' should resolve to the package's 'uvm_core_state' type";
}

TEST_F(PackageDpiTest, MUvmCoreStateInitializerIsFirstEnumName) {
  const hldb::Variable *const v = getPkgVar("m_uvm_core_state");
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getValue(), nullptr) << "the declaration has an initializer";
  const hldb::Enum *const decl = getEnumDecl();
  ASSERT_NE(decl, nullptr);
  ASSERT_NE(decl->getEnumConsts(), nullptr);
  ASSERT_EQ(decl->getEnumConsts()->size(), 2u);
  if (const hldb::RefObj *const ref = v->getValue<hldb::RefObj>()) {
    EXPECT_EQ(ref->getName(), "UVM_CORE_UNINITIALIZED");
    EXPECT_EQ(ref->getActual<hldb::EnumConst>(), decl->getEnumConsts()->at(0))
        << "the initializer must bind to the first name of uvm_core_state";
  } else {
    const hldb::Constant *const folded = v->getValue<hldb::Constant>();
    ASSERT_NE(folded, nullptr) << "initializer is neither a reference to the enum name nor a folded Constant";
    EXPECT_EQ(folded->getDecompile(), "0") << "6.19: the first enum name defaults to 0";
  }
}

// ---------------------------------------------------------------------------
// class toto; const static int DO_NOT_CATCH = 1; endclass
// ---------------------------------------------------------------------------

TEST_F(PackageDpiTest, PackageHasOneUserDefinedClassToto) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getClassDefns(), nullptr);
  EXPECT_EQ(pkg->getClassDefns()->size(), 1u);
  const hldb::ClassDefn *const c = getToto();
  ASSERT_NE(c, nullptr) << "ClassDefn 'toto' not found in the package";
  EXPECT_EQ(c->getName(), "toto");
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(PackageDpiTest, TotoHasOneConstStaticPropertyDoNotCatch) {
  const hldb::ClassDefn *const c = getToto();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const v = c->getVariables()->at(0);
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getName(), "DO_NOT_CATCH");
  EXPECT_TRUE(v->getConstantVariable()) << "8.19: declared 'const'";
  EXPECT_FALSE(v->getAutomatic()) << "8.9: declared 'static', so one copy is shared by all objects";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::IntTypespec *const ts = v->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "declared 'int'";
  EXPECT_TRUE(ts->getSigned());
  const hldb::Constant *const init = v->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "1");
}

TEST_F(PackageDpiTest, TotoHasNoMethods) {
  const hldb::ClassDefn *const c = getToto();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getMethods() == nullptr || c->getMethods()->empty());
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageDpiTest, UndeclaredTypeUvmObjectWrapperIsReported) {
  EXPECT_NE(findError(ErrorDefinition::COMP_UNDEFINED_TYPE, "uvm_object_wrapper"), nullptr)
      << "'uvm_object_wrapper' is declared nowhere in the design, so it cannot be used as a data type";
}

TEST_F(PackageDpiTest, NoSyntaxOrFatalErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "every declaration in the file is syntactically well formed";
}

TEST_F(PackageDpiTest, DeclaredNamesAreNotReported) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_TYPE, "uvm_core_state"), nullptr)
      << "'uvm_core_state' is declared by the typedef";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "UVM_CORE_UNINITIALIZED"), nullptr)
      << "'UVM_CORE_UNINITIALIZED' is a name of uvm_core_state";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
