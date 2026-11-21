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

// IEEE 1800-2023 Sec 25.3.2 with Sec 23.3.2.4: ".*" connects every port to the identically named
// item in the instantiating scope -- here port b to top's interface instance b.
// Source: tests/Google/chapter-25/ch25_suite2/25.3.2--if_dot_star_connection.sv

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

#include <vector>

namespace hlc {

class IfDotStarConnectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.2--if_dot_star_connection.hlc"}); }
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

  // The connections of u_sink that were made by ".*".
  static std::vector<const hldb::NamedArgument *> getWildcardConnections() {
    std::vector<const hldb::NamedArgument *> result;
    const hldb::RefInstance *const inst = getTopInstance("u_sink");
    if (inst == nullptr || inst->getArguments() == nullptr) return result;
    for (const hldb::NamedArgument *const arg : *inst->getArguments()) {
      if (arg->getWildcardConn()) result.emplace_back(arg);
    }
    return result;
  }
};

TEST_F(IfDotStarConnectionTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfDotStarConnectionTest, SinkPortBIsInterfacePort) {
  // Line 27: "module sink(bus_if b);".
  const hldb::Module *const sink = getModule("sink");
  ASSERT_NE(sink, nullptr);
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", sink->getPorts());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getPortType(), vpiInterfacePort);
}

TEST_F(IfDotStarConnectionTest, TopHasTwoInstances) {
  // Lines 31-32: b and u_sink.
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 2u);
}

TEST_F(IfDotStarConnectionTest, BIsBusIfInstance) {
  // Line 31: "bus_if b();".
  const hldb::RefInstance *const b = getTopInstance("b");
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = b->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), hldb::findByDefName<hldb::Interface>("bus_if", m_design->getAllInterfaces()));
}

TEST_F(IfDotStarConnectionTest, WildcardMakesOneConnectionForPortB) {
  // Line 32: ".*" -- sink has one port, so exactly one connection, made by name.
  const std::vector<const hldb::NamedArgument *> conns = getWildcardConnections();
  ASSERT_EQ(conns.size(), 1u);
  EXPECT_TRUE(conns[0]->getConnByName());
  const hldb::RefObj *const low = conns[0]->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getName(), "b");
  EXPECT_EQ(low->getActual(), hldb::findByName<hldb::Port>("b", getModule("sink")->getPorts()));
}

TEST_F(IfDotStarConnectionTest, ImpliedActualIsInstanceB) {
  // Line 32: port b binds top's instance b (line 31).
  const std::vector<const hldb::NamedArgument *> conns = getWildcardConnections();
  ASSERT_EQ(conns.size(), 1u);
  const hldb::RefObj *const high = conns[0]->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr);
  EXPECT_EQ(high->getActual(), getTopInstance("b"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
