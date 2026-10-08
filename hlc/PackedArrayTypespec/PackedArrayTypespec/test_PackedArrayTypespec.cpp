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

// Validates the HLDB model built for tests/PackedArrayTypespec/dut.sv:
//
//   module top();
//      typedef enum logic [1:0] {
//       DISABLED,
//       PARALLEL,
//       MERGED
//     } unit_type_t;
//     typedef unit_type_t [0:4] fmt_unit_types_t;
//     typedef fmt_unit_types_t [0:1] opgrp_fmt_unit_types_t;
//   endmodule
//
// The point of the fixture is a chain of typedefs, each adding a packed
// dimension to the previous one: an enum, a packed array of 5 enums, and a
// packed array of 2 of those (IEEE 1800-2023 6.18, 7.4.1). The regression
// this file exists to catch is HLC losing a level of the chain or the order
// of an ascending [0:n] range.
//
// What is checked, and why:
//   Module top (23.2)
//     - '()' declares one null port (37.14 detail 10, "module M();"): no
//       name (detail 8), port index 0 (detail 9), and no low connection
//     - exactly 3 typedefs and no variables
//   typedef enum logic [1:0] { ... } unit_type_t; (6.19)
//     - its alias is an EnumTypespec whose base type is a LogicTypespec with
//       the single packed range [1:0], with exactly 3 names in source order:
//       DISABLED, PARALLEL, MERGED
//   typedef unit_type_t [0:4] fmt_unit_types_t;
//     - its alias is a packed array with the single packed dimension [0:4]
//       (an ascending range, left bound 0 and right bound 4) whose element
//       type is unit_type_t
//   typedef fmt_unit_types_t [0:1] opgrp_fmt_unit_types_t;
//     - its alias is a packed array with the single packed dimension [0:1]
//       whose element type is fmt_unit_types_t
//   Elaboration (23.3.1)
//     - top appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "top"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the ranges are constant literals, so only the
// instance tree is checked under getElaborated().
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models 'module top();' with no port at all, although 37.14 detail 10 names
// "module M();" as declaring a null port. TopHasOneNullPort is expected to
// fail until HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The implicit values of the enum names (0, 1 and 2, 6.19) are not
//     written in the source, and whether HLC materializes them as value
//     expressions is a tool convention.
//   - The widths of the types (2, 10 and 20 bits). Nothing in the source
//     computes them, so there is no expression whose value could be asserted.
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

#include <string_view>

namespace hlc {

class PackedArrayTypespecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedArrayTypespec.hlc"}); }
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

  // Verifies typedef 'name' aliases a packed array [left:right] of 'elem'.
  static void ExpectPackedArrayTypedef(std::string_view name, std::string_view left, std::string_view right,
                                       std::string_view elem) {
    const hldb::Typedef *const td = getTypedef(name);
    ASSERT_NE(td, nullptr) << "typedef '" << name << "' not found";
    ASSERT_NE(td->getAlias(), nullptr);
    const hldb::ArrayTypespec *const at = td->getAlias()->getActual<hldb::ArrayTypespec>();
    ASSERT_NE(at, nullptr) << "'" << name << "' names an array type";
    EXPECT_TRUE(at->getPacked()) << "7.4.1: the dimension follows the type, so it is packed";
    ExpectConstRange(at->getRange(), left, right);
    EXPECT_TRUE(namesTypedef(at->getElemTypespec(), getTypedef(elem)))
        << "7.4.1: the element type of '" << name << "' is " << elem;
  }
};

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(PackedArrayTypespecTest, TopHasOneNullPort) {
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

TEST_F(PackedArrayTypespecTest, TopDeclaresThreeTypedefsAndNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypedefs(), nullptr);
  EXPECT_EQ(top->getTypedefs()->size(), 3u);
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty()) << "the module declares only types";
}

TEST_F(PackedArrayTypespecTest, UnitTypeTIsLogic1To0EnumOfThreeNames) {
  const hldb::Typedef *const td = getTypedef("unit_type_t");
  ASSERT_NE(td, nullptr) << "typedef 'unit_type_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'unit_type_t' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(base, nullptr) << "the base type is 'logic [1:0]'";
  ASSERT_NE(base->getRanges(), nullptr);
  ASSERT_EQ(base->getRanges()->size(), 1u);
  ExpectConstRange(base->getRanges()->at(0), "1", "0");
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 3u);
  const char *const names[] = {"DISABLED", "PARALLEL", "MERGED"};
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_NE(et->getEnum()->getEnumConsts()->at(i), nullptr);
    EXPECT_EQ(et->getEnum()->getEnumConsts()->at(i)->getName(), names[i]) << "enum name " << i << ", in source order";
  }
}

TEST_F(PackedArrayTypespecTest, FmtUnitTypesTIsPackedArray0To4OfUnitTypeT) {
  ExpectPackedArrayTypedef("fmt_unit_types_t", "0", "4", "unit_type_t");
}

TEST_F(PackedArrayTypespecTest, OpgrpFmtUnitTypesTIsPackedArray0To1OfFmtUnitTypesT) {
  ExpectPackedArrayTypedef("opgrp_fmt_unit_types_t", "0", "1", "fmt_unit_types_t");
}

TEST_F(PackedArrayTypespecTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'top' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "top");
  }
}

TEST_F(PackedArrayTypespecTest, NoFatalSyntaxOrErrorDiagnostics) {
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
