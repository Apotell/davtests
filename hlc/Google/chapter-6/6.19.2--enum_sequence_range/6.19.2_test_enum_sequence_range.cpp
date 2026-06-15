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
// Checked:
//   - design has module work@top with 1 net ('e')
//   - net 'e' RefTypespec vpiActual resolves to EnumTypespec
//   - EnumTypespec has 2 consts: "start" and "stop"
//   - "start" value is stored as vpiUIntConst = "10"
//   - second const name is "stop" (not "step" — distinguishes from enum_sequence)
//   - work@top has no processes
//
// Not checked:
//   - "stop" const value — Surelog normalizes stop[11:13] to a single "stop"
//     EnumConst with no explicit value stored for the range base

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/enum_const.h>
#include <uhdm/enum_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class EnumSequenceRange : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19.2--enum_sequence_range.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(EnumSequenceRange, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(EnumSequenceRange, OneNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

// ---------------------------------------------------------------------------
// Net 'e' — typespec RefTypespec → EnumTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumSequenceRange, ENetTypespecIsEnum) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const e = uhdm::findByName<uhdm::Net>("e", top->getNets());
  ASSERT_NE(e, nullptr);
  EXPECT_NE(e->getTypespec()->getActual<uhdm::EnumTypespec>(), nullptr)
      << "net 'e' typespec should resolve to EnumTypespec";
}

// ---------------------------------------------------------------------------
// EnumTypespec — 2 consts: "start" (value=10) and "stop" (range base)
// ---------------------------------------------------------------------------
TEST_F(EnumSequenceRange, EnumHasTwoConsts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const e = uhdm::findByName<uhdm::Net>("e", top->getNets());
  ASSERT_NE(e, nullptr);
  const uhdm::EnumTypespec *const enumTs =
      e->getTypespec()->getActual<uhdm::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 2u);
}

TEST_F(EnumSequenceRange, FirstConstIsStart) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const e = uhdm::findByName<uhdm::Net>("e", top->getNets());
  ASSERT_NE(e, nullptr);
  const uhdm::EnumTypespec *const enumTs =
      e->getTypespec()->getActual<uhdm::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const uhdm::EnumConst *const ec = enumTs->getEnumConsts()->at(0);
  ASSERT_NE(ec, nullptr);
  EXPECT_EQ(ec->getName(), "start");
}

TEST_F(EnumSequenceRange, StartConstValueIs10) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const e = uhdm::findByName<uhdm::Net>("e", top->getNets());
  ASSERT_NE(e, nullptr);
  const uhdm::EnumTypespec *const enumTs =
      e->getTypespec()->getActual<uhdm::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const uhdm::EnumConst *const ec = enumTs->getEnumConsts()->at(0);
  ASSERT_NE(ec, nullptr);
  const uhdm::Constant *const val = ec->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr) << "'start' EnumConst should have an explicit value";
  EXPECT_EQ(val->getConstType(), vpiUIntConst);
  EXPECT_EQ(val->getDecompile(), "10");
}

TEST_F(EnumSequenceRange, SecondConstIsStop) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const e = uhdm::findByName<uhdm::Net>("e", top->getNets());
  ASSERT_NE(e, nullptr);
  const uhdm::EnumTypespec *const enumTs =
      e->getTypespec()->getActual<uhdm::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const uhdm::EnumConst *const ec = enumTs->getEnumConsts()->at(1);
  ASSERT_NE(ec, nullptr);
  EXPECT_EQ(ec->getName(), "stop")
      << "stop[11:13] range sequence base EnumConst is named 'stop'";
}

TEST_F(EnumSequenceRange, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
