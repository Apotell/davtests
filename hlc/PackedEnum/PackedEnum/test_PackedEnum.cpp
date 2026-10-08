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

// Validates the HLDB model built for tests/PackedEnum/dut.sv:
//
//   module dut;
//      typedef enum logic {c = 0, d = 1} cstate_e;
//      struct packed {
//         cstate_e [1:0] class_esc_state;
//         logic [3:2] blah;
//      } hw2reg_wrap_t;
//   endmodule
//
// The point of the fixture is a packed array of an enumerated type used as a
// member of a packed structure (IEEE 1800-2023 7.2.1, 7.4.1). Note that
// 'hw2reg_wrap_t' is a variable of an anonymous structure type, not a
// typedef. The regression this file exists to catch is HLC losing the packed
// dimension on the enum-typed member.
//
// What is checked, and why:
//   Module dut (23.2)
//     - 'module dut;' writes no port list, so the module has no port at all
//       (a null port is declared only by an empty list, "module M();",
//       37.14 detail 10)
//     - exactly 1 typedef and exactly 1 variable
//   typedef enum logic {c = 0, d = 1} cstate_e; (6.19)
//     - its alias is an EnumTypespec whose base type is a LogicTypespec with
//       no packed range, with exactly 2 names in source order: c with the
//       value Constant "0" and d with the value Constant "1"
//   struct packed { ... } hw2reg_wrap_t; (6.8, 7.2.1)
//     - a Variable, not a typedef, whose type is an anonymous packed
//       StructTypespec with exactly 2 members in source order:
//       class_esc_state, a packed array (7.4.1) with the single packed
//       dimension [1:0] whose element type is cstate_e; and blah, a
//       LogicTypespec with the single packed range [3:2]
//   Elaboration (23.3.1)
//     - dut appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "dut"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the enum values and ranges are literals, so
// only the instance tree is checked under getElaborated().
//
// What is NOT checked, and why:
//   - The width of hw2reg_wrap_t (2 x 1 + 2 = 4 bits). Nothing in the source
//     computes it, so there is no expression whose value could be asserted.
//   - How a RefTypespec refers to cstate_e: it may resolve to the
//     TypedefTypespec or to the EnumTypespec it aliases. Both are that type
//     (6.18), so either is accepted.
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
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {

class PackedEnumTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedEnum.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Typedef *getCstateE() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("cstate_e", dut->getTypedefs());
  }

  static const hldb::Struct *getWrapStruct() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr) return nullptr;
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>("hw2reg_wrap_t", dut->getVariables());
    if (v == nullptr || v->getTypespec() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = v->getTypespec()->getActual<hldb::StructTypespec>();
    if (st == nullptr) return nullptr;
    return st->getStruct();
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

TEST_F(PackedEnumTest, DutHasNoPorts) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr) << "module 'dut' not found";
  EXPECT_TRUE(dut->getPorts() == nullptr || dut->getPorts()->empty())
      << "'module dut;' writes no port list, so it declares no port";
}

TEST_F(PackedEnumTest, DutDeclaresOneTypedefAndOneVariable) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getTypedefs(), nullptr);
  EXPECT_EQ(dut->getTypedefs()->size(), 1u) << "'cstate_e' is the only typedef; hw2reg_wrap_t is a variable";
  ASSERT_NE(dut->getVariables(), nullptr);
  EXPECT_EQ(dut->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("hw2reg_wrap_t", dut->getVariables()), nullptr)
      << "6.8: 'struct packed {...} hw2reg_wrap_t;' declares a variable";
}

TEST_F(PackedEnumTest, CstateEIsOneBitLogicEnum) {
  const hldb::Typedef *const td = getCstateE();
  ASSERT_NE(td, nullptr) << "typedef 'cstate_e' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'cstate_e' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(base, nullptr) << "the base type is 'logic'";
  EXPECT_TRUE(base->getRanges() == nullptr || base->getRanges()->empty()) << "'logic' with no dimension is one bit";
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 2u);
  const char *const names[] = {"c", "d"};
  const char *const values[] = {"0", "1"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::EnumConst *const ec = et->getEnum()->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getName(), names[i]) << "enum name " << i << ", in source order";
    const hldb::Constant *const value = ec->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "'" << names[i] << "' is given an explicit literal value";
    EXPECT_EQ(value->getDecompile(), values[i]);
  }
}

TEST_F(PackedEnumTest, Hw2regWrapTIsPackedStructOfTwoMembers) {
  const hldb::Struct *const s = getWrapStruct();
  ASSERT_NE(s, nullptr) << "'hw2reg_wrap_t' should be typed by an anonymous structure";
  EXPECT_TRUE(s->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 2u);
  EXPECT_EQ(s->getMembers()->at(0)->getName(), "class_esc_state");
  EXPECT_EQ(s->getMembers()->at(1)->getName(), "blah");
}

TEST_F(PackedEnumTest, ClassEscStateIsPackedArray1To0OfCstateE) {
  const hldb::Struct *const s = getWrapStruct();
  ASSERT_NE(s, nullptr);
  const hldb::TypespecMember *const m = hldb::findByName<hldb::TypespecMember>("class_esc_state", s->getMembers());
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = m->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'cstate_e [1:0]' is an array type";
  EXPECT_TRUE(at->getPacked()) << "7.4.1: '[1:0]' is written before the member name, so it is packed";
  ExpectConstRange(at->getRange(), "1", "0");
  const hldb::Typedef *const td = getCstateE();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::Typespec *const actual = at->getElemTypespec()->getActual();
  ASSERT_NE(actual, nullptr) << "the element type must resolve";
  const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
  EXPECT_TRUE(((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual()))
      << "7.4.1: the element type is the enumerated type cstate_e";
}

TEST_F(PackedEnumTest, BlahIsLogic3To2) {
  const hldb::Struct *const s = getWrapStruct();
  ASSERT_NE(s, nullptr);
  const hldb::TypespecMember *const m = hldb::findByName<hldb::TypespecMember>("blah", s->getMembers());
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'blah' is declared 'logic [3:2]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectConstRange(lt->getRanges()->at(0), "3", "2");
}

TEST_F(PackedEnumTest, DutIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'dut' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "dut");
  }
}

TEST_F(PackedEnumTest, NoFatalSyntaxOrErrorDiagnostics) {
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
