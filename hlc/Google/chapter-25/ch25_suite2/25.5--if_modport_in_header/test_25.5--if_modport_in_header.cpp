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

// IEEE 1800-2023 Sec 25.5: in "module m (i2.initiator i);" the interface name selects the interface
// and the modport name selects the directional information for the port. The instances connect
// the plain interface instance i ("m u1(.i(i));").
// Source: tests/Google/chapter-25/ch25_suite2/25.5--if_modport_in_header.sv

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

class IfModportInHeaderTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--if_modport_in_header.hlc"}); }
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
  static const hldb::InterfaceTypespec *getPortTypespec(std::string_view module) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    const hldb::Port *const p = hldb::findByName<hldb::Port>("i", m->getPorts());
    if ((p == nullptr) || (p->getTypespec() == nullptr)) return nullptr;
    return p->getTypespec()->getActual<hldb::InterfaceTypespec>();
  }
};

TEST_F(IfModportInHeaderTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportInHeaderTest, HeaderPortsAreModportPorts) {
  // Lines 29 and 32: "i2.initiator i" and "i2.target i" name a modport, so the port type is
  // vpiModportPort rather than vpiInterfacePort (Sec 37.13, vpiPortType).
  for (std::string_view name : {"m", "s"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << name;
    ASSERT_NE(m->getPorts(), nullptr) << name;
    ASSERT_EQ(m->getPorts()->size(), 1u) << name;
    EXPECT_EQ(m->getPorts()->at(0)->getName(), std::string_view("i")) << name;
    EXPECT_EQ(m->getPorts()->at(0)->getPortType(), vpiModportPort) << name;
  }
}

TEST_F(IfModportInHeaderTest, ModuleMSelectsI2Initiator) {
  // Line 29: "module m (i2.initiator i);"
  const hldb::InterfaceTypespec *const its = getPortTypespec("m");
  ASSERT_NE(its, nullptr) << "port i of m is not typed by an interface";
  EXPECT_EQ(its->getInterface(), getI2());
  ASSERT_NE(getModport("initiator"), nullptr);
  EXPECT_EQ(its->getModport(), getModport("initiator"));
}

TEST_F(IfModportInHeaderTest, ModuleSSelectsI2Target) {
  // Line 32: "module s (i2.target i);"
  const hldb::InterfaceTypespec *const its = getPortTypespec("s");
  ASSERT_NE(its, nullptr) << "port i of s is not typed by an interface";
  EXPECT_EQ(its->getInterface(), getI2());
  ASSERT_NE(getModport("target"), nullptr);
  EXPECT_EQ(its->getModport(), getModport("target"));
}

TEST_F(IfModportInHeaderTest, ConnectionsBindInterfaceInstanceI) {
  // Lines 36-38: "i2 i();", "m u1(.i(i));", "s u2(.i(i));" connect the instance, not a modport.
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const i = hldb::findByName<hldb::RefInstance>("i", top->getRefInstances());
  ASSERT_NE(i, nullptr);
  for (std::string_view instName : {"u1", "u2"}) {
    const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>(instName, top->getRefInstances());
    ASSERT_NE(inst, nullptr) << instName;
    ASSERT_NE(inst->getArguments(), nullptr) << instName;
    ASSERT_EQ(inst->getArguments()->size(), 1u) << instName;
    const hldb::RefObj *const high = inst->getArguments()->at(0)->getHighConn<hldb::RefObj>();
    ASSERT_NE(high, nullptr) << instName;
    EXPECT_EQ(high->getActual(), i) << instName;
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
