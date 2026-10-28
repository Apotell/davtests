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

// Validates the HLDB model built for tests/PartSelect3/dut.sv:
//
//   module t();
//   logic [3:0][3:0][7:0] state_d;
//   assign state_d[2][3][7:0] = 8'b10101010;
//   endmodule
//
// The point of the fixture is a continuous assignment (10.3.2) whose left-hand
// side indexes two packed dimensions of a three-dimensional packed array and
// then part-selects the third. The regression this file exists to catch is
// HLC flattening or mis-nesting the selects, attaching the indexes in the
// wrong order, or modeling [7:0] as anything but a plain part-select.
//
// Values: 7.4.4 says "In a list of dimensions, the rightmost one varies most
// rapidly, as in C.", so state_d is 4 elements of 4 elements of 8 bits, in
// the order [3:0], [3:0], [7:0]. 7.4.5: "A single element of a packed or
// unpacked array can be selected using an indexed name." state_d[2] is
// element 2 of the leftmost [3:0], a logic [3:0][7:0]; state_d[2][3] is
// element 3 of that, a logic [7:0]; and 7.4.5 "The term part-select refers to
// a selection of one or more contiguous bits of a single-dimension packed
// array.", so [7:0] is a part-select of that 8-bit element, 7 - 0 + 1 = 8
// bits wide. The right-hand side 8'b10101010 is 8 bits wide (5.7.1), so the
// two sides match. 37.59 detail 10 fixes the parent of a part-select: "the
// vpiParent object shall correspond to the expression formed by removing the
// part-select range from the expression represented by the part-select", so
// the parent of state_d[2][3][7:0] is state_d[2][3], and Table 37-1 gives the
// same shape for r[4][3][1:0] -> r[4][3].
//
// What is checked, and why:
//   Module t
//     - is the only module, and its '()' declares exactly one null port
//       (37.14 detail 10, "module M();") with no name (detail 8), no low or
//       high connection (detail 10), port index 0 (detail 9) and port type
//       vpiPort (detail 1)
//     - declares exactly one data object, the variable state_d, and no net:
//       the declaration is a data_declaration with no net type (6.8)
//   Variable state_d
//     - has no initializer: 10.3.2, "It shall be an error for a variable
//       driven by a continuous assignment or output to have an initializer in
//       the declaration or any procedural assignment."
//     - has a static lifetime: 6.21, "Variables declared inside a module,
//       interface, program, or checker, but outside a task, process, or
//       function, are local in scope and have a static lifetime."
//     - is a logic with the three packed dimensions [3:0], [3:0] and [7:0],
//       in that order (7.4.4), each bound an unsized decimal literal
//     - is unsigned: 6.9.1, "Vectors of reg, logic, and bit types shall be
//       treated as unsigned quantities, unless declared to be signed"
//     - is a vector and not a scalar: 37.17 detail 20, "A bit var or logic
//       var, with one or more packed dimensions defined, is a vector, and the
//       property vpiVector shall return TRUE (vpiScalar shall return FALSE)."
//   The continuous assignment
//     - t holds exactly one, owned by t (37.47 vpiParent module), which is
//       a continuous assign statement and not a net declaration assignment
//       (10.3.1, 10.3.2), with no delay
//     - its LHS is a PartSelect (11.5.1, non-indexed) with the range [7:0];
//       11.5.1: "The first expression shall address a more significant bit
//       than the second expression."
//     - the parent of the part-select is the bit-select state_d[2][3] with
//       index 3, whose parent is the bit-select state_d[2] with index 2,
//       whose parent is the name state_d bound to the variable (37.59 detail
//       10, Table 37-1)
//     - all three selects are constant selects (37.59 detail 9, 37.58 detail
//       3, 37.17 detail 27): every index and range bound is a literal, which
//       11.2.1 counts among the "constant numbers" of a constant expression,
//       and state_d is a static variable with static bounds
//     - every index and bound literal (2, 3, 7, 0) is unsized, so at least 32
//       bits (5.7.1), and signed (5.7.1) wherever HLC attaches a typespec
//     - the RHS is the literal 8'b10101010: vpiBinaryConst ("binary
//       integer"), 8 bits, unsigned (5.7.1) wherever HLC attaches a typespec
//   Elaboration
//     - each range's vpiSize counts its elements: 4, 4 and 8 for the declared
//       dimensions and 8 for the part-select range (37.22, 37.18 detail 2)
//     - t is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the range sizes and the top-level instance are
// checked only when the design is elaborated, on Design::getTopModules();
// everything else is fixed by the source text and is asserted on the
// definition in every run. That includes vpiConstantSelect: every index and
// bound is a literal and state_d's bounds are literals, so nothing has to be
// elaborated to know the selects are constant (HLC's own parse tree already
// labels them Constant_select). 37.59 detail 5 makes the typespec of a constant
// implementation dependent ("For other expressions, it is implementation
// dependent as to whether or not there is any associated typespec."), so a
// literal's signedness is checked only where HLC attaches one.
//
// KNOWN COMPILER BUG (constant selects not flagged), not a defect in this
// test: HLC leaves vpiConstantSelect FALSE on all three selects of
// 'state_d[2][3][7:0]', although 37.58 detail 3 says "The property
// vpiConstantSelect shall return TRUE for a bit-select if -- every associated
// index expression is an elaboration-time constant expression, and --
// vpiConstantSelect returns TRUE for the parent of the bit-select." and 37.59
// detail 9 says "The property vpiConstantSelect shall return TRUE for a
// part-select or indexed part-select if -- vpiConstantSelect returns TRUE for
// its parent, and -- the parent is a packed or unpacked array with static
// bounds, and -- each range expression in the part-select or indexed
// part-select is an elaboration-time constant expression." The root of the
// chain qualifies by 37.17 detail 27: "The property vpiConstantSelect shall
// return TRUE for a var bit or other variable if it has a static lifetime and
// has no parent (the vpiParent relation returns NULL)". AllSelectsAreConstantSelects
// is expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// Model convention: the relation 37.59 detail 10 calls the vpiParent of a
// select is held as Select::getPrefix(); HLDB's own getParent() of a select is
// the node that syntactically owns it (the ContAssign, or the enclosing
// select). The tests walk getPrefix().
//
// Model convention: an HLDB BitSelect carries a single index, so the
// two-index bit-select state_d[2][3] is a chain of two BitSelects, state_d[2]
// being the prefix of state_d[2][3]. This is the same "expression formed by
// removing" the last select that 37.59 detail 10 uses for a part-select.
//
// Model convention: the vpiVector and vpiScalar properties 37.17 detail 20
// gives the variable are held on its LogicTypespec.
//
// What is NOT checked, and why:
//   - The vpiConstType of the unsized decimals 2, 3, 7 and 0. The VPI header
//     offers both vpiDecConst ("decimal integer") and vpiIntConst ("integer
//     constant (1364-2001)"), and the standard does not say which one an
//     unsized decimal takes.
//   - The width carried by the typespec HLC attaches to 8'b10101010. 37.59
//     detail 5 makes that typespec implementation dependent, so only its
//     signedness is asserted, and the width is asserted through the
//     constant's vpiSize.
//   - The typespec of state_d[2] and state_d[2][3], which 37.59 detail 5
//     requires for simple expressions. HLDB's BitSelect, PartSelect and RefObj
//     have no typespec relation, so the model has no place to hold it.
//   - Whether the variable's drivers list the continuous assignment. 37.21
//     detail 2 only says the drivers of a variable array "should include
//     driver/load for entire array/vector or any portion of an array/vector to
//     which a handle can be obtained", which is a recommendation.
//   - The direction of the null port. 37.14 gives none for a null port.
//   - The drive strength of the assignment. The source writes none, and
//     10.3.4 says a drive strength "applies only to assignments to scalar
//     nets, except for nets of types supply0 and supply1", while the LHS here
//     selects from a variable.
//   - The value state_d holds once 8'b10101010 is driven onto
//     state_d[2][3][7:0]. It exists only while time advances. HLC is a static
//     compiler/elaborator and is not a simulator, so this is out of scope
//     permanently rather than a gap waiting to be filled, and no placeholder
//     test is carried for it; RhsIs8BitBinaryLiteral and LhsIsPartSelect7To0
//     are the static evidence of what is assigned where.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string_view>

namespace hlc {

class PartSelect3Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PartSelect3.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Module t as written, or its elaborated top-level instance.
  static const hldb::Module *getT(bool elaborated = false) {
    return hldb::findByDefName<hldb::Module>("t", elaborated ? m_design->getTopModules() : m_design->getAllModules());
  }

  static const hldb::Variable *getStateD(const hldb::Module *t) {
    if (t == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("state_d", t->getVariables());
  }

  static const hldb::LogicTypespec *getStateDType(const hldb::Module *t) {
    const hldb::Variable *const v = getStateD(t);
    if ((v == nullptr) || (v->getTypespec() == nullptr)) return nullptr;
    return v->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  // The only continuous assignment of 't'.
  static const hldb::ContAssign *getAssign(const hldb::Module *t) {
    if ((t == nullptr) || (t->getContAssigns() == nullptr) || (t->getContAssigns()->size() != 1)) return nullptr;
    return t->getContAssigns()->at(0);
  }

  // The LHS state_d[2][3][7:0].
  static const hldb::PartSelect *getLhs(const hldb::Module *t) {
    const hldb::ContAssign *const ca = getAssign(t);
    if (ca == nullptr) return nullptr;
    return ca->getLhs<hldb::PartSelect>();
  }

  // The parent state_d[2][3] of the part-select.
  static const hldb::BitSelect *getSel23(const hldb::Module *t) {
    const hldb::PartSelect *const ps = getLhs(t);
    if (ps == nullptr) return nullptr;
    return ps->getPrefix<hldb::BitSelect>();
  }

  // The parent state_d[2] of state_d[2][3].
  static const hldb::BitSelect *getSel2(const hldb::Module *t) {
    const hldb::BitSelect *const sel23 = getSel23(t);
    if (sel23 == nullptr) return nullptr;
    return sel23->getPrefix<hldb::BitSelect>();
  }

  // Whether the integral typespec 'ts' is signed; 'known' reports whether
  // 'ts' is an integral typespec this file can read.
  static bool isSigned(const hldb::Typespec *ts, bool *known) {
    *known = true;
    if (const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(ts)) return it->getSigned();
    if (const hldb::IntegerTypespec *const it = any_cast<hldb::IntegerTypespec>(ts)) return it->getSigned();
    if (const hldb::LogicTypespec *const lt = any_cast<hldb::LogicTypespec>(ts)) return lt->getSigned();
    if (const hldb::BitTypespec *const bt = any_cast<hldb::BitTypespec>(ts)) return bt->getSigned();
    *known = false;
    return false;
  }

  // Verifies the signedness of the typespec attached to 'c', if any. 37.59
  // detail 5 makes the typespec of a constant implementation dependent.
  static void ExpectSignedness(const hldb::Constant *c, bool expected, std::string_view why) {
    if (c->getTypespec() == nullptr) return;
    const hldb::Typespec *const ts = c->getTypespec()->getActual();
    ASSERT_NE(ts, nullptr) << "the typespec attached to '" << c->getDecompile() << "' is bound";
    bool known = false;
    const bool isSignedType = isSigned(ts, &known);
    ASSERT_TRUE(known) << "the typespec attached to '" << c->getDecompile() << "' is integral";
    EXPECT_EQ(isSignedType, expected) << why;
  }

  // Verifies 'e' is the unsized decimal literal 'text'.
  static void ExpectUnsizedDecimal(const hldb::Expr *e, std::string_view text, std::string_view what) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << what << " is the literal " << text;
    EXPECT_EQ(c->getDecompile(), text) << what;
    EXPECT_GE(c->getSize(), 32) << "5.7.1: " << what << " is unsized, and \"The number of bits that make up an "
                                << "unsized number (which is a simple decimal number or a number with a base "
                                << "specifier but no size specification) shall be at least 32.\"";
    ExpectSignedness(c, true,
                     "5.7.1: \"Simple decimal numbers without the size and the base format shall be treated as "
                     "signed integers\"");
  }
};

// ---------------------------------------------------------------------------
// module t
// ---------------------------------------------------------------------------

TEST_F(PartSelect3Test, TIsTheOnlyModule) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  ASSERT_EQ(m_design->getAllModules()->size(), 1u) << "the source declares one module";
  EXPECT_EQ(m_design->getAllModules()->at(0)->getDefName(), "t");
}

TEST_F(PartSelect3Test, EmptyPortListDeclaresOneNullPort) {
  const hldb::Module *const t = getT();
  ASSERT_NE(t, nullptr) << "module 't' not found";
  ASSERT_NE(t->getPorts(), nullptr) << "37.14 detail 10: t's '()' declares a null port";
  ASSERT_EQ(t->getPorts()->size(), 1u) << "37.14 detail 10: t's '()' declares exactly one null port";
  const hldb::Port *const p = t->getPorts()->at(0);
  EXPECT_EQ(p->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(p->getLowConn(), nullptr) << "37.14 detail 10: a null port has no low connection";
  EXPECT_EQ(p->getHighConn(), nullptr) << "37.14 detail 10: nothing connects to the port";
  EXPECT_EQ(p->getPortIndex(), 0) << "37.14 detail 9: \"The first port has a port index of zero.\"";
  EXPECT_EQ(p->getPortType(), vpiPort) << "37.14 detail 1: neither an interface nor a modport port";
}

TEST_F(PartSelect3Test, StateDIsTheOnlyDeclaration) {
  const hldb::Module *const t = getT();
  ASSERT_NE(t, nullptr);
  ASSERT_NE(t->getVariables(), nullptr) << "6.8: 'logic ... state_d;' declares a variable";
  ASSERT_EQ(t->getVariables()->size(), 1u);
  EXPECT_EQ(t->getVariables()->at(0)->getName(), "state_d");
  EXPECT_TRUE((t->getNets() == nullptr) || t->getNets()->empty())
      << "6.8: the declaration writes no net type, so it declares no net";
}

TEST_F(PartSelect3Test, StateDIsStaticWithNoInitializer) {
  const hldb::Variable *const v = getStateD(getT());
  ASSERT_NE(v, nullptr) << "variable 'state_d' not found";
  EXPECT_EQ(v->getExpr(), nullptr) << "10.3.2: a variable driven by a continuous assignment has no initializer";
  EXPECT_FALSE(v->getAutomatic()) << "6.21: a variable declared in a module has a static lifetime";
}

TEST_F(PartSelect3Test, StateDIsUnsignedLogic3To0By3To0By7To0) {
  const hldb::Variable *const v = getStateD(getT());
  ASSERT_NE(v, nullptr) << "variable 'state_d' not found";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = v->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "state_d is declared 'logic'";
  EXPECT_FALSE(lt->getSigned()) << "6.9.1: a logic vector is unsigned unless declared signed";
  EXPECT_TRUE(lt->getVector()) << "37.17 detail 20: a logic var with packed dimensions is a vector";
  EXPECT_FALSE(lt->getScalar()) << "37.17 detail 20: a logic var with packed dimensions is not a scalar";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 3u) << "7.4.1: three packed dimensions";
  const std::string_view lefts[] = {"3", "3", "7"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Range *const r = lt->getRanges()->at(i);
    ExpectUnsizedDecimal(r->getLeftExpr(), lefts[i], "the left bound of the declared dimension");
    ExpectUnsizedDecimal(r->getRightExpr(), "0", "the right bound of the declared dimension");
  }
}

// ---------------------------------------------------------------------------
// assign state_d[2][3][7:0] = 8'b10101010;
// ---------------------------------------------------------------------------

TEST_F(PartSelect3Test, TDeclaresOneContinuousAssignStatement) {
  const hldb::Module *const t = getT();
  ASSERT_NE(t, nullptr);
  ASSERT_NE(t->getContAssigns(), nullptr);
  ASSERT_EQ(t->getContAssigns()->size(), 1u) << "the source writes one continuous assignment";
  const hldb::ContAssign *const ca = t->getContAssigns()->at(0);
  EXPECT_EQ(ca->getParent(), t) << "37.47: the vpiParent of a cont assign is its module";
  EXPECT_FALSE(ca->getNetDeclAssign()) << "10.3.2: an 'assign' statement, not a net declaration assignment";
  EXPECT_EQ(ca->getDelay(), nullptr) << "the statement writes no delay";
}

TEST_F(PartSelect3Test, LhsIsPartSelect7To0) {
  const hldb::ContAssign *const ca = getAssign(getT());
  ASSERT_NE(ca, nullptr);
  ASSERT_NE(ca->getLhs(), nullptr);
  EXPECT_EQ(ca->getLhs()->getAnyType(), hldb::AnyType::PartSelect) << "11.5.1: [7:0] is a non-indexed part-select";
  const hldb::PartSelect *const ps = getLhs(getT());
  ASSERT_NE(ps, nullptr);
  ASSERT_NE(ps->getRange(), nullptr);
  ExpectUnsizedDecimal(ps->getRange()->getLeftExpr(), "7", "the msb of the part-select");
  ExpectUnsizedDecimal(ps->getRange()->getRightExpr(), "0", "the lsb of the part-select");
}

TEST_F(PartSelect3Test, PartSelectParentIsStateD2Then3) {
  const hldb::PartSelect *const ps = getLhs(getT());
  ASSERT_NE(ps, nullptr);
  ASSERT_NE(ps->getPrefix(), nullptr);
  EXPECT_EQ(ps->getPrefix()->getAnyType(), hldb::AnyType::BitSelect)
      << "37.59 detail 10: the parent of state_d[2][3][7:0] is the bit-select state_d[2][3]";
  const hldb::BitSelect *const sel23 = getSel23(getT());
  ASSERT_NE(sel23, nullptr);
  ExpectUnsizedDecimal(sel23->getIndex(), "3", "the index of state_d[2][3]");
  ASSERT_NE(sel23->getPrefix(), nullptr);
  EXPECT_EQ(sel23->getPrefix()->getAnyType(), hldb::AnyType::BitSelect)
      << "the parent of state_d[2][3] is the bit-select state_d[2]";
  const hldb::BitSelect *const sel2 = getSel2(getT());
  ASSERT_NE(sel2, nullptr);
  ExpectUnsizedDecimal(sel2->getIndex(), "2", "the index of state_d[2]");
  const hldb::RefObj *const ref = sel2->getPrefix<hldb::RefObj>();
  ASSERT_NE(ref, nullptr) << "the parent of state_d[2] is the name state_d";
  EXPECT_EQ(ref->getName(), "state_d");
  ASSERT_NE(getStateD(getT()), nullptr);
  EXPECT_EQ(ref->getActual(), getStateD(getT())) << "'state_d' is t's variable";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (constant
// selects not flagged) in the file header.
TEST_F(PartSelect3Test, AllSelectsAreConstantSelects) {
  const hldb::BitSelect *const sel2 = getSel2(getT());
  ASSERT_NE(sel2, nullptr);
  EXPECT_TRUE(sel2->getConstantSelect())
      << "37.58 detail 3: the index 2 is a constant and state_d is a static variable (37.17 detail 27)";
  const hldb::BitSelect *const sel23 = getSel23(getT());
  ASSERT_NE(sel23, nullptr);
  EXPECT_TRUE(sel23->getConstantSelect()) << "37.58 detail 3: the index 3 is a constant and state_d[2] is constant";
  const hldb::PartSelect *const ps = getLhs(getT());
  ASSERT_NE(ps, nullptr);
  EXPECT_TRUE(ps->getConstantSelect())
      << "37.59 detail 9: state_d[2][3] is constant, a packed array with static bounds, and 7 and 0 are constants";
}

TEST_F(PartSelect3Test, RhsIs8BitBinaryLiteral) {
  const hldb::ContAssign *const ca = getAssign(getT());
  ASSERT_NE(ca, nullptr);
  const hldb::Constant *const c = ca->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "the RHS is the literal 8'b10101010";
  EXPECT_EQ(c->getDecompile(), "8'b10101010");
  EXPECT_EQ(c->getConstType(), vpiBinaryConst) << "the base format 'b' makes it a binary integer";
  EXPECT_EQ(c->getSize(), 8) << "5.7.1: the size constant 8 gives the width in bits";
  ExpectSignedness(c, false, "5.7.1: based numbers are \"unsigned integers if the base format only is used\"");
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PartSelect3Test, RangeSizesCountElements) {
  if (m_design->getElaborated()) {
    const hldb::Module *const t = getT(true);
    ASSERT_NE(t, nullptr) << "the elaborated t not found";
    const hldb::LogicTypespec *const lt = getStateDType(t);
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 3u);
    const int32_t sizes[] = {4, 4, 8};
    for (size_t i = 0; i < 3; ++i) {
      EXPECT_EQ(lt->getRanges()->at(i)->getSize(), sizes[i])
          << "37.18 detail 2: the element count is \"the product of the vpiSize property for all of its packed "
          << "ranges\"; dimension " << i;
    }
    const hldb::PartSelect *const ps = getLhs(t);
    ASSERT_NE(ps, nullptr);
    ASSERT_NE(ps->getRange(), nullptr);
    EXPECT_EQ(ps->getRange()->getSize(), 8) << "[7:0] spans 8 bits";
  }
}

TEST_F(PartSelect3Test, TIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: t is not instantiated anywhere";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "t");
  }
}

TEST_F(PartSelect3Test, NoFatalSyntaxOrErrorDiagnostics) {
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
