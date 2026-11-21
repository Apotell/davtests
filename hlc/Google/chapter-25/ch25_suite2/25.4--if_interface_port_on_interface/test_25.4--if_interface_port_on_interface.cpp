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

// IEEE 1800-2023 Sec 25.4 (with A.1.3): an interface port list may contain interface ports, so an
// interface can take another interface ("interface outer_if(inner_if i);").
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_interface_port_on_interface.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfInterfacePortOnInterfaceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_interface_port_on_interface.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface(std::string_view name) {
    return hldb::findByDefName<hldb::Interface>(name, m_design->getAllInterfaces());
  }

  static const hldb::RefInstance *getTopInstance(std::string_view name) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(name, top->getRefInstances());
  }

  static const hldb::Port *getPortI() {
    const hldb::Interface *const outer = getInterface("outer_if");
    if (outer == nullptr) return nullptr;
    return hldb::findByName<hldb::Port>("i", outer->getPorts());
  }
};

TEST_F(IfInterfacePortOnInterfaceTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfInterfacePortOnInterfaceTest, InnerHasVariableS) {
  // Line 24: "logic s;".
  const hldb::Interface *const inner = getInterface("inner_if");
  ASSERT_NE(inner, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("s", inner->getVariables()), nullptr);
}

TEST_F(IfInterfacePortOnInterfaceTest, OuterHasOneInterfacePortI) {
  // Line 27: "interface outer_if(inner_if i);".
  const hldb::Interface *const outer = getInterface("outer_if");
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getPorts(), nullptr);
  ASSERT_EQ(outer->getPorts()->size(), 1u);
  const hldb::Port *const i = getPortI();
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getPortType(), vpiInterfacePort);
  EXPECT_EQ(i->getDirection(), vpiNoDirection);
}

TEST_F(IfInterfacePortOnInterfaceTest, PortITypedByInnerIf) {
  // Line 27: the port type is inner_if.
  const hldb::Port *const i = getPortI();
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = i->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), getInterface("inner_if"));
}

TEST_F(IfInterfacePortOnInterfaceTest, UOutConnectsIToUIn) {
  // Lines 31-32: "inner_if u_in();" and "outer_if u_out(.i(u_in));".
  const hldb::RefInstance *const out = getTopInstance("u_out");
  ASSERT_NE(out, nullptr);
  ASSERT_NE(out->getArguments(), nullptr);
  ASSERT_EQ(out->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg = out->getArguments()->at(0);
  const hldb::RefObj *const low = arg->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getActual(), getPortI());
  const hldb::RefObj *const high = arg->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr);
  EXPECT_EQ(high->getActual(), getTopInstance("u_in"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
