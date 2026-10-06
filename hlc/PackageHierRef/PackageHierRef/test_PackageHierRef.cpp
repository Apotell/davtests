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

// Validates the HLDB model built for the two source files of
// tests/PackageHierRef.
//
// packages.sv declares four packages p, q, r and s with the same shape:
//
//   package p;
//   parameter package_param = 1;    // q, r and s: 'package_para' instead
//   struct {
//     int x;
//   } s1;
//   struct {
//   int x;
//   } s2;
//   function
//   void f();
//   int x;
//   endfunction
//   endpackage
//
// top.sv starts with a block comment quoting IEEE 1800-2023 23.7, then
// declares:
//
//   module m;
//   import p::*;
//   import bug::*;
//   if (package_param)
//     begin: sB1
//       initial
//       begin: bblock
//         s1.x = 1;   // dotted name 1
//         s2.x = 1;   // dotted name 2
//         f.x = 1;    // dotted name 3
//         f2.x = 1;   // dotted name 4
//       end
//       int x;
//       some_module sInst();
//     end
//   endmodule
//
// The point of the fixture is the rule of 23.7 for dotted names: the first
// name is resolved as a simple identifier, and what it resolves to decides
// whether the name is a member select or a hierarchical name. The module is
// adapted from the example in 23.7, with two differences that change the
// outcome: the generate block is named sB1 (not s1), and the instance is
// named sInst (not s2). So no scope named s1 or s2 is visible, and:
//   - dotted name 1: s1 resolves, through 'import p::*', to the data object
//     p::s1, so rule a) applies and s1.x selects member x of p::s1
//   - dotted name 2: likewise s2.x selects member x of p::s2 (rule a)
//   - dotted name 3: f resolves to the imported scope name p::f, so rule c)
//     applies and f.x is the hierarchical name p::f.x -- the local variable
//     x of f. Package functions default to static (13.4.2), so x is static
//     and can be referenced hierarchically (23.6)
//   - dotted name 4: f2 is declared nowhere, so rule d) makes f2.x a
//     hierarchical name, and no scope f2 exists for it to name
// Packages q, r and s declare the same names as p but are not imported. The
// regression this file exists to catch is HLC binding a dotted name to the
// wrong declaration: to a same-named object in a package that is not
// imported, to the generate block's own 'x', or to anything at all for f2.x.
//
// The fixture also imports 'bug', a package that is declared nowhere, and
// instantiates 'some_module', a module that is declared nowhere.
//
// What is checked, and why:
//   Packages p, q, r and s (26.2)
//     - all four exist as distinct packages
//     - each declares exactly 1 parameter -- 'package_param' in p,
//       'package_para' in q, r and s -- whose ParamAssign RHS is Constant
//       "1"; each is a local parameter (6.20.4: in a package 'parameter' is
//       a synonym for 'localparam')
//     - each declares exactly 2 variables, s1 and s2, each of an anonymous
//       unpacked structure type (7.2) with exactly 1 member, 'x', a signed
//       int (6.11)
//     - each declares exactly 1 subroutine, the Function f: a void function
//       (13.4.1) with no formals, static by default in a package (13.4.2),
//       declaring exactly 1 local variable 'x', a signed int, which is
//       static like the function
//   Module m (23.2)
//     - exactly 1 generate construct, an if-generate (27.5) whose condition
//       is RefObj 'package_param' bound to p's Parameter. Only p declares
//       that name, and only p is imported
//     - its generate block is named "sB1" and declares the variable 'x', a
//       signed int
//     - the block holds exactly 1 process, an Initial whose statement is
//       the Begin named "bblock" (9.3.4) with exactly 4 statements, each a
//       blocking Assignment (10.4.1) of the Constant "1"
//   The four dotted names, each a RefObj path of 2 elements
//     - 1: 's1' bound to p::s1, 'x' bound to the member x of p::s1's struct
//     - 2: 's2' bound to p::s2, 'x' bound to the member x of p::s2's struct
//     - 3: 'f' bound to p::f, 'x' bound to f's local variable x
//     - 4: neither 'f2' nor 'x' binds to anything; in particular 'x' must
//       not bind to sB1's 'x' or to any package's 'x'
//   Elaboration (27.5)
//     - package_param is 1, so the if-generate selects its block: on an
//       elaborated design the instance of m holds exactly 1 generate scope
//       named "sB1"
//   Diagnostics
//     - 'bug' is never compiled, but "The compilation of a package shall
//       precede the compilation of scopes in which the package is imported"
//       (26.3), so the import is reported at error severity, as
//       COMP_UNDEFINED_PACKAGE or ELAB_UNDEFINED_PACKAGE
//     - the imported names are not reported: no COMP_FAILED_TO_BIND or
//       COMP_UNDEFINED_VARIABLE for package_param, s1, s2 or f
//     - there is no syntax error: zero syntax and zero fatal diagnostics
//
// Reduction and elaboration: the generate condition is the only expression
// that must be evaluated during elaboration (27.5), and it is checked
// through which block is instantiated, gated on getElaborated(). Every other
// check is name resolution or declaration structure on the module and
// package definitions, which hold the source form in every run.
//
// KNOWN COMPILER BUG (package parameter is not a localparam), not a defect
// in this test: 6.20.4 says that in a package "the parameter keyword shall
// be a synonym for the localparam keyword", but HLC reports
// Parameter::getLocalParam() false for the parameter of every package.
// EachPackageParameterIsLocalParam asserts the LRM value and is expected to
// fail until HLC is fixed; it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (component of an unresolved hierarchical name bound),
// not a defect in this test: HLC leaves 'f2' in 'f2.x' unbound, but binds
// the second component 'x' to sB1's own variable x. Under 23.7 rule d) f2.x
// is a hierarchical name, so x names an item inside the scope f2, which does
// not exist; it is not a simple reference to the x declared in sB1.
// DottedName4F2XBindsToNothing is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (import of an undeclared package not reported), not a
// defect in this test: HLC records 'import bug::*;' but reports nothing for
// it, although package bug is never compiled, which 26.3 requires before
// the import. ImportFromUndeclaredPackageBugIsReportedAsError is expected to
// fail until HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Whether HLC reports a diagnostic for f2.x, and how. The standard makes
//     f2.x a hierarchical name (23.7 rule d) and gives the search for it
//     (23.8) but names no specific diagnostic, so only the fact the source
//     fixes -- that nothing named f2 exists to bind to -- is asserted.
//   - The instance 'some_module sInst();'. some_module is declared nowhere,
//     and the standard does not say how an instance of an undefined module
//     is reported or represented, so neither is asserted.
//   - The values the four assignments store only exist while simulation
//     runs. Permanently out of scope; the static half, which declaration
//     each assignment writes, is covered by the DottedName* tests.
//   - The order in which the .hlc lists packages.sv and top.sv. 26.3
//     requires p to be compiled before m imports it; this test asserts the
//     resulting bindings, not the file order.
//   - How a void function records its return type (no RefTypespec, or one
//     resolving to a VoidTypespec) is a tool convention; either is
//     accepted.
//   - Which scope owns f's local 'x' (the function or a Begin wrapping its
//     body) is a tool convention, so both are searched.
//   - The node kind of the generate block in the module definition. HLC
//     models it as a Begin holding the block's items; the test reaches it
//     through the if-generate and asserts only its name and contents.
//   - Where HLC records the two imports of m is a model convention. The
//     effect of 'import p::*' is asserted through the bindings above.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention, so the package count is
//     not asserted; packages are looked up by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The block comment at the top of top.sv is a comment, not a design
//     object.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/void_typespec.h>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace hlc {

// The four packages of packages.sv, and the parameter name each declares.
struct PackageInfo final {
  std::string_view m_name;
  std::string_view m_paramName;
};

constexpr PackageInfo kPackages[] = {
    {"p", "package_param"}, {"q", "package_para"}, {"r", "package_para"}, {"s", "package_para"}};

class PackageHierRefTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageHierRef.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Parameter *getParam(std::string_view pkgName, std::string_view name) {
    const hldb::Package *const pkg = getPkg(pkgName);
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
  }

  static const hldb::Variable *getPkgVar(std::string_view pkgName, std::string_view name) {
    const hldb::Package *const pkg = getPkg(pkgName);
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, pkg->getVariables());
  }

  // The member 'x' of the anonymous structure type of 'var'.
  static const hldb::TypespecMember *getStructMemberX(const hldb::Variable *var) {
    if (var == nullptr || var->getTypespec() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = var->getTypespec()->getActual<hldb::StructTypespec>();
    if (st == nullptr || st->getStruct() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypespecMember>("x", st->getStruct()->getMembers());
  }

  static const hldb::Function *getF(std::string_view pkgName) {
    const hldb::Package *const pkg = getPkg(pkgName);
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("f", pkg->getTaskFuncs());
  }

  // f's local 'x'. Which scope owns it -- the function itself or a Begin
  // wrapping its body -- is a tool convention, so both are searched.
  static const hldb::Variable *getFLocalX(std::string_view pkgName) {
    const hldb::Function *const fn = getF(pkgName);
    if (fn == nullptr) return nullptr;
    if (const hldb::Variable *const v = hldb::findByName<hldb::Variable>("x", fn->getVariables())) return v;
    const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
    if (body == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("x", body->getVariables());
  }

  static const hldb::Module *getM() { return hldb::findByName<hldb::Module>("m", m_design->getAllModules()); }

  static const hldb::GenIf *getGenIf() {
    const hldb::Module *const m = getM();
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(stmt)) return genIf;
    }
    return nullptr;
  }

  // The generate block 'begin: sB1 ... end' of the if-generate. In the
  // module definition HLC models a generate block as a Begin holding the
  // block's items.
  static const hldb::Begin *getSB1() {
    const hldb::GenIf *const genIf = getGenIf();
    if (genIf == nullptr) return nullptr;
    return any_cast<hldb::Begin>(genIf->getStmt());
  }

  // The initial procedures among sB1's items.
  static std::vector<const hldb::Initial *> getSB1Initials() {
    std::vector<const hldb::Initial *> initials;
    const hldb::Begin *const sB1 = getSB1();
    if (sB1 == nullptr || sB1->getStmts() == nullptr) return initials;
    for (const hldb::Any *const item : *sB1->getStmts()) {
      if (const hldb::Initial *const init = any_cast<hldb::Initial>(item)) initials.emplace_back(init);
    }
    return initials;
  }

  // The named block 'begin: bblock ... end' of sB1's initial procedure.
  static const hldb::Begin *getBblock() {
    const std::vector<const hldb::Initial *> initials = getSB1Initials();
    if (initials.empty()) return nullptr;
    return initials.front()->getStmt<hldb::Begin>();
  }

  // The LHS of the assignment holding dotted name 'number' (1 to 4).
  static const hldb::Any *getDottedName(size_t number) {
    const hldb::Begin *const block = getBblock();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() < number) return nullptr;
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(block->getStmts()->at(number - 1));
    if (assign == nullptr) return nullptr;
    return assign->getLhs();
  }

  // Verifies 'expr' is the dotted name '<head>.<member>': a RefObj path of
  // exactly 2 elements, the first bound by object identity to 'headDecl' and
  // the second named 'member' and bound to 'memberDecl'.
  static void ExpectDottedName(const hldb::Any *expr, std::string_view head, const hldb::Any *headDecl,
                               std::string_view member, const hldb::Any *memberDecl, std::string_view rule) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'" << head << "." << member << "' should be a RefObj path";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u) << "'" << head << "." << member << "' has two name components";
    const hldb::RefObj *const first = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->getName(), head);
    ASSERT_NE(headDecl, nullptr) << "the declaration '" << head << "' should bind to was not found";
    EXPECT_EQ(first->getActual(), headDecl) << rule;
    const hldb::RefObj *const second = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->getName(), member);
    ASSERT_NE(memberDecl, nullptr) << "the declaration '" << member << "' should bind to was not found";
    EXPECT_EQ(second->getActual(), memberDecl) << rule;
  }

  // Verifies one of 'types' was reported naming 'symbol', at error severity:
  // the standard makes the construct illegal.
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
// packages.sv: package p; package q; package r; package s;
// ---------------------------------------------------------------------------

TEST_F(PackageHierRefTest, FourPackagesExistAndAreDistinct) {
  const hldb::Package *const p = getPkg("p");
  const hldb::Package *const q = getPkg("q");
  const hldb::Package *const r = getPkg("r");
  const hldb::Package *const s = getPkg("s");
  ASSERT_NE(p, nullptr) << "package 'p' not found";
  ASSERT_NE(q, nullptr) << "package 'q' not found";
  ASSERT_NE(r, nullptr) << "package 'r' not found";
  ASSERT_NE(s, nullptr) << "package 's' not found";
  EXPECT_NE(p, q);
  EXPECT_NE(p, r);
  EXPECT_NE(p, s);
  EXPECT_NE(q, r);
  EXPECT_NE(q, s);
  EXPECT_NE(r, s);
}

TEST_F(PackageHierRefTest, EachPackageDeclaresOneParameterEqualToOne) {
  for (const PackageInfo &info : kPackages) {
    const hldb::Package *const pkg = getPkg(info.m_name);
    ASSERT_NE(pkg, nullptr) << "package '" << info.m_name << "' not found";
    ASSERT_NE(pkg->getParameters(), nullptr) << info.m_name;
    EXPECT_EQ(pkg->getParameters()->size(), 1u) << info.m_name;
    const hldb::Parameter *const param = getParam(info.m_name, info.m_paramName);
    ASSERT_NE(param, nullptr) << "parameter '" << info.m_paramName << "' not found in " << info.m_name;
    ASSERT_NE(pkg->getParamAssigns(), nullptr) << info.m_name;
    ASSERT_EQ(pkg->getParamAssigns()->size(), 1u) << info.m_name;
    const hldb::ParamAssign *const pa = pkg->getParamAssigns()->at(0);
    ASSERT_NE(pa, nullptr);
    const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << info.m_name;
    EXPECT_EQ(lhs->getActual<hldb::Parameter>(), param) << info.m_name;
    const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << info.m_name;
    EXPECT_EQ(rhs->getDecompile(), "1") << info.m_name;
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (package
// parameter is not a localparam) in the file header.
TEST_F(PackageHierRefTest, EachPackageParameterIsLocalParam) {
  for (const PackageInfo &info : kPackages) {
    const hldb::Parameter *const param = getParam(info.m_name, info.m_paramName);
    ASSERT_NE(param, nullptr) << "parameter '" << info.m_paramName << "' not found in " << info.m_name;
    EXPECT_TRUE(param->getLocalParam()) << "6.20.4: '" << info.m_paramName << "' is declared in package " << info.m_name
                                        << ", where 'parameter' is a synonym for 'localparam'";
  }
}

TEST_F(PackageHierRefTest, EachPackageDeclaresUnpackedStructVariablesS1AndS2) {
  for (const PackageInfo &info : kPackages) {
    const hldb::Package *const pkg = getPkg(info.m_name);
    ASSERT_NE(pkg, nullptr) << "package '" << info.m_name << "' not found";
    ASSERT_NE(pkg->getVariables(), nullptr) << info.m_name;
    EXPECT_EQ(pkg->getVariables()->size(), 2u) << info.m_name << " declares exactly 's1' and 's2'";
    for (std::string_view name : {"s1", "s2"}) {
      const hldb::Variable *const var = getPkgVar(info.m_name, name);
      ASSERT_NE(var, nullptr) << "'" << name << "' not found in " << info.m_name;
      ASSERT_NE(var->getTypespec(), nullptr) << info.m_name << "::" << name;
      const hldb::StructTypespec *const st = var->getTypespec()->getActual<hldb::StructTypespec>();
      ASSERT_NE(st, nullptr) << info.m_name << "::" << name << " is declared with a 'struct' type";
      const hldb::Struct *const decl = st->getStruct();
      ASSERT_NE(decl, nullptr) << info.m_name << "::" << name;
      EXPECT_FALSE(decl->getPacked()) << "7.2: '" << info.m_name << "::" << name << "' is not declared 'packed'";
      ASSERT_NE(decl->getMembers(), nullptr) << info.m_name << "::" << name;
      ASSERT_EQ(decl->getMembers()->size(), 1u) << info.m_name << "::" << name << " has exactly the member 'x'";
      const hldb::TypespecMember *const x = decl->getMembers()->at(0);
      ASSERT_NE(x, nullptr);
      EXPECT_EQ(x->getName(), "x");
      ASSERT_NE(x->getTypespec(), nullptr);
      const hldb::IntTypespec *const it = x->getTypespec()->getActual<hldb::IntTypespec>();
      ASSERT_NE(it, nullptr) << info.m_name << "::" << name << ".x is declared 'int'";
      EXPECT_TRUE(it->getSigned()) << "6.11: 'int' is signed";
    }
  }
}

TEST_F(PackageHierRefTest, EachPackageDeclaresStaticVoidFunctionF) {
  for (const PackageInfo &info : kPackages) {
    const hldb::Package *const pkg = getPkg(info.m_name);
    ASSERT_NE(pkg, nullptr) << "package '" << info.m_name << "' not found";
    ASSERT_NE(pkg->getTaskFuncs(), nullptr) << info.m_name;
    EXPECT_EQ(pkg->getTaskFuncs()->size(), 1u) << info.m_name << " declares exactly the subroutine 'f'";
    const hldb::Function *const fn = getF(info.m_name);
    ASSERT_NE(fn, nullptr) << "13.4: function 'f' not found in " << info.m_name;
    EXPECT_TRUE(fn->getReturn() == nullptr || fn->getReturn()->getActual<hldb::VoidTypespec>() != nullptr)
        << "13.4.1: " << info.m_name << "::f is declared 'void'";
    EXPECT_TRUE(fn->getIODecls() == nullptr || fn->getIODecls()->empty())
        << info.m_name << "::f is declared with an empty argument list";
    EXPECT_FALSE(fn->getAutomatic()) << "13.4.2: a function defined in a package defaults to static";
  }
}

TEST_F(PackageHierRefTest, EachFunctionFDeclaresStaticIntX) {
  for (const PackageInfo &info : kPackages) {
    const hldb::Variable *const x = getFLocalX(info.m_name);
    ASSERT_NE(x, nullptr) << "local variable 'x' not found in " << info.m_name << "::f";
    ASSERT_NE(x->getTypespec(), nullptr);
    const hldb::IntTypespec *const it = x->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << info.m_name << "::f.x is declared 'int'";
    EXPECT_TRUE(it->getSigned()) << "6.11: 'int' is signed";
    EXPECT_FALSE(x->getAutomatic()) << "13.4.2: items declared in a static function are statically allocated";
  }
}

// ---------------------------------------------------------------------------
// top.sv: module m; import p::*; import bug::*; if (package_param) ...
// ---------------------------------------------------------------------------

TEST_F(PackageHierRefTest, ModuleMHasOneIfGenerateOnPackageParam) {
  const hldb::Module *const m = getM();
  ASSERT_NE(m, nullptr) << "module 'm' not found";
  ASSERT_NE(m->getGenStmts(), nullptr);
  EXPECT_EQ(m->getGenStmts()->size(), 1u) << "the module body holds one generate construct";
  const hldb::GenIf *const genIf = getGenIf();
  ASSERT_NE(genIf, nullptr) << "27.5: 'if (package_param) begin: sB1 ... end' is an if-generate with no else";
  const hldb::RefObj *const cond = genIf->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "the condition 'package_param' is a reference";
  EXPECT_EQ(cond->getName(), "package_param");
  ASSERT_NE(getParam("p", "package_param"), nullptr);
  EXPECT_EQ(cond->getActual(), getParam("p", "package_param"))
      << "26.3: 'package_param' is declared only in p, and p is imported";
}

TEST_F(PackageHierRefTest, GenerateBlockSB1DeclaresIntX) {
  const hldb::Begin *const sB1 = getSB1();
  ASSERT_NE(sB1, nullptr) << "the if-generate should hold its generate block";
  EXPECT_EQ(sB1->getName(), "sB1") << "27.5: 'begin: sB1' names the generate block";
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", sB1->getVariables());
  ASSERT_NE(x, nullptr) << "'int x;' is declared in sB1";
  ASSERT_NE(x->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = x->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "sB1.x is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11: 'int' is signed";
}

TEST_F(PackageHierRefTest, GenerateBlockSB1HasOneInitialWithBlockBblock) {
  ASSERT_NE(getSB1(), nullptr);
  EXPECT_EQ(getSB1Initials().size(), 1u) << "9.2.1: sB1 holds exactly one initial procedure";
  const hldb::Begin *const block = getBblock();
  ASSERT_NE(block, nullptr) << "the initial's statement is a begin-end";
  EXPECT_EQ(block->getName(), "bblock") << "9.3.4: 'begin: bblock' names the block";
  ASSERT_NE(block->getStmts(), nullptr);
  EXPECT_EQ(block->getStmts()->size(), 4u) << "the block holds the four dotted-name assignments";
}

TEST_F(PackageHierRefTest, EachStatementBlockingAssignsOne) {
  const hldb::Begin *const block = getBblock();
  ASSERT_NE(block, nullptr);
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 4u);
  for (size_t i = 0; i < block->getStmts()->size(); ++i) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(block->getStmts()->at(i));
    ASSERT_NE(assign, nullptr) << "statement " << i << " is an assignment";
    EXPECT_TRUE(assign->getBlocking()) << "10.4.1: statement " << i << " uses '='";
    const hldb::Constant *const one = assign->getRhs<hldb::Constant>();
    ASSERT_NE(one, nullptr) << "statement " << i << " assigns a literal";
    EXPECT_EQ(one->getDecompile(), "1");
  }
}

// ---------------------------------------------------------------------------
// The four dotted names (23.7)
// ---------------------------------------------------------------------------

TEST_F(PackageHierRefTest, DottedName1IsMemberSelectOfPS1) {
  const hldb::Variable *const s1 = getPkgVar("p", "s1");
  ExpectDottedName(getDottedName(1), "s1", s1, "x", getStructMemberX(s1),
                   "23.7 rule a): s1 resolves through 'import p::*' to the data object p::s1, so s1.x selects "
                   "its member x");
}

TEST_F(PackageHierRefTest, DottedName2IsMemberSelectOfPS2) {
  const hldb::Variable *const s2 = getPkgVar("p", "s2");
  ExpectDottedName(getDottedName(2), "s2", s2, "x", getStructMemberX(s2),
                   "23.7 rule a): no scope named s2 is visible, so s2 resolves to the data object p::s2 and "
                   "s2.x selects its member x");
}

TEST_F(PackageHierRefTest, DottedName3IsHierarchicalNamePFX) {
  ExpectDottedName(getDottedName(3), "f", getF("p"), "x", getFLocalX("p"),
                   "23.7 rule c): f is the imported scope name p::f, so f.x is p::f.x, f's static local x");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (component
// of an unresolved hierarchical name bound) in the file header.
TEST_F(PackageHierRefTest, DottedName4F2XBindsToNothing) {
  const hldb::RefObj *const path = any_cast<hldb::RefObj>(getDottedName(4));
  ASSERT_NE(path, nullptr) << "'f2.x' should be a RefObj";
  EXPECT_EQ(path->getActual(), nullptr) << "23.7 rule d): f2 is declared nowhere, so f2.x names nothing";
  if (path->getPathElems() != nullptr) {
    for (const hldb::Any *const elem : *path->getPathElems()) {
      const hldb::RefObj *const ref = any_cast<hldb::RefObj>(elem);
      ASSERT_NE(ref, nullptr);
      EXPECT_EQ(ref->getActual(), nullptr)
          << "23.7 rule d): '" << ref->getName() << "' in f2.x must not bind -- f2 is declared nowhere";
    }
  }
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(PackageHierRefTest, IfGenerateInstantiatesSB1) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByName<hldb::Module>("m", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "m is the only module, so it is the top-level instance";
    const hldb::GenScopeArray *const sB1 = hldb::findByName<hldb::GenScopeArray>("sB1", top->getGenScopeArrays());
    ASSERT_NE(sB1, nullptr) << "27.5: package_param is 1, so the if-generate instantiates its block sB1";
    ASSERT_NE(sB1->getGenScopes(), nullptr);
    EXPECT_EQ(sB1->getGenScopes()->size(), 1u) << "27.5: a conditional generate selects at most one block";
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (import of
// an undeclared package not reported) in the file header.
TEST_F(PackageHierRefTest, ImportFromUndeclaredPackageBugIsReportedAsError) {
  ExpectReportedAsError({ErrorDefinition::COMP_UNDEFINED_PACKAGE, ErrorDefinition::ELAB_UNDEFINED_PACKAGE}, "bug");
}

TEST_F(PackageHierRefTest, ImportedNamesAreNotReported) {
  for (std::string_view name : {"package_param", "s1", "s2", "f"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "26.3: '" << name << "' is declared in p, and p is imported";
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "26.3: '" << name << "' is declared in p, and p is imported";
  }
}

TEST_F(PackageHierRefTest, NoSyntaxOrFatalDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "both files are syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
