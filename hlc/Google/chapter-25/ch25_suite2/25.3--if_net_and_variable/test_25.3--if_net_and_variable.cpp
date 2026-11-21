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

// IEEE 1800-2023 Sec 25.3: an interface can contain nets and variables, like a module.
// "wire w;" is a net of type wire, "logic v;" is a variable (Sec 6.5, 6.7, 6.8).
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_net_and_variable.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfNetAndVariableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_net_and_variable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("nv_if", m_design->getAllInterfaces());
  }
};

TEST_F(IfNetAndVariableTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfNetAndVariableTest, WIsWireNet) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("w", iface->getNets());
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getNetType(), vpiWire);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("w", iface->getVariables()), nullptr);
}

TEST_F(IfNetAndVariableTest, VIsVariable) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("v", iface->getVariables()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("v", iface->getNets()), nullptr);
}

TEST_F(IfNetAndVariableTest, ExactlyOneNetAndOneVariable) {
  // Lines 24-25 are the only items.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getNets(), nullptr);
  ASSERT_NE(iface->getVariables(), nullptr);
  EXPECT_EQ(iface->getNets()->size(), 1u);
  EXPECT_EQ(iface->getVariables()->size(), 1u);
}

TEST_F(IfNetAndVariableTest, VIsLogic) {
  // Line 25: "logic v;".
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", iface->getVariables());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  EXPECT_NE(v->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(IfNetAndVariableTest, WDataTypeDefaultsToLogic) {
  // Line 24: Sec 6.7.1 -- a net declared without a data type has data type logic.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("w", iface->getNets());
  ASSERT_NE(w, nullptr);
  ASSERT_NE(w->getTypespec(), nullptr);
  EXPECT_NE(w->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
