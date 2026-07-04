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

// Validates the UHDM graph for a module using an inline enum with a sequence:
//   module top();
//     enum {start=10, step[10]} e;
//   endmodule
//
// Checked:
//   - design has module work@top with 1 net ('e')
//   - net 'e' RefTypespec vpiActual resolves to EnumTypespec
//   - EnumTypespec has 2 consts: "start" and "step"
//   - "start" value is stored as vpiUIntConst = "10"
//   - work@top has no processes
//
// Not checked:
//   - "step" const value — HLC normalizes step[10] to a single "step" EnumConst
//     with no explicit value stored for the sequence base

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
#include <hldb/vpi_user.h>

namespace hlc {

class EnumSequence : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19.2--enum_sequence.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumSequence, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(EnumSequence, OneNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

// ---------------------------------------------------------------------------
// Net 'e' — typespec RefTypespec → EnumTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumSequence, ENetTypespecIsEnum) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const e = hldb::findByName<hldb::Net>("e", top->getNets());
  ASSERT_NE(e, nullptr);
  EXPECT_NE(e->getTypespec()->getActual<hldb::EnumTypespec>(), nullptr)
      << "net 'e' typespec should resolve to EnumTypespec";
}

// ---------------------------------------------------------------------------
// EnumTypespec — 2 consts: "start" (value=10) and "step" (sequence base)
// ---------------------------------------------------------------------------
TEST_F(EnumSequence, EnumHasTwoConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const e = hldb::findByName<hldb::Net>("e", top->getNets());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 2u);
}

TEST_F(EnumSequence, FirstConstIsStart) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const e = hldb::findByName<hldb::Net>("e", top->getNets());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::EnumConst *const ec = enumTs->getEnumConsts()->at(0);
  ASSERT_NE(ec, nullptr);
  EXPECT_EQ(ec->getName(), "start");
}

TEST_F(EnumSequence, StartConstValueIs10) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const e = hldb::findByName<hldb::Net>("e", top->getNets());
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

TEST_F(EnumSequence, SecondConstIsStep) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const e = hldb::findByName<hldb::Net>("e", top->getNets());
  ASSERT_NE(e, nullptr);
  const hldb::EnumTypespec *const enumTs = e->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::EnumConst *const ec = enumTs->getEnumConsts()->at(1);
  ASSERT_NE(ec, nullptr);
  EXPECT_EQ(ec->getName(), "step") << "step[10] sequence base EnumConst is named 'step'";
}

TEST_F(EnumSequence, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
