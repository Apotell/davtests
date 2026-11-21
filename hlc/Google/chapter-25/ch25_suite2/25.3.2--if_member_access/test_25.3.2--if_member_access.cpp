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

// IEEE 1800-2023 Sec 25.3.2: through a named interface port the module reaches the interface's
// members by name ("assign r = b.req;" reads req of bus_if).
// Source: tests/Google/chapter-25/ch25_suite2/25.3.2--if_member_access.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfMemberAccessTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.2--if_member_access.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getSink() { return hldb::findByDefName<hldb::Module>("sink", m_design->getAllModules()); }

  static const hldb::Variable *getReq() {
    const hldb::Interface *const iface = hldb::findByDefName<hldb::Interface>("bus_if", m_design->getAllInterfaces());
    if (iface == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("req", iface->getVariables());
  }

  static const hldb::ContAssign *getAssign() {
    const hldb::Module *const sink = getSink();
    if (sink == nullptr || sink->getContAssigns() == nullptr || sink->getContAssigns()->size() != 1) return nullptr;
    return sink->getContAssigns()->at(0);
  }
};

TEST_F(IfMemberAccessTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfMemberAccessTest, BusIfHasLogicReq) {
  // Line 24: "logic req;".
  const hldb::Variable *const req = getReq();
  ASSERT_NE(req, nullptr);
  ASSERT_NE(req->getTypespec(), nullptr);
  EXPECT_NE(req->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(IfMemberAccessTest, SinkPortBIsInterfacePort) {
  // Line 27: "module sink(bus_if b);".
  const hldb::Module *const sink = getSink();
  ASSERT_NE(sink, nullptr);
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", sink->getPorts());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getPortType(), vpiInterfacePort);
}

TEST_F(IfMemberAccessTest, SinkHasLogicVariableR) {
  // Line 28: "logic r;".
  const hldb::Module *const sink = getSink();
  ASSERT_NE(sink, nullptr);
  const hldb::Variable *const r = hldb::findByName<hldb::Variable>("r", sink->getVariables());
  ASSERT_NE(r, nullptr);
  ASSERT_NE(r->getTypespec(), nullptr);
  EXPECT_NE(r->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(IfMemberAccessTest, SinkHasOneContinuousAssign) {
  // Line 29: one "assign".
  const hldb::Module *const sink = getSink();
  ASSERT_NE(sink, nullptr);
  ASSERT_NE(sink->getContAssigns(), nullptr);
  EXPECT_EQ(sink->getContAssigns()->size(), 1u);
}

TEST_F(IfMemberAccessTest, AssignLhsIsR) {
  // Line 29: the target of the assign is r.
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::Variable>("r", getSink()->getVariables());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(lhs->getActual(), expected);
}

TEST_F(IfMemberAccessTest, AssignRhsResolvesToBusIfReq) {
  // Line 29: "b.req" names req of the interface behind port b (bus_if).
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "b.req is a reference";
  ASSERT_NE(getReq(), nullptr);
  EXPECT_EQ(rhs->getActual(), getReq());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
