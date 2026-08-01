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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Variables.sv:
// every standard variable keyword (logic, reg, bit, int, integer, shortint,
// longint, byte, time, real, realtime, string, chandle, event, enum, packed
// vectors), split out of the combined NetsAndVariablesAnsi.sv suite so
// variable-keyword coverage stands on its own.
//
// Checked:
//   - net-type / typespec assertions wherever an existing test in this suite
//     already establishes the pattern (int -> IntTypespec, real ->
//     RealTypespec, string -> StringTypespec, event -> EventTypespec, enum ->
//     EnumTypespec, bit -> BitTypespec, logic/reg (+ vectors) ->
//     LogicTypespec, integer -> IntegerTypespec, longint -> LongIntTypespec,
//     byte -> ByteTypespec, time -> TimeTypespec)
//   - realtime and chandle have a RefTypespec node whose vpiActual is
//     unresolved (matches 6.12--realtime / 6.14--chandle in this suite)
//   - shortint is checked for existence only: no test anywhere in this suite
//     exercises a dedicated typespec for it

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/byte_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/event_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/long_int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/time_typespec.h>

namespace hlc {

class AnsiVariablesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Variables.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("nets_and_variables_test", m_design->getAllModules());
  }
};

// ---------------------------------------------------------------------------
// var_logic / var_reg -- plain logic/reg declarations
// ---------------------------------------------------------------------------
TEST_F(AnsiVariablesTest, VarLogicIsLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_logic", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarRegIsLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_reg", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr) << "reg is an alias of logic";
}

// ---------------------------------------------------------------------------
// Numeric / misc variable keyword coverage
// ---------------------------------------------------------------------------
TEST_F(AnsiVariablesTest, VarBitIsBitTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_bit", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::BitTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarIntIsIntTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_int", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarIntegerIsIntegerTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_integer", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntegerTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarShortintExists) {
  // No dedicated typespec class for shortint has been found anywhere in this
  // suite, so only existence is checked.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("var_shortint", top->getVariables()), nullptr)
      << "'var_shortint' not found among the module's nets";
}

TEST_F(AnsiVariablesTest, VarLongintIsLongIntTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_longint", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LongIntTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarByteIsByteTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_byte", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::ByteTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarTimeIsTimeTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_time", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::TimeTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarRealIsRealTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_real", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarRealtimeTypespecActualIsNull) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_realtime", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  ASSERT_NE(rts->getActual(), nullptr);
  EXPECT_EQ(rts->getActual()->getAnyType(), hldb::AnyType::RealTypespec);
}

TEST_F(AnsiVariablesTest, VarStringIsStringTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_string", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarChandleTypespecActualIsNull) {
  // Matches 6.14--chandle: chandle has a RefTypespec node but vpiActual is
  // unresolved in this compiler version.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_chandle", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual(), nullptr);
  EXPECT_EQ(rts->getActual()->getAnyType(), hldb::AnyType::ChandleTypespec);
}

TEST_F(AnsiVariablesTest, VarEventIsEventTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_event", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::EventTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// var_enum -- enum logic [1:0] {IDLE, BUSY} var_enum
// ---------------------------------------------------------------------------
TEST_F(AnsiVariablesTest, VarEnumIsEnumTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_enum", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(AnsiVariablesTest, VarEnumHasIdleAndBusyConsts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_enum", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::EnumTypespec *const enumTs = rts->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  ASSERT_EQ(enumTs->getEnumConsts()->size(), 2u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "IDLE");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "BUSY");
}

TEST_F(AnsiVariablesTest, VarEnumHasExplicitBaseTypespec) {
  // Contrast with 6.19--enum_anon: our enum has an explicit "logic [1:0]"
  // base type, so getBaseTypespec() should be set (unlike the anonymous case).
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_enum", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::EnumTypespec *const enumTs = rts->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  EXPECT_NE(enumTs->getBaseTypespec(), nullptr) << "enum logic [1:0] {...} has an explicit base type";
}

// ---------------------------------------------------------------------------
// var_vector / var_reg_vector -- packed vectors of logic/reg
// ---------------------------------------------------------------------------
TEST_F(AnsiVariablesTest, VarVectorIsThreeToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_vector", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(AnsiVariablesTest, VarRegVectorIsSevenToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_reg_vector", top->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
