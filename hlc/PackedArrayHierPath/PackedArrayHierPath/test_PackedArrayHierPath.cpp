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

// Validates the HLDB model built for tests/PackedArrayHierPath/dut.sv:
//
//   package uvm;
//   typedef struct packed {
//     int unsigned exp_bits;
//     int unsigned man_bits;
//   } fp_encoding_t;
//   parameter NUM_FP_FORMATS = 4;
//   localparam fp_encoding_t [0:NUM_FP_FORMATS-1] FP_ENCODINGS  = '{
//     '{8,  23}, // IEEE binary32 (single)
//     '{11, 52}, // IEEE binary64 (double)
//     '{5,  10}, // IEEE binary16 (half)
//     '{5,  2},  // custom binary8
//     '{8,  7}   // custom binary16alt
//     // add new formats here
//   };
//   function automatic int unsigned fp_width(fp_format_e fmt);
//   return FP_ENCODINGS[fmt].exp_bits + FP_ENCODINGS[fmt].man_bits + 1;
//   endfunction
//   endpackage
//
// The point of the fixture is a member select applied to an element select
// of a packed array of packed structures (IEEE 1800-2023 7.4.1, 7.2):
// 'FP_ENCODINGS[fmt].exp_bits' must bind the array to the parameter, the
// index to the formal, and the member to the structure's member. The fixture
// is lifted from a larger design and is incomplete: the formal's type
// 'fp_format_e' is declared nowhere, and the pattern lists 5 structures for
// an array of NUM_FP_FORMATS = 4 elements.
//
// What is checked, and why:
//   typedef struct packed { ... } fp_encoding_t; (6.18, 7.2.1)
//     - the package declares exactly 1 Typedef, whose alias is a packed
//       StructTypespec with exactly 2 members, exp_bits then man_bits, each
//       'int unsigned': an IntTypespec that is not signed (6.11.3)
//   parameter NUM_FP_FORMATS = 4;
//     - a local parameter (6.20.4: in a package 'parameter' is a synonym for
//       'localparam') with the ParamAssign RHS Constant "4"
//   localparam fp_encoding_t [0:NUM_FP_FORMATS-1] FP_ENCODINGS = '{...};
//     - a local parameter whose type is a packed array (7.4.1) with the
//       single packed dimension [0:NUM_FP_FORMATS-1] -- left bound Constant
//       "0", right bound Operation vpiSubOp over RefObj 'NUM_FP_FORMATS'
//       (bound to that Parameter) and Constant "1" -- whose element type is
//       fp_encoding_t
//     - unreduced, its value is Operation vpiAssignmentPatternOp (10.9.1)
//       with exactly 5 items, each itself an assignment pattern of 2
//       Constants, in source order (8,23), (11,52), (5,10), (5,2), (8,7)
//   function automatic int unsigned fp_width(fp_format_e fmt); (13.4)
//     - an automatic Function (13.4.2) returning 'int unsigned', with
//       exactly 1 formal, 'fmt', an input whose type 'fp_format_e' is
//       declared nowhere and so resolves to nothing
//     - its body is a single ReturnStmt whose expression adds three terms
//       (11.4.3), in source order: FP_ENCODINGS[fmt].exp_bits,
//       FP_ENCODINGS[fmt].man_bits and the Constant "1"
//     - each select term is a RefObj path of 2 elements: a bit-select whose
//       prefix is bound to the Parameter FP_ENCODINGS and whose index is
//       bound to the formal fmt, then the member (exp_bits or man_bits)
//       bound to that member of fp_encoding_t
//   Diagnostics
//     - 'fp_format_e' is declared nowhere. For a reference other than a
//       subroutine call "it shall be illegal if no identifier can be found
//       that matches the reference" (26.3), so it is reported at error
//       severity, as COMP_UNDEFINED_TYPE or COMP_FAILED_TO_BIND
//     - an array assignment pattern's "expressions shall match element for
//       element" (10.9.1). The pattern has 5 items for an array of 4
//       elements, so an elaborated design, where NUM_FP_FORMATS is known,
//       holds at least 2 errors: this one and the one for fp_format_e
//     - there is no syntax error: zero syntax and zero fatal diagnostics
//
// Reduction and elaboration: the element count of FP_ENCODINGS is only known
// once NUM_FP_FORMATS is evaluated, so the check of the item-count error is
// gated on getElaborated(). FP_ENCODINGS itself is erroneous, so its reduced
// value is not asserted.
//
// KNOWN COMPILER BUG (undeclared type reported as a warning), not a defect
// in this test: HLC leaves the type of 'fmt' unresolved, as it should, but
// reports 'fp_format_e' only as a CP5851 "Failed to bind" warning; 26.3
// makes the reference illegal, so it is an error.
// UndeclaredTypeFpFormatEIsReportedAsError is expected to fail until HLC is
// fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Which diagnostic HLC gives for the item-count mismatch. The standard
//     makes it an error but names no specific diagnostic and no symbol to
//     key it on, so only the error count is checked.
//   - The values fp_width returns only exist while simulation runs, and the
//     function's formal type is undeclared anyway.
//   - How HLC nests the two '+' operations (left-associative pairs or a
//     flattened list) is a tool convention; the terms are collected in order.
//   - How a RefTypespec refers to fp_encoding_t: it may resolve to the
//     TypedefTypespec or to the StructTypespec it aliases. Both are that
//     type (6.18), so either is accepted.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; uvm is looked up by
//     name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The comments in the pattern are not design objects (5.4).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace hlc {

class PackedArrayHierPathTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedArrayHierPath.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("uvm", m_design->getAllPackages()); }

  static const hldb::Parameter *getParam(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
  }

  // The ParamAssign whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr || pkg->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *pkg->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Typedef *getEncodingT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("fp_encoding_t", pkg->getTypedefs());
  }

  static const hldb::TypespecMember *getMember(std::string_view name) {
    const hldb::Typedef *const td = getEncodingT();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr || st->getStruct() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypespecMember>(name, st->getStruct()->getMembers());
  }

  static const hldb::Function *getFpWidth() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("fp_width", pkg->getTaskFuncs());
  }

  static const hldb::IODecl *getFmt() {
    const hldb::Function *const fn = getFpWidth();
    if (fn == nullptr) return nullptr;
    return hldb::findByName<hldb::IODecl>("fmt", fn->getIODecls());
  }

  // Collects, in order, the terms of a tree of vpiAddOp operations.
  static void CollectAddTerms(const hldb::Any *expr, std::vector<const hldb::Any *> *terms) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
    if ((op != nullptr) && (op->getOpType() == vpiAddOp) && (op->getOperands() != nullptr)) {
      for (const hldb::Any *const operand : *op->getOperands()) CollectAddTerms(operand, terms);
      return;
    }
    terms->emplace_back(expr);
  }

  static std::vector<const hldb::Any *> getReturnTerms() {
    std::vector<const hldb::Any *> terms;
    const hldb::Function *const fn = getFpWidth();
    if (fn == nullptr) return terms;
    const hldb::ReturnStmt *const ret = fn->getStmt<hldb::ReturnStmt>();
    if (ret == nullptr) return terms;
    CollectAddTerms(ret->getCondition(), &terms);
    return terms;
  }

  // Verifies 'expr' is 'FP_ENCODINGS[fmt].<member>'.
  static void ExpectEncodingMember(const hldb::Any *expr, std::string_view member) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'FP_ENCODINGS[fmt]." << member << "' should be a RefObj path";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u) << "the element select, then the member";
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(path->getPathElems()->at(0));
    ASSERT_NE(sel, nullptr) << "11.5.1: 'FP_ENCODINGS[fmt]' selects one element";
    const hldb::RefObj *const array = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(array, nullptr) << "the select applies to 'FP_ENCODINGS'";
    EXPECT_EQ(array->getName(), "FP_ENCODINGS");
    ASSERT_NE(getParam("FP_ENCODINGS"), nullptr);
    EXPECT_EQ(array->getActual(), getParam("FP_ENCODINGS")) << "'FP_ENCODINGS' is the package's parameter";
    const hldb::RefObj *const index = sel->getIndex<hldb::RefObj>();
    ASSERT_NE(index, nullptr) << "the index is 'fmt'";
    EXPECT_EQ(index->getName(), "fmt");
    ASSERT_NE(getFmt(), nullptr);
    EXPECT_EQ(index->getActual(), getFmt()) << "'fmt' is fp_width's formal";
    const hldb::RefObj *const m = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getName(), member);
    ASSERT_NE(getMember(member), nullptr);
    EXPECT_EQ(m->getActual(), getMember(member)) << "7.2: '" << member << "' is a member of fp_encoding_t";
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
// typedef struct packed { ... } fp_encoding_t;
// ---------------------------------------------------------------------------

TEST_F(PackedArrayHierPathTest, EncodingTIsPackedStructOfTwoUnsignedInts) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'uvm' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'fp_encoding_t' is the package's only typedef";
  const hldb::Typedef *const td = getEncodingT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'fp_encoding_t' names a structure type";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 2u);
  const char *const names[] = {"exp_bits", "man_bits"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TypespecMember *const m = st->getStruct()->getMembers()->at(i);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getName(), names[i]) << "member " << i << ", in source order";
    ASSERT_NE(m->getTypespec(), nullptr);
    const hldb::IntTypespec *const it = m->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << "'" << names[i] << "' is declared 'int unsigned'";
    EXPECT_FALSE(it->getSigned()) << "6.11.3: 'unsigned' overrides int's default signedness";
  }
}

// ---------------------------------------------------------------------------
// parameter NUM_FP_FORMATS = 4; localparam ... FP_ENCODINGS = '{...};
// ---------------------------------------------------------------------------

TEST_F(PackedArrayHierPathTest, NumFpFormatsIsLocalFour) {
  const hldb::Parameter *const p = getParam("NUM_FP_FORMATS");
  ASSERT_NE(p, nullptr) << "parameter 'NUM_FP_FORMATS' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
  const hldb::ParamAssign *const pa = getParamAssign("NUM_FP_FORMATS");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "4");
}

TEST_F(PackedArrayHierPathTest, FpEncodingsIsLocalPackedArrayOfEncodingT) {
  const hldb::Parameter *const p = getParam("FP_ENCODINGS");
  ASSERT_NE(p, nullptr) << "parameter 'FP_ENCODINGS' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: declared 'localparam'";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = p->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'fp_encoding_t [0:NUM_FP_FORMATS-1]' is an array type";
  EXPECT_TRUE(at->getPacked()) << "7.4.1: the dimension is written before the name, so it is packed";
  const hldb::Range *const r = at->getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "0");
  const hldb::Operation *const right = r->getRightExpr<hldb::Operation>();
  ASSERT_NE(right, nullptr) << "the right bound 'NUM_FP_FORMATS-1' is an expression";
  EXPECT_EQ(right->getOpType(), vpiSubOp);
  ASSERT_NE(right->getOperands(), nullptr);
  ASSERT_EQ(right->getOperands()->size(), 2u);
  const hldb::RefObj *const num = any_cast<hldb::RefObj>(right->getOperands()->at(0));
  ASSERT_NE(num, nullptr);
  EXPECT_EQ(num->getName(), "NUM_FP_FORMATS");
  EXPECT_EQ(num->getActual(), getParam("NUM_FP_FORMATS"));
  const hldb::Constant *const one = any_cast<hldb::Constant>(right->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Typedef *const td = getEncodingT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::Typespec *const actual = at->getElemTypespec()->getActual();
  ASSERT_NE(actual, nullptr) << "the element type must resolve";
  const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
  EXPECT_TRUE(((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual()))
      << "7.4.1: the element type is fp_encoding_t";
}

TEST_F(PackedArrayHierPathTest, FpEncodingsPatternListsFiveEncodings) {
  const hldb::ParamAssign *const pa = getParamAssign("FP_ENCODINGS");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'FP_ENCODINGS'";
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.1: an array assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 5u) << "the source writes five structure patterns";
  const char *const values[5][2] = {{"8", "23"}, {"11", "52"}, {"5", "10"}, {"5", "2"}, {"8", "7"}};
  for (size_t i = 0; i < 5; ++i) {
    const hldb::Operation *const item = any_cast<hldb::Operation>(pattern->getOperands()->at(i));
    ASSERT_NE(item, nullptr) << "item " << i;
    EXPECT_EQ(item->getOpType(), vpiAssignmentPatternOp) << "10.9.2: item " << i << " is a structure pattern";
    ASSERT_NE(item->getOperands(), nullptr);
    ASSERT_EQ(item->getOperands()->size(), 2u) << "item " << i << ": exp_bits, then man_bits";
    for (size_t j = 0; j < 2; ++j) {
      const hldb::Constant *const c = any_cast<hldb::Constant>(item->getOperands()->at(j));
      ASSERT_NE(c, nullptr) << "item " << i << ", value " << j;
      EXPECT_EQ(c->getDecompile(), values[i][j]) << "item " << i << ", value " << j;
    }
  }
}

// ---------------------------------------------------------------------------
// function automatic int unsigned fp_width(fp_format_e fmt);
// ---------------------------------------------------------------------------

TEST_F(PackedArrayHierPathTest, FpWidthIsAutomaticReturningUnsignedInt) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncs()->size(), 1u) << "'fp_width' is the package's only subroutine";
  const hldb::Function *const fn = getFpWidth();
  ASSERT_NE(fn, nullptr) << "function 'fp_width' not found";
  EXPECT_TRUE(fn->getAutomatic()) << "13.4.2: declared 'function automatic'";
  ASSERT_NE(fn->getReturn(), nullptr);
  const hldb::IntTypespec *const it = fn->getReturn()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "the return type is 'int unsigned'";
  EXPECT_FALSE(it->getSigned()) << "6.11.3: 'unsigned' overrides int's default signedness";
}

TEST_F(PackedArrayHierPathTest, FormalFmtHasUndeclaredType) {
  const hldb::Function *const fn = getFpWidth();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u) << "'fmt' is the only formal";
  const hldb::IODecl *const fmt = getFmt();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDirection(), vpiInput) << "13.4: a formal with no direction is an input";
  ASSERT_NE(fmt->getTypespec(), nullptr) << "'fmt' is declared with the type 'fp_format_e'";
  EXPECT_EQ(fmt->getTypespec()->getActual(), nullptr) << "'fp_format_e' is declared nowhere, so it cannot resolve";
}

TEST_F(PackedArrayHierPathTest, FpWidthReturnsSumOfThreeTerms) {
  const hldb::Function *const fn = getFpWidth();
  ASSERT_NE(fn, nullptr);
  const hldb::ReturnStmt *const ret = fn->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "the body is the single return statement";
  const hldb::Operation *const add = ret->getCondition<hldb::Operation>();
  ASSERT_NE(add, nullptr) << "the returned expression is an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp) << "11.4.3: the terms are joined with '+'";
  const std::vector<const hldb::Any *> terms = getReturnTerms();
  ASSERT_EQ(terms.size(), 3u) << "the source writes three terms";
  const hldb::Constant *const one = any_cast<hldb::Constant>(terms[2]);
  ASSERT_NE(one, nullptr) << "the third term is the literal 1";
  EXPECT_EQ(one->getDecompile(), "1");
}

TEST_F(PackedArrayHierPathTest, FirstTermSelectsExpBitsOfElementFmt) {
  const std::vector<const hldb::Any *> terms = getReturnTerms();
  ASSERT_EQ(terms.size(), 3u);
  ExpectEncodingMember(terms[0], "exp_bits");
}

TEST_F(PackedArrayHierPathTest, SecondTermSelectsManBitsOfElementFmt) {
  const std::vector<const hldb::Any *> terms = getReturnTerms();
  ASSERT_EQ(terms.size(), 3u);
  ExpectEncodingMember(terms[1], "man_bits");
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (undeclared
// type reported as a warning) in the file header.
TEST_F(PackedArrayHierPathTest, UndeclaredTypeFpFormatEIsReportedAsError) {
  ExpectReportedAsError({ErrorDefinition::COMP_UNDEFINED_TYPE, ErrorDefinition::COMP_FAILED_TO_BIND}, "fp_format_e");
}

TEST_F(PackedArrayHierPathTest, PatternItemCountMismatchIsReportedAfterElaboration) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_session->getErrorContainer(), nullptr);
    const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
    EXPECT_GE(stats.nbError, 2) << "10.9.1: 5 items for the 4 elements of FP_ENCODINGS is an error, in addition to "
                                   "the undeclared type fp_format_e";
  }
}

TEST_F(PackedArrayHierPathTest, NoSyntaxOrFatalDiagnostics) {
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
