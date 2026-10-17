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

// Validates the HLDB model built for tests/PackageValue/top.sv:
//
//   package prim_pkg;
//     typedef enum integer {
//       ImplGeneric = 1,
//       ImplXilinx  = 0
//     } impl_e;
//   endpackage
//
//   module prim_diff_decode ();
//     parameter prim_pkg::impl_e Impl = prim_pkg::ImplGeneric;
//       import prim_pkg::*;
//       if (Impl == ImplGeneric) begin : gen_pad_generic
//       always_comb begin : p_diff_fsm
//         unique case (state_q)
//           IsSkewed: begin
//               state_d = IsStd;
//           end
//           default : ;
//         endcase
//       end
//     end
//   endmodule // prim_diff_decode
//
//   package riscv;
//       localparam logic [63:0] SSTATUS_UIE  = 64'h00000001;
//       ... eleven more 'localparam logic [63:0]' masks, through
//       localparam logic [63:0] SSTATUS32_SD = 64'h80000000;
//   endpackage
//
//   package ariane_pkg;
//      localparam logic [63:0] SMODE_STATUS_READ_MASK = riscv::SSTATUS_UIE
//                                                      | riscv::SSTATUS_SIE
//                                                      ... ten more terms
//                                                      | riscv::SSTATUS64_SD;
//   endpackage
//
// The point of the fixture is values taken from packages: a module parameter
// whose type and default are package-scoped (IEEE 1800-2023 26.3) and which
// controls a generate block through a wildcard-imported enum name, and a
// package parameter computed from twelve package-scoped parameters of another
// package. The always_comb body is a fragment lifted from a larger design:
// state_q, IsSkewed, state_d and IsStd are declared nowhere in the file.
// The regression this file exists to catch is HLC losing the package-scoped
// bindings, mis-evaluating the values that depend on them, or missing the
// errors for the undeclared names.
//
// What is checked, and why:
//   Package prim_pkg
//     - exactly 1 Typedef, 'impl_e', whose alias is an EnumTypespec (6.19)
//       with base type 'integer' (an IntegerTypespec) and exactly 2 names in
//       source order: ImplGeneric with the value Constant "1", and ImplXilinx
//       with the value Constant "0"
//   Module prim_diff_decode (23.2)
//     - '()' declares one null port (37.14 detail 10, "module M();"): no
//       name (detail 8), port index 0 (detail 9), and no low connection
//     - exactly 1 parameter, Impl. The module has no parameter port list, so
//       Impl is not a local parameter (6.20.1)
//     - Impl's type is the package-scoped name prim_pkg::impl_e: a
//       RefTypespec path whose prefix is bound to prim_pkg and whose last
//       element resolves to prim_pkg's impl_e
//     - Impl's default is the package-scoped name prim_pkg::ImplGeneric: a
//       RefObj path whose prefix is bound to prim_pkg and whose last element
//       is bound to the EnumConst ImplGeneric
//     - exactly 1 generate construct, an if-generate (27.5) whose condition
//       is Operation vpiEqOp over RefObj 'Impl', bound to the module's
//       Parameter, and RefObj 'ImplGeneric', bound through the wildcard
//       import to prim_pkg's EnumConst. The import precedes the reference,
//       so the name is potentially locally visible there (26.3)
//     - its generate block is named "gen_pad_generic"; it holds exactly 1
//       always_comb (9.2.2.2) whose statement is the Begin named
//       "p_diff_fsm" (9.3.4) holding exactly 1 statement
//   unique case (state_q) ... endcase (12.5, 12.5.3)
//     - a case statement with the unique qualifier (vpiUniqueQualifier)
//       whose case expression is RefObj 'state_q'
//     - exactly 2 case items: the first has the single expression RefObj
//       'IsSkewed' and a begin-end holding the blocking Assignment
//       'state_d = IsStd'; the second is the default item, which has no
//       expression and the null statement ';'
//     - state_q, IsSkewed, state_d and IsStd are declared nowhere, and a
//       procedural assignment never creates an implicit net (6.10), so none
//       of the four references binds
//   Package riscv
//     - exactly 12 parameters, each a local parameter of type logic [63:0]
//       (a LogicTypespec with exactly 1 packed range [63:0]) whose
//       ParamAssign RHS is the literal written in the source
//   Package ariane_pkg
//     - exactly 1 parameter, SMODE_STATUS_READ_MASK, a local logic [63:0]
//     - unreduced, its value is a bitwise OR (11.4.8) whose 12 terms, in
//       source order, are package-scoped RefObj paths into riscv: SSTATUS_UIE,
//       SIE, SPIE, SPP, FS, XS, SUM, MXR, UPIE, SPIE (again), UXL and
//       SSTATUS64_SD, each with its prefix bound to package riscv and its
//       last element bound to riscv's Parameter of that name
//     - a parameter value is a constant expression (6.20.2), so on an
//       elaborated design it is reduced to the OR of the twelve masks,
//       64'h80000003000DE133
//   Elaboration (23.3.1, 27.5)
//     - prim_diff_decode appears in no instantiation, so on an elaborated
//       design it is the only top-level instance. Impl takes its default
//       ImplGeneric, so the condition holds and the instance holds exactly 1
//       generate scope named "gen_pad_generic"
//   Diagnostics
//     - state_q, IsSkewed, state_d and IsStd are declared nowhere. For a
//       reference other than a subroutine call "it shall be illegal if no
//       identifier can be found that matches the reference" (26.3), so each
//       is reported at error severity, as COMP_UNDEFINED_VARIABLE,
//       ELAB_UNDEF_VARIABLE or COMP_FAILED_TO_BIND
//     - the package-scoped names are not reported: no COMP_FAILED_TO_BIND
//       for ImplGeneric, impl_e or SSTATUS_UIE
//     - there is no syntax error: zero syntax and zero fatal diagnostics
//
// Reduction and elaboration: SMODE_STATUS_READ_MASK and the generate
// condition are evaluated during elaboration. The unreduced forms are
// asserted on the definitions in Design::getAllPackages() and
// Design::getAllModules(), which hold the source form in every run; the
// reduced value and the instantiated generate block are asserted only when
// the design is elaborated, on Design::getTopPackages() and
// Design::getTopModules().
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test:
// HLC models '()' with no port at all, although 37.14
// detail 10 names "module M();" as declaring a null port.
// ModuleHasOneNullPort is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (undeclared identifier not reported as an error), not a defect in this test:
// the references are left
// unbound, as they should be, but HLC reports no error for them; 26.3 makes
// each such reference illegal.
// UndeclaredNamesAreReportedAsErrors is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Whether the package element of a scoped type path is a RefObj or a
//     RefTypespec wrapping one is a model convention; either is accepted.
//   - Which case item executes, and the values state_d takes, only exist
//     while simulation runs (and the names are undeclared anyway).
//     Permanently out of scope; the static shape of the case statement is
//     covered by the UniqueCase* tests.
//   - The node kind of the generate block in the module definition. HLC
//     models it as a Begin holding the block's items; the test reaches it
//     through the if-generate and asserts only its name and contents.
//   - Whether HLC stores the default item's null statement as a NullStmt or
//     leaves the statement empty is a tool convention; either is accepted.
//   - How HLC nests the eleven '|' operations (left-associative pairs or a
//     flattened list) is a tool convention; the terms are collected in
//     order and asserted.
//   - How a path's last element refers to impl_e: it may resolve to the
//     TypedefTypespec or to the EnumTypespec impl_e aliases. Both are that
//     type (6.18), so either is accepted.
//   - HLC represents a package-scoped name as a RefObj or RefTypespec path
//     (the package, then the named item). That shape is a model convention;
//     the test follows it and asserts both the package and the item binding.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention, so the package count is
//     not asserted; packages are looked up by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The comments in the fixture are not design objects (5.4).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/integer_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/null_stmt.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

// The twelve terms of SMODE_STATUS_READ_MASK, in source order.
constexpr std::string_view kMaskTerms[] = {"SSTATUS_UIE",  "SSTATUS_SIE",  "SSTATUS_SPIE", "SSTATUS_SPP",
                                           "SSTATUS_FS",   "SSTATUS_XS",   "SSTATUS_SUM",  "SSTATUS_MXR",
                                           "SSTATUS_UPIE", "SSTATUS_SPIE", "SSTATUS_UXL",  "SSTATUS64_SD"};

class PackageValueTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageValue.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The reference to the package in the first element of a package-scoped
  // type path. HLC records it either as a RefObj or as a RefTypespec whose own
  // path holds that RefObj; which one is a model convention.
  static const hldb::RefObj *getScopePackageRef(const hldb::Any *elem) {
    if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(elem)) return ref;
    const hldb::RefTypespec *const rts = any_cast<hldb::RefTypespec>(elem);
    if (rts == nullptr || rts->getPathElems() == nullptr || rts->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::RefObj>(rts->getPathElems()->at(0));
  }

  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Parameter *getPkgParam(std::string_view pkgName, std::string_view name) {
    const hldb::Package *const pkg = getPkg(pkgName);
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
  }

  // The ParamAssign in 'pkg' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Instance *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Typedef *getImplE() {
    const hldb::Package *const pkg = getPkg("prim_pkg");
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("impl_e", pkg->getTypedefs());
  }

  static const hldb::Enum *getImplEnum() {
    const hldb::Typedef *const td = getImplE();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (et == nullptr) return nullptr;
    return et->getEnum();
  }

  static const hldb::EnumConst *getImplGeneric() {
    const hldb::Enum *const e = getImplEnum();
    if (e == nullptr) return nullptr;
    return hldb::findByName<hldb::EnumConst>("ImplGeneric", e->getEnumConsts());
  }

  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("prim_diff_decode", m_design->getAllModules());
  }

  static const hldb::Parameter *getImpl() {
    const hldb::Module *const m = getModule();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("Impl", m->getParameters());
  }

  static const hldb::GenIf *getGenIf() {
    const hldb::Module *const m = getModule();
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(stmt)) return genIf;
    }
    return nullptr;
  }

  // The generate block 'begin : gen_pad_generic ... end'. In the module
  // definition HLC models a generate block as a Begin holding its items.
  static const hldb::Begin *getGenBlock() {
    const hldb::GenIf *const genIf = getGenIf();
    if (genIf == nullptr) return nullptr;
    return any_cast<hldb::Begin>(genIf->getStmt());
  }

  static std::vector<const hldb::Always *> getGenAlways() {
    std::vector<const hldb::Always *> found;
    const hldb::Begin *const block = getGenBlock();
    if (block == nullptr || block->getStmts() == nullptr) return found;
    for (const hldb::Any *const item : *block->getStmts()) {
      if (const hldb::Always *const always = any_cast<hldb::Always>(item)) found.emplace_back(always);
    }
    return found;
  }

  static const hldb::CaseStmt *getCase() {
    const std::vector<const hldb::Always *> always = getGenAlways();
    if (always.empty()) return nullptr;
    const hldb::Begin *const body = always.front()->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::CaseStmt>(body->getStmts()->at(0));
  }

  // Collects, in order, the terms of a tree of vpiBitOrOp operations.
  static void CollectOrTerms(const hldb::Any *expr, std::vector<const hldb::Any *> *terms) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
    if ((op != nullptr) && (op->getOpType() == vpiBitOrOp) && (op->getOperands() != nullptr)) {
      for (const hldb::Any *const operand : *op->getOperands()) CollectOrTerms(operand, terms);
      return;
    }
    terms->emplace_back(expr);
  }

  // Verifies 'expr' is a package-scoped RefObj path '<pkg>::<name>' whose
  // prefix is bound to 'pkg' and whose last element is bound to 'target'.
  static void ExpectScopedRef(const hldb::Any *expr, const hldb::Package *pkg, std::string_view name,
                              const hldb::Any *target) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'" << name << "' should be a package-scoped RefObj path";
    ASSERT_NE(path->getPathElems(), nullptr) << "'" << name << "'";
    ASSERT_EQ(path->getPathElems()->size(), 2u) << "'" << name << "': the package, then the item";
    const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(scope, nullptr);
    ASSERT_NE(pkg, nullptr);
    EXPECT_EQ(scope->getName(), pkg->getName());
    EXPECT_EQ(scope->getActual<hldb::Package>(), pkg) << "26.3: the scope prefix names package " << pkg->getName();
    const hldb::RefObj *const item = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->getName(), name);
    ASSERT_NE(target, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(item->getActual(), target) << "26.3: '" << name << "' must bind to its declaration in " << pkg->getName();
  }

  // Verifies 'expr' is a RefObj named 'name' that binds to nothing.
  static void ExpectUnboundRef(const hldb::Any *expr, std::string_view name) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    EXPECT_EQ(ref->getActual(), nullptr) << "'" << name << "' is declared nowhere, so it cannot bind";
  }

  // Verifies 'type' is a LogicTypespec with the single packed range [63:0].
  static void ExpectLogic63To0(const hldb::RefTypespec *type, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::LogicTypespec *const lt = type->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << what << " is declared 'logic [63:0]'";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u) << what;
    const hldb::Range *const r = lt->getRanges()->at(0);
    ASSERT_NE(r, nullptr);
    const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->getDecompile(), "63") << what;
    EXPECT_EQ(right->getDecompile(), "0") << what;
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal. The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted.
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if (ch != '_') text.push_back(ch);
    }
    int base = 10;
    const std::string::size_type tick = text.find('\'');
    if (tick != std::string::npos) {
      std::string::size_type pos = tick + 1;
      if ((pos < text.size()) && ((text[pos] == 's') || (text[pos] == 'S'))) ++pos;
      if (pos >= text.size()) return false;
      switch (text[pos]) {
        case 'h':
        case 'H': base = 16; break;
        case 'd':
        case 'D': base = 10; break;
        case 'o':
        case 'O': base = 8; break;
        case 'b':
        case 'B': base = 2; break;
        default: return false;
      }
      text = text.substr(pos + 1);
    }
    if (text.empty()) return false;
    const char *const last = text.data() + text.size();
    const std::from_chars_result res = std::from_chars(text.data(), last, *value, base);
    return (res.ec == std::errc()) && (res.ptr == last);
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
// package prim_pkg; typedef enum integer { ... } impl_e; endpackage
// ---------------------------------------------------------------------------

TEST_F(PackageValueTest, PrimPkgDeclaresEnumTypedefImplE) {
  const hldb::Package *const pkg = getPkg("prim_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'prim_pkg' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'impl_e' is prim_pkg's only typedef";
  const hldb::Enum *const e = getImplEnum();
  ASSERT_NE(e, nullptr) << "6.19: typedef 'impl_e' should alias an enum type";
  ASSERT_NE(e->getBaseTypespec(), nullptr) << "6.19: the explicit base type 'integer' is recorded";
  EXPECT_NE(e->getBaseTypespec()->getActual<hldb::IntegerTypespec>(), nullptr) << "the base type is 'integer'";
}

TEST_F(PackageValueTest, ImplEHasTwoNamesWithExplicitValues) {
  const hldb::Enum *const e = getImplEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  const char *const names[] = {"ImplGeneric", "ImplXilinx"};
  const char *const values[] = {"1", "0"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::EnumConst *const ec = e->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getName(), names[i]) << "enum name " << i << ", in source order";
    const hldb::Constant *const value = ec->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "'" << names[i] << "' is given an explicit literal value";
    EXPECT_EQ(value->getDecompile(), values[i]) << "'" << names[i] << "'";
  }
}

// ---------------------------------------------------------------------------
// module prim_diff_decode (); parameter prim_pkg::impl_e Impl = ...;
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG
// (null port missing) in the file header.
TEST_F(PackageValueTest, ModuleHasOneNullPort) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr) << "module 'prim_diff_decode' not found";
  ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  const hldb::Port *const port = m->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(port->getPortIndex(), 0) << "37.14 detail 9: the first port has index 0";
  EXPECT_EQ(port->getLowConn(), nullptr) << "37.14 detail 10: a null port has no low connection";
}

TEST_F(PackageValueTest, ModuleHasOneOverridableParameterImpl) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "'Impl' is the module's only parameter";
  const hldb::Parameter *const impl = getImpl();
  ASSERT_NE(impl, nullptr) << "parameter 'Impl' not found";
  EXPECT_FALSE(impl->getLocalParam())
      << "6.20.1: the module has no parameter port list, so 'parameter' in its body is not a localparam";
}

TEST_F(PackageValueTest, ImplIsTypedByPrimPkgImplE) {
  const hldb::Parameter *const impl = getImpl();
  ASSERT_NE(impl, nullptr);
  const hldb::RefTypespec *const type = impl->getTypespec();
  ASSERT_NE(type, nullptr) << "'Impl' is declared with the type prim_pkg::impl_e";
  ASSERT_NE(type->getPathElems(), nullptr) << "'prim_pkg::impl_e' should be a package-scoped path";
  ASSERT_EQ(type->getPathElems()->size(), 2u) << "the package, then the type";
  const hldb::RefObj *const pkgRef = getScopePackageRef(type->getPathElems()->at(0));
  ASSERT_NE(pkgRef, nullptr) << "the path starts with the package";
  EXPECT_EQ(pkgRef->getName(), "prim_pkg");
  EXPECT_EQ(pkgRef->getActual<hldb::Package>(), getPkg("prim_pkg")) << "26.3: the scope prefix names prim_pkg";
  const hldb::RefTypespec *const last = any_cast<hldb::RefTypespec>(type->getPathElems()->at(1));
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(last->getName(), "impl_e");
  const hldb::Typedef *const td = getImplE();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::Typespec *const actual = last->getActual();
  ASSERT_NE(actual, nullptr) << "26.3: 'impl_e' is declared in prim_pkg, so the scoped type name must resolve";
  const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
  const bool isImplE =
      ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual());
  EXPECT_TRUE(isImplE) << "26.3: 'Impl' is typed by prim_pkg's impl_e";
}

TEST_F(PackageValueTest, ImplDefaultIsPrimPkgImplGeneric) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule(), "Impl");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'Impl'";
  ExpectScopedRef(pa->getRhs(), getPkg("prim_pkg"), "ImplGeneric", getImplGeneric());
}

// ---------------------------------------------------------------------------
// import prim_pkg::*; if (Impl == ImplGeneric) begin : gen_pad_generic ...
// ---------------------------------------------------------------------------

TEST_F(PackageValueTest, IfGenerateComparesImplWithImportedImplGeneric) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr);
  EXPECT_EQ(m->getGenStmts()->size(), 1u) << "the module body holds one generate construct";
  const hldb::GenIf *const genIf = getGenIf();
  ASSERT_NE(genIf, nullptr) << "27.5: 'if (Impl == ImplGeneric) begin : gen_pad_generic' is an if-generate";
  const hldb::Operation *const eq = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr) << "the condition 'Impl == ImplGeneric' is an Operation";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  const hldb::RefObj *const impl = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(impl, nullptr);
  EXPECT_EQ(impl->getName(), "Impl");
  ASSERT_NE(getImpl(), nullptr);
  EXPECT_EQ(impl->getActual(), getImpl()) << "'Impl' is the module's own parameter";
  const hldb::RefObj *const generic = any_cast<hldb::RefObj>(eq->getOperands()->at(1));
  ASSERT_NE(generic, nullptr);
  EXPECT_EQ(generic->getName(), "ImplGeneric");
  ASSERT_NE(getImplGeneric(), nullptr);
  EXPECT_EQ(generic->getActual(), getImplGeneric())
      << "26.3: 'import prim_pkg::*' precedes the reference, so ImplGeneric binds to prim_pkg's enum name";
}

TEST_F(PackageValueTest, GenerateBlockHoldsAlwaysCombPDiffFsm) {
  const hldb::Begin *const block = getGenBlock();
  ASSERT_NE(block, nullptr) << "the if-generate should hold its generate block";
  EXPECT_EQ(block->getName(), "gen_pad_generic") << "27.5: 'begin : gen_pad_generic' names the generate block";
  const std::vector<const hldb::Always *> always = getGenAlways();
  ASSERT_EQ(always.size(), 1u) << "the block holds one always_comb";
  EXPECT_EQ(always[0]->getAlwaysType(), vpiAlwaysComb) << "9.2.2.2: declared 'always_comb'";
  const hldb::Begin *const body = always[0]->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "the always_comb's statement is a begin-end";
  EXPECT_EQ(body->getName(), "p_diff_fsm") << "9.3.4: 'begin : p_diff_fsm' names the block";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "the block holds the case statement only";
}

// ---------------------------------------------------------------------------
// unique case (state_q) IsSkewed: begin state_d = IsStd; end default : ; endcase
// ---------------------------------------------------------------------------

TEST_F(PackageValueTest, UniqueCaseOnUndeclaredStateQ) {
  const hldb::CaseStmt *const cs = getCase();
  ASSERT_NE(cs, nullptr) << "12.5: the block's statement is a case statement";
  EXPECT_EQ(cs->getQualifier(), vpiUniqueQualifier) << "12.5.3: declared 'unique case'";
  ExpectUnboundRef(cs->getCondition(), "state_q");
}

TEST_F(PackageValueTest, UniqueCaseFirstItemIsSkewedAssignsIsStd) {
  const hldb::CaseStmt *const cs = getCase();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_EQ(cs->getCaseItems()->size(), 2u) << "'IsSkewed:' and 'default:'";
  const hldb::CaseItem *const item = cs->getCaseItems()->at(0);
  ASSERT_NE(item, nullptr);
  ASSERT_NE(item->getExprs(), nullptr);
  ASSERT_EQ(item->getExprs()->size(), 1u);
  ExpectUnboundRef(item->getExprs()->at(0), "IsSkewed");
  const hldb::Begin *const block = item->getStmt<hldb::Begin>();
  ASSERT_NE(block, nullptr) << "the item's statement is a begin-end";
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(block->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' is a blocking assignment";
  ExpectUnboundRef(assign->getLhs(), "state_d");
  ExpectUnboundRef(assign->getRhs(), "IsStd");
}

TEST_F(PackageValueTest, UniqueCaseSecondItemIsDefaultWithNullStatement) {
  const hldb::CaseStmt *const cs = getCase();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_EQ(cs->getCaseItems()->size(), 2u);
  const hldb::CaseItem *const item = cs->getCaseItems()->at(1);
  ASSERT_NE(item, nullptr);
  EXPECT_TRUE(item->getExprs() == nullptr || item->getExprs()->empty()) << "12.5: the default item has no expression";
  EXPECT_TRUE(item->getStmt() == nullptr || any_cast<hldb::NullStmt>(item->getStmt()) != nullptr)
      << "'default : ;' has the null statement";
}

// ---------------------------------------------------------------------------
// package riscv; localparam logic [63:0] SSTATUS_... ; endpackage
// ---------------------------------------------------------------------------

TEST_F(PackageValueTest, RiscvDeclaresTwelveLocalLogic63To0Parameters) {
  const hldb::Package *const pkg = getPkg("riscv");
  ASSERT_NE(pkg, nullptr) << "package 'riscv' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 12u);
  for (std::string_view name :
       {"SSTATUS_UIE", "SSTATUS_SIE", "SSTATUS_SPIE", "SSTATUS_SPP", "SSTATUS_FS", "SSTATUS_XS", "SSTATUS_SUM",
        "SSTATUS_MXR", "SSTATUS_UPIE", "SSTATUS_UXL", "SSTATUS64_SD", "SSTATUS32_SD"}) {
    const hldb::Parameter *const p = getPkgParam("riscv", name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found in riscv";
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: '" << name << "' is declared 'localparam'";
    ExpectLogic63To0(p->getTypespec(), name);
  }
}

TEST_F(PackageValueTest, RiscvParameterValuesAreTheWrittenLiterals) {
  const hldb::Package *const pkg = getPkg("riscv");
  ASSERT_NE(pkg, nullptr);
  struct Literal final {
    std::string_view m_name;
    std::string_view m_text;
  };
  const Literal literals[] = {{"SSTATUS_UIE", "64'h00000001"},          {"SSTATUS_SIE", "64'h00000002"},
                              {"SSTATUS_SPIE", "64'h00000020"},         {"SSTATUS_SPP", "64'h00000100"},
                              {"SSTATUS_FS", "64'h00006000"},           {"SSTATUS_XS", "64'h00018000"},
                              {"SSTATUS_SUM", "64'h00040000"},          {"SSTATUS_MXR", "64'h00080000"},
                              {"SSTATUS_UPIE", "64'h00000010"},         {"SSTATUS_UXL", "64'h0000000300000000"},
                              {"SSTATUS64_SD", "64'h8000000000000000"}, {"SSTATUS32_SD", "64'h80000000"}};
  for (const Literal &lit : literals) {
    const hldb::ParamAssign *const pa = getParamAssign(pkg, lit.m_name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << lit.m_name << "'";
    const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "'" << lit.m_name << "' is assigned a literal";
    EXPECT_EQ(rhs->getDecompile(), lit.m_text) << "'" << lit.m_name << "'";
  }
}

// ---------------------------------------------------------------------------
// package ariane_pkg; localparam logic [63:0] SMODE_STATUS_READ_MASK = ...;
// ---------------------------------------------------------------------------

TEST_F(PackageValueTest, ArianePkgDeclaresLocalMaskParameter) {
  const hldb::Package *const pkg = getPkg("ariane_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'ariane_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const mask = getPkgParam("ariane_pkg", "SMODE_STATUS_READ_MASK");
  ASSERT_NE(mask, nullptr) << "parameter 'SMODE_STATUS_READ_MASK' not found";
  EXPECT_TRUE(mask->getLocalParam()) << "6.20.4: declared 'localparam'";
  ExpectLogic63To0(mask->getTypespec(), "SMODE_STATUS_READ_MASK");
}

TEST_F(PackageValueTest, MaskIsBitwiseOrOfTwelveScopedRiscvParameters) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg("ariane_pkg"), "SMODE_STATUS_READ_MASK");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'SMODE_STATUS_READ_MASK'";
  const hldb::Operation *const top = pa->getRhs<hldb::Operation>();
  ASSERT_NE(top, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(top->getOpType(), vpiBitOrOp) << "11.4.8: the terms are joined with '|'";
  std::vector<const hldb::Any *> terms;
  CollectOrTerms(top, &terms);
  ASSERT_EQ(terms.size(), 12u) << "the source writes twelve terms";
  const hldb::Package *const riscv = getPkg("riscv");
  ASSERT_NE(riscv, nullptr);
  for (size_t i = 0; i < terms.size(); ++i) {
    ExpectScopedRef(terms[i], riscv, kMaskTerms[i], getPkgParam("riscv", kMaskTerms[i]));
  }
}

TEST_F(PackageValueTest, MaskReducesToOrOfTheTwelveMasks) {
  if (m_design->getElaborated()) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>("ariane_pkg", m_design->getTopPackages());
    const hldb::ParamAssign *const pa = getParamAssign(pkg, "SMODE_STATUS_READ_MASK");
    ASSERT_NE(pa, nullptr) << "no ParamAssign for 'SMODE_STATUS_READ_MASK' in the elaborated ariane_pkg";
    const hldb::Constant *const value = pa->getRhs<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "6.20.2: a parameter value is reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(value, &v)) << "unparsable constant '" << value->getDecompile() << "'";
    EXPECT_EQ(v, 0x80000003000DE133ULL) << "11.4.8: the bitwise OR of the twelve riscv masks";
  }
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(PackageValueTest, ElaboratedModuleInstantiatesGenPadGeneric) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: prim_diff_decode appears in no instantiation";
    const hldb::Module *const top = m_design->getTopModules()->at(0);
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->getName(), "prim_diff_decode");
    const hldb::GenScopeArray *const gen =
        hldb::findByName<hldb::GenScopeArray>("gen_pad_generic", top->getGenScopeArrays());
    ASSERT_NE(gen, nullptr) << "27.5: Impl defaults to ImplGeneric, so the if-generate selects its block";
    ASSERT_NE(gen->getGenScopes(), nullptr);
    EXPECT_EQ(gen->getGenScopes()->size(), 1u);
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG
// (undeclared identifier not reported as an error) in the file header.
TEST_F(PackageValueTest, UndeclaredNamesAreReportedAsErrors) {
  for (std::string_view name : {"state_q", "IsSkewed", "state_d", "IsStd"}) {
    ExpectReportedAsError({ErrorDefinition::COMP_UNDEFINED_VARIABLE, ErrorDefinition::ELAB_UNDEF_VARIABLE,
                           ErrorDefinition::COMP_FAILED_TO_BIND},
                          name);
  }
}

TEST_F(PackageValueTest, PackageScopedNamesAreNotReported) {
  for (std::string_view name : {"ImplGeneric", "impl_e", "SSTATUS_UIE"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "26.3: '" << name << "' is declared in a package the file references";
  }
}

TEST_F(PackageValueTest, NoSyntaxOrFatalDiagnostics) {
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
