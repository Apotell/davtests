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

// IEEE 1800-2023 Sec 25.3.2 with Sec 23.3.2.1: an interface port can be connected by position.
// "sink u_sink(sb);" binds the interface instance sb to sink's first (only) port b.
// Source: tests/Google/chapter-25/ch25_suite2/25.3.2--if_ordered_connection.sv

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
#include <hldb/vpi_user.h>

namespace hlc {

class IfOrderedConnectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.2--if_ordered_connection.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::RefInstance *getTopInstance(std::string_view name) {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(name, top->getRefInstances());
  }

  static const hldb::NamedArgument *getOnlyArgument() {
    const hldb::RefInstance *const inst = getTopInstance("u_sink");
    if (inst == nullptr || inst->getArguments() == nullptr || inst->getArguments()->size() != 1) return nullptr;
    return inst->getArguments()->at(0);
  }
};

TEST_F(IfOrderedConnectionTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfOrderedConnectionTest, SinkPortBIsInterfacePort) {
  // Line 27: "module sink(bus_if b);".
  const hldb::Module *const sink = getModule("sink");
  ASSERT_NE(sink, nullptr);
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", sink->getPorts());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getPortType(), vpiInterfacePort);
}

TEST_F(IfOrderedConnectionTest, TopHasTwoInstances) {
  // Lines 31-32: sb and u_sink.
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 2u);
}

TEST_F(IfOrderedConnectionTest, SbIsBusIfInstance) {
  // Line 31: "bus_if sb();".
  const hldb::RefInstance *const sb = getTopInstance("sb");
  ASSERT_NE(sb, nullptr);
  ASSERT_NE(sb->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = sb->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), hldb::findByDefName<hldb::Interface>("bus_if", m_design->getAllInterfaces()));
}

TEST_F(IfOrderedConnectionTest, ConnectionIsPositional) {
  // Line 32: "(sb)" -- one connection, neither named nor wildcard.
  const hldb::NamedArgument *const arg = getOnlyArgument();
  ASSERT_NE(arg, nullptr) << "u_sink has exactly one connection";
  EXPECT_FALSE(arg->getConnByName());
  EXPECT_FALSE(arg->getWildcardConn());
}

TEST_F(IfOrderedConnectionTest, FirstPositionIsPortB) {
  // Line 32: position 0 is sink's port b.
  const hldb::NamedArgument *const arg = getOnlyArgument();
  ASSERT_NE(arg, nullptr);
  const hldb::RefObj *const low = arg->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getActual(), hldb::findByName<hldb::Port>("b", getModule("sink")->getPorts()));
}

TEST_F(IfOrderedConnectionTest, ActualIsInstanceSb) {
  // Line 32: the expression "sb" is the interface instance from line 31.
  const hldb::NamedArgument *const arg = getOnlyArgument();
  ASSERT_NE(arg, nullptr);
  const hldb::RefObj *const high = arg->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr);
  EXPECT_EQ(high->getName(), "sb");
  EXPECT_EQ(high->getActual(), getTopInstance("sb"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
