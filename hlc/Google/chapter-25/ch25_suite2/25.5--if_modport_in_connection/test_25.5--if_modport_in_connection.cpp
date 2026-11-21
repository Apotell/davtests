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

// IEEE 1800-2023 Sec 25.5: the modport can instead be named in the port connection of the module
// instance ("m u1(.i(i.initiator));"), where the modport name is hierarchical from the interface
// instance. The module header ("module m (i2 i);") then names no modport.
// Source: tests/Google/chapter-25/ch25_suite2/25.5--if_modport_in_connection.sv

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

#include <string_view>

namespace hlc {

class IfModportInConnectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--if_modport_in_connection.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getI2() {
    return hldb::findByDefName<hldb::Interface>("i2", m_design->getAllInterfaces());
  }
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
  static const hldb::Modport *getModport(std::string_view name) {
    const hldb::Interface *const i2 = getI2();
    return (i2 == nullptr) ? nullptr : hldb::findByName<hldb::Modport>(name, i2->getModports());
  }
  static const hldb::RefObj *getHighConn(std::string_view instName) {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>(instName, top->getRefInstances());
    if ((inst == nullptr) || (inst->getArguments() == nullptr) || (inst->getArguments()->size() != 1)) return nullptr;
    return inst->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  }
};

TEST_F(IfModportInConnectionTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportInConnectionTest, HeadersNameI2WithoutModport) {
  // Lines 29 and 32: "module m (i2 i);" and "module s (i2 i);"
  for (std::string_view name : {"m", "s"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << name;
    const hldb::Port *const p = hldb::findByName<hldb::Port>("i", m->getPorts());
    ASSERT_NE(p, nullptr) << name;
    EXPECT_EQ(p->getPortType(), vpiInterfacePort) << name;
    ASSERT_NE(p->getTypespec(), nullptr) << name;
    const hldb::InterfaceTypespec *const its = p->getTypespec()->getActual<hldb::InterfaceTypespec>();
    ASSERT_NE(its, nullptr) << name;
    EXPECT_EQ(its->getInterface(), getI2()) << name;
    EXPECT_EQ(its->getModport(), nullptr) << name << ": the header names no modport";
  }
}

TEST_F(IfModportInConnectionTest, U1ConnectsModportInitiator) {
  // Line 37: "m u1(.i(i.initiator));"
  const hldb::RefObj *const high = getHighConn("u1");
  ASSERT_NE(high, nullptr);
  ASSERT_NE(getModport("initiator"), nullptr);
  EXPECT_EQ(high->getActual(), getModport("initiator"));
}

TEST_F(IfModportInConnectionTest, U2ConnectsModportTarget) {
  // Line 38: "s u2(.i(i.target));"
  const hldb::RefObj *const high = getHighConn("u2");
  ASSERT_NE(high, nullptr);
  ASSERT_NE(getModport("target"), nullptr);
  EXPECT_EQ(high->getActual(), getModport("target"));
}

TEST_F(IfModportInConnectionTest, ModportPathStartsAtInstanceI) {
  // Line 37: in "i.initiator" the modport name is hierarchical from interface instance i (line 36).
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::RefObj *const high = getHighConn("u1");
  ASSERT_NE(high, nullptr);
  ASSERT_NE(high->getPathElems(), nullptr);
  ASSERT_EQ(high->getPathElems()->size(), 2u);
  const hldb::RefObj *const head = any_cast<hldb::RefObj>(high->getPathElems()->at(0));
  ASSERT_NE(head, nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::RefInstance>("i", top->getRefInstances());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(head->getActual(), expected);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
