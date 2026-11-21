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

// IEEE 1800-2023 Sec 25.5: "Adding modports to an interface does not require that any of the
// modports be used". With no modport in the header ("module m (i2 i);") or the connection
// (".i(i)"), all nets of i2 are accessible, so "assign i.c = i.a;" is legal.
// Source: tests/Google/chapter-25/ch25_suite2/25.5--if_modport_unused.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfModportUnusedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--if_modport_unused.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getI2() {
    return hldb::findByDefName<hldb::Interface>("i2", m_design->getAllInterfaces());
  }
  static const hldb::Module *getM() { return hldb::findByDefName<hldb::Module>("m", m_design->getAllModules()); }
  static const hldb::ContAssign *getAssign() {
    const hldb::Module *const m = getM();
    if ((m == nullptr) || (m->getContAssigns() == nullptr) || (m->getContAssigns()->size() != 1)) return nullptr;
    return m->getContAssigns()->at(0);
  }
};

TEST_F(IfModportUnusedTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportUnusedTest, I2StillDeclaresTwoModports) {
  // Lines 25-26.
  const hldb::Interface *const i2 = getI2();
  ASSERT_NE(i2, nullptr);
  ASSERT_NE(i2->getModports(), nullptr);
  EXPECT_EQ(i2->getModports()->size(), 2u);
}

TEST_F(IfModportUnusedTest, HeaderNamesNoModport) {
  // Line 29: "module m (i2 i);"
  const hldb::Module *const m = getM();
  ASSERT_NE(m, nullptr);
  const hldb::Port *const p = hldb::findByName<hldb::Port>("i", m->getPorts());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getPortType(), vpiInterfacePort);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = p->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), getI2());
  EXPECT_EQ(its->getModport(), nullptr);
}

TEST_F(IfModportUnusedTest, ConnectionNamesNoModport) {
  // Line 35: "m u1(.i(i));" connects interface instance i (line 34) itself.
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const u1 = hldb::findByName<hldb::RefInstance>("u1", top->getRefInstances());
  ASSERT_NE(u1, nullptr);
  ASSERT_NE(u1->getArguments(), nullptr);
  ASSERT_EQ(u1->getArguments()->size(), 1u);
  const hldb::RefObj *const high = u1->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::RefInstance>("i", top->getRefInstances());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(high->getActual(), expected);
}

TEST_F(IfModportUnusedTest, AssignWritesI2NetC) {
  // Line 30: "assign i.c = i.a;" -- i.c reaches net c of i2 (line 24).
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(getI2(), nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::Net>("c", getI2()->getNets());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(lhs->getActual(), expected);
}

TEST_F(IfModportUnusedTest, AssignReadsI2NetA) {
  // Line 30: i.a reaches net a of i2 (line 24).
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(getI2(), nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::Net>("a", getI2()->getNets());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(rhs->getActual(), expected);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
