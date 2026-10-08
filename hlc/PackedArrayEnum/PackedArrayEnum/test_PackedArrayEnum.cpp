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

// Validates the HLDB model built for tests/PackedArrayEnum/dut.sv:
//
//   module top();
//     typedef enum logic [13:0] {
//       DecLcStTestUnlocked0 = 1,
//       DecLcStTestLocked0 = 2
//     } dec_lc_state_e;
//     typedef dec_lc_state_e [17:0] ext_dec_lc_state_t;
//     ext_dec_lc_state_t transition_target_d;
//   endmodule
//
// The point of the fixture is a typedef that adds a packed dimension to an
// enumerated type (IEEE 1800-2023 7.4.1: "Packed arrays can be made of only
// the single bit data types (bit, logic, reg), enumerated types, and
// recursively other packed arrays and packed structures"), and a variable of
// that type. The regression this file exists to catch is HLC losing the
// packed dimension or the enum element type of such a typedef.
//
// What is checked, and why:
//   Module top (23.2)
//     - '()' declares one null port (37.14 detail 10, "module M();"): no
//       name (detail 8), port index 0 (detail 9), and no low connection
//     - exactly 2 typedefs and exactly 1 variable
//   typedef enum logic [13:0] { ... } dec_lc_state_e; (6.19)
//     - its alias is an EnumTypespec whose base type is a LogicTypespec with
//       the single packed range [13:0], with exactly 2 names in source
//       order: DecLcStTestUnlocked0 with the value Constant "1", and
//       DecLcStTestLocked0 with the value Constant "2"
//   typedef dec_lc_state_e [17:0] ext_dec_lc_state_t; (6.18, 7.4.1)
//     - its alias is a packed array with the single packed dimension [17:0]
//       whose element type is dec_lc_state_e
//   ext_dec_lc_state_t transition_target_d; (6.8)
//     - a Variable typed by ext_dec_lc_state_t, with no initializer
//   Elaboration (23.3.1)
//     - top appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "top"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the enum values are already literals and the
// ranges are constant literals, so only the instance tree is checked under
// getElaborated().
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models 'module top();' with no port at all, although 37.14 detail 10 names
// "module M();" as declaring a null port. TopHasOneNullPort is expected to
// fail until HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The width of transition_target_d (18 x 14 = 252 bits). Nothing in the
//     source computes it, so there is no expression whose value could be
//     asserted.
//   - How a RefTypespec refers to a typedef: it may resolve to the
//     TypedefTypespec or to the typespec the typedef aliases. Both are that
//     type (6.18), so either is accepted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {

class PackedArrayEnumTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedArrayEnum.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Typedef *getTypedef(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>(name, top->getTypedefs());
  }

  // Whether 'rts' names the type of 'td': it may resolve to the
  // TypedefTypespec or to the typespec the typedef aliases (6.18).
  static bool namesTypedef(const hldb::RefTypespec *rts, const hldb::Typedef *td) {
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr) return false;
    const hldb::Typespec *const actual = rts->getActual();
    if (actual == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual());
  }

  // Verifies 'range' is [left:right] with Constant bounds.
  static void ExpectConstRange(const hldb::Range *range, std::string_view left, std::string_view right) {
    ASSERT_NE(range, nullptr);
    const hldb::Constant *const l = range->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const r = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(l->getDecompile(), left);
    EXPECT_EQ(r->getDecompile(), right);
  }
};

// ---------------------------------------------------------------------------
// module top();
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(PackedArrayEnumTest, TopHasOneNullPort) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(top->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  const hldb::Port *const port = top->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(port->getPortIndex(), 0) << "37.14 detail 9: the first port has index 0";
  EXPECT_EQ(port->getLowConn(), nullptr) << "37.14 detail 10: a null port has no low connection";
}

TEST_F(PackedArrayEnumTest, TopDeclaresTwoTypedefsAndOneVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypedefs(), nullptr);
  EXPECT_EQ(top->getTypedefs()->size(), 2u) << "'dec_lc_state_e' and 'ext_dec_lc_state_t'";
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "'transition_target_d' is the only variable";
}

// ---------------------------------------------------------------------------
// typedef enum logic [13:0] { ... } dec_lc_state_e;
// ---------------------------------------------------------------------------

TEST_F(PackedArrayEnumTest, DecLcStateEIsLogic13To0Enum) {
  const hldb::Typedef *const td = getTypedef("dec_lc_state_e");
  ASSERT_NE(td, nullptr) << "typedef 'dec_lc_state_e' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'dec_lc_state_e' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  const hldb::RefTypespec *const base = et->getEnum()->getBaseTypespec();
  ASSERT_NE(base, nullptr) << "6.19: the explicit base type 'logic [13:0]' is recorded";
  const hldb::LogicTypespec *const lt = base->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "the base type is 'logic [13:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectConstRange(lt->getRanges()->at(0), "13", "0");
}

TEST_F(PackedArrayEnumTest, DecLcStateEHasTwoNamesWithValues) {
  const hldb::Typedef *const td = getTypedef("dec_lc_state_e");
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(et->getEnum(), nullptr);
  const hldb::EnumConstCollection *const consts = et->getEnum()->getEnumConsts();
  ASSERT_NE(consts, nullptr);
  ASSERT_EQ(consts->size(), 2u);
  const char *const names[] = {"DecLcStTestUnlocked0", "DecLcStTestLocked0"};
  const char *const values[] = {"1", "2"};
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_NE(consts->at(i), nullptr);
    EXPECT_EQ(consts->at(i)->getName(), names[i]) << "enum name " << i << ", in source order";
    const hldb::Constant *const value = consts->at(i)->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "'" << names[i] << "' is given an explicit literal value";
    EXPECT_EQ(value->getDecompile(), values[i]);
  }
}

// ---------------------------------------------------------------------------
// typedef dec_lc_state_e [17:0] ext_dec_lc_state_t;
// ---------------------------------------------------------------------------

TEST_F(PackedArrayEnumTest, ExtDecLcStateTIsPackedArrayOfDecLcStateE) {
  const hldb::Typedef *const td = getTypedef("ext_dec_lc_state_t");
  ASSERT_NE(td, nullptr) << "typedef 'ext_dec_lc_state_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::ArrayTypespec *const at = td->getAlias()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'dec_lc_state_e [17:0]' is an array type";
  EXPECT_TRUE(at->getPacked()) << "7.4.1: '[17:0]' follows the type, so it is a packed dimension";
  ExpectConstRange(at->getRange(), "17", "0");
  EXPECT_TRUE(namesTypedef(at->getElemTypespec(), getTypedef("dec_lc_state_e")))
      << "7.4.1: the element type is the enumerated type dec_lc_state_e";
}

// ---------------------------------------------------------------------------
// ext_dec_lc_state_t transition_target_d;
// ---------------------------------------------------------------------------

TEST_F(PackedArrayEnumTest, TransitionTargetDIsTypedByExtDecLcStateT) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("transition_target_d", top->getVariables());
  ASSERT_NE(v, nullptr) << "variable 'transition_target_d' not found";
  EXPECT_TRUE(namesTypedef(v->getTypespec(), getTypedef("ext_dec_lc_state_t")))
      << "6.18: the variable is declared with the type ext_dec_lc_state_t";
  EXPECT_EQ(v->getValue(), nullptr) << "the declaration has no initializer";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackedArrayEnumTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'top' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "top");
  }
}

TEST_F(PackedArrayEnumTest, NoFatalSyntaxOrErrorDiagnostics) {
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
