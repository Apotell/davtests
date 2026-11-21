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

// IEEE 1800-2023 Sec 25.3: interface items include continuous assignments, like module items
// ("assign y = ~a;" inside ca_if).
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_cont_assign_item.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfContAssignItemTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_cont_assign_item.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("ca_if", m_design->getAllInterfaces());
  }

  static const hldb::ContAssign *getAssign() {
    const hldb::Interface *const iface = getInterface();
    if (iface == nullptr || iface->getContAssigns() == nullptr || iface->getContAssigns()->size() != 1) return nullptr;
    return iface->getContAssigns()->at(0);
  }
};

TEST_F(IfContAssignItemTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfContAssignItemTest, AIsLogicVariable) {
  // Line 24: "logic a;".
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", iface->getVariables());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getTypespec(), nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(IfContAssignItemTest, YIsWireNet) {
  // Line 25: "wire y;".
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const y = hldb::findByName<hldb::Net>("y", iface->getNets());
  ASSERT_NE(y, nullptr);
  EXPECT_EQ(y->getNetType(), vpiWire);
}

TEST_F(IfContAssignItemTest, InterfaceHasOneContinuousAssign) {
  // Line 26: one "assign".
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  EXPECT_EQ(iface->getContAssigns()->size(), 1u);
}

TEST_F(IfContAssignItemTest, LhsIsNetY) {
  // Line 26: "assign y = ...".
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::Net>("y", getInterface()->getNets());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(lhs->getActual(), expected);
}

TEST_F(IfContAssignItemTest, RhsIsBitwiseNotOfA) {
  // Line 26: "~a" -- unary bitwise negation of variable a.
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const op = ca->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiBitNegOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const hldb::RefObj *const a = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(a, nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::Variable>("a", getInterface()->getVariables());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(a->getActual(), expected);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
