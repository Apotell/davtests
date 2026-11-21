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

// IEEE 1800-2023 Sec 25.3 / A.1.2: "endinterface [ : interface_identifier ]" -- the optional end
// label repeats the interface name.
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_end_label.sv

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

namespace hlc {

class IfEndLabelTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_end_label.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("lbl_if", m_design->getAllInterfaces());
  }
};

TEST_F(IfEndLabelTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfEndLabelTest, EndLabelMatchesName) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  EXPECT_EQ(iface->getEndLabel(), "lbl_if");
}

TEST_F(IfEndLabelTest, InterfaceIsDeclared) {
  // Line 23: "interface lbl_if;".
  EXPECT_NE(getInterface(), nullptr);
}

TEST_F(IfEndLabelTest, BodyHasOnlyVariableS) {
  // Line 24: "logic s;" is the single item.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getVariables(), nullptr);
  EXPECT_EQ(iface->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("s", iface->getVariables()), nullptr);
  EXPECT_TRUE(iface->getNets() == nullptr || iface->getNets()->empty()) << "'logic s;' is a variable, not a net";
}

TEST_F(IfEndLabelTest, SIsLogic) {
  // Line 24: the data type of s is logic.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", iface->getVariables());
  ASSERT_NE(s, nullptr);
  ASSERT_NE(s->getTypespec(), nullptr);
  EXPECT_NE(s->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
