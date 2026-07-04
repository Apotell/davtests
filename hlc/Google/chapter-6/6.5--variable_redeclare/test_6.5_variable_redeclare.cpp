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

// Tests for 6.5--variable_redeclare.sv (tags: 6.5)
//   module top();
//     reg v;
//     wire v;
//   endmodule
//
// HLC merges reg+wire into a single Net; wire wins for vpiNetType,
// reg declaration maps to a LogicTypespec.
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'v' (vpiNetType=vpiWire, RefTypespec→LogicTypespec)
//   - work@top has no processes
//   - work@top has no continuous assignments
//
// Not checked:
//   - HLC error/warning reporting for variable redeclaration (SV spec: should fail)
//   - order-dependence (reg first vs wire first)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VariableRedeclare : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.5--variable_redeclare.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(VariableRedeclare, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net — reg v and wire v merge into a single Net named 'v'
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclare, OneNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";
  EXPECT_EQ(top->getNets()->size(), 1u) << "reg v and wire v should collapse to exactly one net";
}

TEST_F(VariableRedeclare, NetNameIsV) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr) << "net 'v' not found in module";
}

// ---------------------------------------------------------------------------
// wire wins — vpiNetType should be vpiWire (1)
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclare, NetTypeIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getNetType(), vpiWire) << "expected vpiNetType wire (1) — wire declaration wins over reg";
}

// ---------------------------------------------------------------------------
// Typespec — reg maps to LogicTypespec referenced via RefTypespec
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclare, NetHasRefTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getTypespec(), nullptr) << "net 'v' has no typespec";
}

TEST_F(VariableRedeclare, NetTypespecIsLogic) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);

  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr) << "net 'v' has no RefTypespec";

  const hldb::LogicTypespec *const lts = rts->getActual<hldb::LogicTypespec>();
  EXPECT_NE(lts, nullptr) << "RefTypespec actual is not a LogicTypespec (expected from reg declaration)";
}

// ---------------------------------------------------------------------------
// No continuous assignments — the module only has declarations, no assign
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclare, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "unexpected continuous assignments in redeclaration-only module";
}

TEST_F(VariableRedeclare, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
