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
// What to check and why (IEEE 1800-2023 6.19.2 "Enumerated type
// ranges", p.121): "name[N:M] ... Creates a sequence of named
// constants starting with nameN and incrementing or decrementing until
// reaching named constant nameM." "enum {start=10, stop[11:13]} e;" is
// exactly this legal range syntax. No :should_fail_because: tag.
//   Also (6.8): "e" declared at module scope must be a Variable, not a
//   Net (fixed from a prior Net-based version).
//
// Checked:
//   - design has module top with 1 variable ('e')
//   - variable 'e' RefTypespec vpiActual resolves to EnumTypespec
//   - EnumTypespec has 2 consts: "start" and "stop"
//   - "start" value is stored as vpiUIntConst = "10"
//   - second const name is "stop" (not "step" -- distinguishes from enum_sequence)
//   - top has no processes
//   - "stop" const has no explicit value stored (HLC normalizes stop[11:13] to
//     a single "stop" EnumConst with no explicit value for the range base)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumSequenceRangeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19.2--enum_sequence_range.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumSequenceRangeTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(EnumSequenceRangeTest, OneVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

// ---------------------------------------------------------------------------
// Variable 'e' -- typespec RefTypespec -> EnumTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumSequenceRangeTest, EVariableTypespecIsEnum) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  EXPECT_NE(e->getTypespec()->getActual<hldb::EnumTypespec>(), nullptr)
      << "variable 'e' typespec should resolve to EnumTypespec";
}

// ---------------------------------------------------------------------------
// EnumTypespec -- 2 consts: "start" (value=10) and "stop" (range base)
// ---------------------------------------------------------------------------
TEST_F(EnumSequenceRangeTest, EnumHasTwoConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 2u);
}

TEST_F(EnumSequenceRangeTest, FirstConstIsStart) {
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

TEST_F(EnumSequenceRangeTest, StartConstValueIs10) {
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

TEST_F(EnumSequenceRangeTest, SecondConstIsStop) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::EnumConst *const ec = enumTs->getEnumConsts()->at(1);
  ASSERT_NE(ec, nullptr);
  EXPECT_EQ(ec->getName(), "stop") << "stop[11:13] range sequence base EnumConst is named 'stop'";
}

TEST_F(EnumSequenceRangeTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(EnumSequenceRangeTest, StopConstHasNoExplicitValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const e = hldb::findByName<hldb::Variable>("e", top->getVariables());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::EnumConst *const ec = enumTs->getEnumConsts()->at(1);
  ASSERT_NE(ec, nullptr);
  EXPECT_EQ(ec->getValue<hldb::Constant>(), nullptr)
      << "stop[11:13] range base has no explicit value stored on the EnumConst";
}

TEST_F(EnumSequenceRangeTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
