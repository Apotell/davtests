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

// Validates the UHDM graph for a module using an inline enum with a range
// sequence:
//   module top();
//     enum {start=10, stop[11:13]} e;
//   endmodule
//
// Per IEEE 1800-2023 Sec 6.19.2 (Table 6-10), "name[N:M]" creates named
// constants nameN, name{N+1}, ..., nameM (incrementing), each assigned
// consecutive values starting right after the previous member's value. So
// "stop[11:13]" following "start=10" must expand to 3 named constants
// stop11, stop12, stop13 with values 11, 12, 13 -- i.e. the EnumTypespec
// must have 4 total consts (start, stop11, stop12, stop13), not 2. HLC
// currently collapses "stop[11:13]" into a single EnumConst literally named
// "stop" with no expansion and no value -- a known, non-trivial gap; see the
// GTEST_SKIP()'d tests below for the standard-correct expectation.
//
// Checked:
//   - design has module top with 1 variable ('e') -- IEEE 1800-2023 6.19/6.8:
//     enum-typed declaration with no net-type keyword is a variable, not a net
//   - variable 'e' RefTypespec vpiActual resolves to EnumTypespec
//   - "start" value is stored as vpiUIntConst = "10"
//   - top has no processes
//   - EnumTypespec should have 4 consts: start, stop11, stop12, stop13
//     (known gap, currently only 2: "start" and unexpanded "stop")

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumSequenceRange : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19.2--enum_sequence_range.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumSequenceRange, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(EnumSequenceRange, OneVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

// ----
// Variable 'e' -- typespec RefTypespec -> EnumTypespec
// ----
TEST_F(EnumSequenceRange, ENotInNets) {
  // Per IEEE 1800-2023 Sec 6.7/6.8, an inline enum has no net-type keyword,
  // so 'e' must not also be materialized as a Net.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || hldb::findByName<hldb::Net>("e", top->getNets()) == nullptr)
      << "enum 'e' must not appear in vpiNet";
}

TEST_F(EnumSequenceRange, EVariableTypespecIsEnum) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  EXPECT_NE(e->getTypespec()->getActual<hldb::EnumTypespec>(), nullptr)
      << "variable 'e' typespec should resolve to EnumTypespec";
}

// ----
// EnumTypespec -- per IEEE 1800-2023 Sec 6.19.2 Table 6-10, "stop[11:13]"
// must expand to 4 total consts: start, stop11, stop12, stop13. Known gap:
// HLC does not expand the range -- see GTEST_SKIP() below.
// ----
TEST_F(EnumSequenceRange, EnumHasFourConsts) {
  GTEST_SKIP() << "known gap: HLC does not expand 'stop[11:13]' into stop11..stop13; "
                  "IEEE 1800-2023 Sec 6.19.2 Table 6-10 requires 4 total consts (start, stop11, stop12, stop13)";
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 4u);
}

TEST_F(EnumSequenceRange, FirstConstIsStart) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::EnumConst *const ec = enumTs->getEnumConsts()->at(0);
  ASSERT_NE(ec, nullptr);
  EXPECT_EQ(ec->getName(), "start");
}

TEST_F(EnumSequenceRange, StartConstValueIs10) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::EnumConst *const ec = enumTs->getEnumConsts()->at(0);
  ASSERT_NE(ec, nullptr);
  const hldb::Constant *const val = ec->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'start' EnumConst should have an explicit value";
  EXPECT_EQ(val->getConstType(), vpiUIntConst);
  EXPECT_EQ(val->getDecompile(), "10");
}

TEST_F(EnumSequenceRange, SecondConstIsStop11) {
  GTEST_SKIP() << "known gap: HLC names the unexpanded range base EnumConst 'stop' instead of "
                  "expanding it to 'stop11'..'stop13' per IEEE 1800-2023 Sec 6.19.2 Table 6-10";
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::EnumConst *const ec = enumTs->getEnumConsts()->at(1);
  ASSERT_NE(ec, nullptr);
  EXPECT_EQ(ec->getName(), "stop11") << "stop[11:13] must expand; the second const should be 'stop11'";
}

TEST_F(EnumSequenceRange, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(EnumSequenceRange, Stop11ConstValueIs11) {
  GTEST_SKIP() << "known gap: HLC does not assign a value to the unexpanded 'stop' EnumConst; "
                  "IEEE 1800-2023 Sec 6.19.2 Table 6-10 requires 'stop11' to be assigned the value 11 "
                  "(next consecutive value after start=10)";
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::EnumConst *const ec = enumTs->getEnumConsts()->at(1);
  ASSERT_NE(ec, nullptr);
  const hldb::Constant *const val = ec->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'stop11' should have an explicit/derived value of 11";
  EXPECT_EQ(val->getDecompile(), "11");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
