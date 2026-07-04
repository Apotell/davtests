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

// Validates the UHDM graph for a module with an anonymous enum:
//   module top();
//     enum {a, b, c} val;
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has 1 typespec: anonymous EnumTypespec (no TypedefTypespec wrapper)
//   - anonymous EnumTypespec has 3 consts: a, b, c
//   - work@top has no processes
//   - net "val" exists
//   - net "val" RefTypespec vpiActual resolves directly to EnumTypespec
//     (not through a TypedefTypespec)
//   - net "val" has no initial value
//
// Not checked:
//   - enum base type (default logic in SV)
//   - enum const default values (a=0, b=1, c=2) — HLC may not store implicit values

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class EnumAnon : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19--enum_anon.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumAnon, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module has 1 typespec: anonymous EnumTypespec (no TypedefTypespec wrapper)
// ---------------------------------------------------------------------------
TEST_F(EnumAnon, ModuleHasOneTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 1u);
}

TEST_F(EnumAnon, TypespecIsAnonymousEnum) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *const enumTs = any_cast<hldb::EnumTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(enumTs, nullptr) << "anonymous enum has EnumTypespec directly, no TypedefTypespec";
}

TEST_F(EnumAnon, EnumHasThreeConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *const enumTs = any_cast<hldb::EnumTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 3u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "a");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "b");
  EXPECT_EQ(enumTs->getEnumConsts()->at(2)->getName(), "c");
}

// ---------------------------------------------------------------------------
// No processes (no initial/always block)
// ---------------------------------------------------------------------------
TEST_F(EnumAnon, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Module-level Net "val" → EnumTypespec directly (not via TypedefTypespec)
// ---------------------------------------------------------------------------
TEST_F(EnumAnon, NetValExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const val = hldb::findByName<hldb::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
}

TEST_F(EnumAnon, NetValTypespecIsEnumDirectly) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const val = hldb::findByName<hldb::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  const hldb::RefTypespec *const rts = val->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::EnumTypespec>(), nullptr)
      << "anonymous enum: net's typespec resolves to EnumTypespec directly";
}

TEST_F(EnumAnon, NetValHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const val = hldb::findByName<hldb::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<hldb::Any>(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
