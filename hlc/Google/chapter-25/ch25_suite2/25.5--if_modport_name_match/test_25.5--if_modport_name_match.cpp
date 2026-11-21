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

// IEEE 1800-2023 Sec 25.5: a modport may be named in both the module header ("i2.initiator i") and
// the port connection (".i(i.initiator)") as long as the two names are identical.
// Source: tests/Google/chapter-25/ch25_suite2/25.5--if_modport_name_match.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfModportNameMatchTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--if_modport_name_match.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Modport *getInitiator() {
    const hldb::Interface *const i2 = hldb::findByDefName<hldb::Interface>("i2", m_design->getAllInterfaces());
    return (i2 == nullptr) ? nullptr : hldb::findByName<hldb::Modport>("initiator", i2->getModports());
  }
};

TEST_F(IfModportNameMatchTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportNameMatchTest, HeaderSelectsInitiator) {
  // Line 30: "module m (i2.initiator i);"
  const hldb::Module *const m = hldb::findByDefName<hldb::Module>("m", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  const hldb::Port *const p = hldb::findByName<hldb::Port>("i", m->getPorts());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getPortType(), vpiModportPort);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = p->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  ASSERT_NE(getInitiator(), nullptr);
  EXPECT_EQ(its->getModport(), getInitiator());
}

TEST_F(IfModportNameMatchTest, ConnectionSelectsInitiator) {
  // Line 35: "m u1(.i(i.initiator));"
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const u1 = hldb::findByName<hldb::RefInstance>("u1", top->getRefInstances());
  ASSERT_NE(u1, nullptr);
  ASSERT_NE(u1->getArguments(), nullptr);
  ASSERT_EQ(u1->getArguments()->size(), 1u);
  const hldb::RefObj *const high = u1->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr);
  ASSERT_NE(getInitiator(), nullptr);
  EXPECT_EQ(high->getActual(), getInitiator());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
