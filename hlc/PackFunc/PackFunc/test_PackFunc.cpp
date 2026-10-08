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

// Validates the HLDB model built for tests/PackFunc/dut.sv:
//
//   package b;
//     typedef struct packed {
//       logic [10:0]         vc;
//     } foo_bar_t;
//   endpackage
//
//   package aa;
//     function automatic b::foo_bar_t foo_bar;
//       input b::foo_bar_t a;
//       b::foo_bar_t y;
//       reg [$bits(a)-1:0] a_vec;
//       begin
//         foo_bar = y; // return the whole struct
//       end
//     endfunction
//   endpackage
//
// The point of the fixture is a function written in the old style -- no
// parenthesized formal list, the formal declared in the body with 'input'
// (IEEE 1800-2023 13.4, tf_item_declaration) -- whose return type, formal
// and local all use a structure type from another package through the
// package scope resolution operator (26.3), and which returns by assigning
// to the function's own name (13.4.1). The regression this file exists to
// catch is HLC failing to resolve the scoped type in any of the three
// places, or losing the old-style formal.
//
// What is checked, and why:
//   Package b
//     - exactly 1 Typedef, 'foo_bar_t', whose alias is a packed StructTypespec
//       (7.2.1) with exactly 1 member, vc, a LogicTypespec with the packed
//       range [10:0]
//   Package aa
//     - exactly 1 subroutine, the automatic (13.4.2) Function foo_bar
//     - its return type is the package-scoped name b::foo_bar_t: a
//       RefTypespec path whose prefix is bound to package b and whose last
//       element resolves to b's typedef
//     - exactly 1 formal, 'a', declared in the body: an input of type
//       b::foo_bar_t
//     - local variables y, of type b::foo_bar_t, and a_vec, a 'reg' (which
//       is the same type as logic, 6.11.1) with the single packed range
//       [$bits(a)-1:0]: left bound Operation vpiSubOp over the SysFuncCall
//       "$bits" (20.6.2) of RefObj 'a' bound to the formal, and Constant
//       "1"; right bound Constant "0"
//     - the body is a begin-end holding exactly 1 blocking Assignment,
//       'foo_bar = y': the LHS names the function's implicit variable
//       (13.4.1: "The function definition shall implicitly declare a
//       variable, internal to the function, with the same name as the
//       function") and the RHS is bound to the local y
//   Elaboration
//     - $bits(a) is the width of foo_bar_t, 11 bits, and a range bound is a
//       constant expression, so on an elaborated design a_vec's left bound
//       is reduced to 10
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: a_vec's left bound is checked in both modes:
// unreduced on the package definition in Design::getAllPackages(), and
// reduced, only when the design is elaborated, on the elaborated package in
// Design::getTopPackages().
//
// KNOWN COMPILER BUG (old-style formal has no direction), not a defect in
// this test: for the formal declared in the body with 'input b::foo_bar_t a;',
// HLC records an IODecl with no direction (0), although the declaration
// writes 'input' (13.4). A formal in a parenthesized list does get its
// direction. OldStyleFormalAIsAnInput is expected to fail until HLC is
// fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - HLC wraps the function body in a Begin that holds the locals and the
//     written begin-end. How deep the assignment sits is a model convention,
//     so nested blocks are searched.
//   - Whether HLC materializes the implicit variable 'foo_bar' as a Variable
//     or binds the assignment to the Function itself is a tool convention;
//     either is accepted, as long as the name binds within the function.
//   - Which scope owns the locals (the function itself or a Begin wrapping
//     its body) is a tool convention, so both are searched.
//   - Whether the package element of a scoped type path is a RefObj or a
//     RefTypespec wrapping one is a model convention; either is accepted.
//   - How the path's last element refers to foo_bar_t: it may resolve to the
//     TypedefTypespec or to the StructTypespec it aliases. Both are that
//     type (6.18), so either is accepted.
//   - What foo_bar returns only exists while simulation runs.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; packages are looked up
//     by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The '// return the whole struct' comment is not a design object (5.4).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class PackFuncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackFunc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Typedef *getFooBarT() {
    const hldb::Package *const pkg = getPkg("b");
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("foo_bar_t", pkg->getTypedefs());
  }

  static const hldb::Function *getFooBar(const hldb::Package *pkg) {
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("foo_bar", pkg->getTaskFuncs());
  }

  // A local of foo_bar, owned by the function or by a Begin wrapping its
  // body (a tool convention, so both are searched).
  static const hldb::Variable *getLocal(const hldb::Function *fn, std::string_view name) {
    if (fn == nullptr) return nullptr;
    if (const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, fn->getVariables())) return v;
    const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
    if (body == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, body->getVariables());
  }

  // The reference to the package in the first element of a package-scoped
  // type path. HLC records it either as a RefObj or as a RefTypespec whose own
  // path holds that RefObj; which one is a model convention.
  static const hldb::RefObj *getScopePackageRef(const hldb::Any *elem) {
    if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(elem)) return ref;
    const hldb::RefTypespec *const rts = any_cast<hldb::RefTypespec>(elem);
    if (rts == nullptr || rts->getPathElems() == nullptr || rts->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::RefObj>(rts->getPathElems()->at(0));
  }

  // Verifies 'rts' is the package-scoped type name b::foo_bar_t.
  static void ExpectScopedFooBarT(const hldb::RefTypespec *rts, std::string_view what) {
    ASSERT_NE(rts, nullptr) << what << " has no typespec";
    ASSERT_NE(rts->getPathElems(), nullptr) << what << ": 'b::foo_bar_t' should be a package-scoped path";
    ASSERT_EQ(rts->getPathElems()->size(), 2u) << what << ": the package, then the type";
    const hldb::RefObj *const pkgRef = getScopePackageRef(rts->getPathElems()->at(0));
    ASSERT_NE(pkgRef, nullptr) << what << ": the path starts with the package";
    EXPECT_EQ(pkgRef->getName(), "b");
    EXPECT_EQ(pkgRef->getActual<hldb::Package>(), getPkg("b")) << "26.3: the scope prefix names package b";
    const hldb::RefTypespec *const last = any_cast<hldb::RefTypespec>(rts->getPathElems()->at(1));
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->getName(), "foo_bar_t");
    const hldb::Typedef *const td = getFooBarT();
    ASSERT_NE(td, nullptr);
    ASSERT_NE(td->getAlias(), nullptr);
    const hldb::Typespec *const actual = last->getActual();
    ASSERT_NE(actual, nullptr) << what << ": 'foo_bar_t' is declared in b, so the scoped type must resolve";
    const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
    EXPECT_TRUE(((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
                (actual == td->getAlias()->getActual()))
        << what << " is typed by b's foo_bar_t";
  }

  // Collects the Assignments in 'stmt' and in any begin-end nested in it. HLC
  // wraps the function body in a Begin holding the locals and the written
  // begin-end; how deep the statement sits is a model convention.
  static void CollectAssignments(const hldb::Any *stmt, std::vector<const hldb::Assignment *> *assigns) {
    if (const hldb::Assignment *const a = any_cast<hldb::Assignment>(stmt)) {
      assigns->emplace_back(a);
      return;
    }
    const hldb::Begin *const block = any_cast<hldb::Begin>(stmt);
    if (block == nullptr || block->getStmts() == nullptr) return;
    for (const hldb::Any *const inner : *block->getStmts()) CollectAssignments(inner, assigns);
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
};

// ---------------------------------------------------------------------------
// package b; typedef struct packed { logic [10:0] vc; } foo_bar_t; endpackage
// ---------------------------------------------------------------------------

TEST_F(PackFuncTest, FooBarTIsPackedStructWithMemberVc) {
  const hldb::Package *const pkg = getPkg("b");
  ASSERT_NE(pkg, nullptr) << "package 'b' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'foo_bar_t' is b's only typedef";
  const hldb::Typedef *const td = getFooBarT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'foo_bar_t' names a structure type";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 1u);
  const hldb::TypespecMember *const vc = st->getStruct()->getMembers()->at(0);
  ASSERT_NE(vc, nullptr);
  EXPECT_EQ(vc->getName(), "vc");
  ASSERT_NE(vc->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = vc->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'vc' is declared 'logic [10:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "10");
}

// ---------------------------------------------------------------------------
// package aa; function automatic b::foo_bar_t foo_bar; ... endfunction
// ---------------------------------------------------------------------------

TEST_F(PackFuncTest, AaDeclaresAutomaticFunctionFooBar) {
  const hldb::Package *const pkg = getPkg("aa");
  ASSERT_NE(pkg, nullptr) << "package 'aa' not found";
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncs()->size(), 1u) << "'foo_bar' is aa's only subroutine";
  const hldb::Function *const fn = getFooBar(pkg);
  ASSERT_NE(fn, nullptr) << "function 'foo_bar' not found";
  EXPECT_TRUE(fn->getAutomatic()) << "13.4.2: declared 'function automatic'";
}

TEST_F(PackFuncTest, ReturnTypeIsScopedFooBarT) {
  const hldb::Function *const fn = getFooBar(getPkg("aa"));
  ASSERT_NE(fn, nullptr);
  ExpectScopedFooBarT(fn->getReturn(), "the return type");
}

TEST_F(PackFuncTest, OldStyleFormalAIsOfScopedFooBarT) {
  const hldb::Function *const fn = getFooBar(getPkg("aa"));
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr) << "13.4: 'input b::foo_bar_t a;' in the body declares a formal";
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const a = fn->getIODecls()->at(0);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
  ExpectScopedFooBarT(a->getTypespec(), "the formal 'a'");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (old-style
// formal has no direction) in the file header.
TEST_F(PackFuncTest, OldStyleFormalAIsAnInput) {
  const hldb::Function *const fn = getFooBar(getPkg("aa"));
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  ASSERT_NE(fn->getIODecls()->at(0), nullptr);
  EXPECT_EQ(fn->getIODecls()->at(0)->getDirection(), vpiInput)
      << "13.4: 'input b::foo_bar_t a;' declares the formal with direction input";
}

TEST_F(PackFuncTest, LocalYIsScopedFooBarT) {
  const hldb::Variable *const y = getLocal(getFooBar(getPkg("aa")), "y");
  ASSERT_NE(y, nullptr) << "local variable 'y' not found";
  ExpectScopedFooBarT(y->getTypespec(), "'y'");
}

TEST_F(PackFuncTest, LocalAVecRangeIsBitsOfAMinusOne) {
  const hldb::Function *const fn = getFooBar(getPkg("aa"));
  ASSERT_NE(fn, nullptr);
  const hldb::Variable *const aVec = getLocal(fn, "a_vec");
  ASSERT_NE(aVec, nullptr) << "local variable 'a_vec' not found";
  ASSERT_NE(aVec->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = aVec->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "6.11.1: 'reg' is the same type as 'logic'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Operation *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "unreduced, the left bound '$bits(a)-1' is an expression";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::SysFuncCall *const bits = any_cast<hldb::SysFuncCall>(left->getOperands()->at(0));
  ASSERT_NE(bits, nullptr);
  EXPECT_EQ(bits->getName(), "$bits");
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);
  const hldb::RefObj *const a = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_FALSE(fn->getIODecls()->empty());
  EXPECT_EQ(a->getActual(), fn->getIODecls()->at(0)) << "'a' is the function's formal";
  const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(PackFuncTest, BodyAssignsYToImplicitVariableFooBar) {
  const hldb::Function *const fn = getFooBar(getPkg("aa"));
  ASSERT_NE(fn, nullptr);
  std::vector<const hldb::Assignment *> assigns;
  CollectAssignments(fn->getStmt(), &assigns);
  ASSERT_EQ(assigns.size(), 1u) << "the written begin-end holds exactly one assignment, 'foo_bar = y;'";
  const hldb::Assignment *const assign = assigns[0];
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' is a blocking assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "foo_bar");
  ASSERT_NE(lhs->getActual(), nullptr) << "13.4.1: 'foo_bar' names the function's implicit return variable";
  const hldb::Variable *const implicitVar = any_cast<hldb::Variable>(lhs->getActual());
  EXPECT_TRUE((lhs->getActual() == fn) || ((implicitVar != nullptr) && (implicitVar->getName() == "foo_bar")))
      << "13.4.1: 'foo_bar' must bind to the function or its implicit variable";
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "y");
  EXPECT_EQ(rhs->getActual(), getLocal(fn, "y")) << "'y' is the function's local";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackFuncTest, AVecLeftBoundReducesToTen) {
  if (m_design->getElaborated()) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>("aa", m_design->getTopPackages());
    const hldb::Variable *const aVec = getLocal(getFooBar(pkg), "a_vec");
    ASSERT_NE(aVec, nullptr) << "'a_vec' not found in the elaborated package aa";
    ASSERT_NE(aVec->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = aVec->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << "a range bound is a constant expression, reduced on an elaborated design";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(left, &v)) << "unparsable constant '" << left->getDecompile() << "'";
    EXPECT_EQ(v, 10u) << "20.6.2: $bits(foo_bar_t) is 11, so $bits(a)-1 is 10";
  }
}

TEST_F(PackFuncTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
