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

// IEEE 1800-2023 Sec 25.3.3: "interface p" declares a generic interface port; any interface
// instance may be connected to it ("sink u_sink(.p(u_gen));").
// Source: tests/Google/chapter-25/ch25_suite2/25.3.3--if_generic_port.sv

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

class IfGenericPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.3--if_generic_port.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfGenericPortTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfGenericPortTest, SinkPortIsInterfacePort) {
  const hldb::Module *const sink = hldb::findByDefName<hldb::Module>("sink", m_design->getAllModules());
  ASSERT_NE(sink, nullptr);
  ASSERT_NE(sink->getPorts(), nullptr);
  ASSERT_EQ(sink->getPorts()->size(), 1u);
  const hldb::Port *const p = sink->getPorts()->at(0);
  EXPECT_EQ(p->getName(), "p");
  EXPECT_EQ(p->getPortType(), vpiInterfacePort);
}

TEST_F(IfGenericPortTest, GenericPortIsNotBoundToOneInterface) {
  const hldb::Module *const sink = hldb::findByDefName<hldb::Module>("sink", m_design->getAllModules());
  ASSERT_NE(sink, nullptr);
  const hldb::Port *const p = hldb::findByName<hldb::Port>("p", sink->getPorts());
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = p->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr) << "a generic interface port is typed by an interface typespec";
  EXPECT_EQ(its->getInterface(), nullptr) << "'interface p' names no specific interface";
}

TEST_F(IfGenericPortTest, TopInstantiatesGenIf) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_gen", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = inst->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  const hldb::Any *const expected = hldb::findByDefName<hldb::Interface>("gen_if", m_design->getAllInterfaces());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(its->getInterface(), expected);
}

TEST_F(IfGenericPortTest, GenIfHasVariableS) {
  // Line 24: "logic s;" in gen_if.
  const hldb::Interface *const iface = hldb::findByDefName<hldb::Interface>("gen_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("s", iface->getVariables()), nullptr);
}

TEST_F(IfGenericPortTest, GenericPortHasNoDirection) {
  // Line 27: an interface port is declared without a direction (Sec 23.2.2.1).
  const hldb::Module *const sink = hldb::findByDefName<hldb::Module>("sink", m_design->getAllModules());
  ASSERT_NE(sink, nullptr);
  const hldb::Port *const p = hldb::findByName<hldb::Port>("p", sink->getPorts());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getDirection(), vpiNoDirection);
}

TEST_F(IfGenericPortTest, ConnectionNamesPortPAndBindsUGen) {
  // Line 32: "sink u_sink(.p(u_gen));" -- port side is sink's port p, actual is instance u_gen.
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  const hldb::Module *const sink = hldb::findByDefName<hldb::Module>("sink", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(sink, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_sink", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getArguments(), nullptr);
  ASSERT_EQ(inst->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg = inst->getArguments()->at(0);
  EXPECT_TRUE(arg->getConnByName());
  const hldb::RefObj *const low = arg->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getName(), "p");
  const hldb::Any *const expected = hldb::findByName<hldb::Port>("p", sink->getPorts());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(low->getActual(), expected);
  const hldb::RefObj *const high = arg->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr);
  EXPECT_EQ(high->getName(), "u_gen");
  const hldb::Any *const expected2 = hldb::findByName<hldb::RefInstance>("u_gen", top->getRefInstances());
  ASSERT_NE(expected2, nullptr);
  EXPECT_EQ(high->getActual(), expected2);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
