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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Behavior.sv,
// split out of the combined NetsAndVariablesAnsi.sv suite so continuous
// assignments and process coverage stand on their own.
//
// Checked:
//   - continuous assignments -- 6 total, in source order: implicit_wire = a |
//     b; implicit_net_a = a & b; implicit_net_b = implicit_net_a | a; w0 = a
//     & b; w_bus[0] = a; uwire_net = a ^ b
//   - processes -- always_comb, always_ff, always_latch, initial, always = 5
//     total

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/assignment.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiBehaviorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Behavior.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("nets_and_variables_test", m_design->getAllModules());
  }
};

// ---------------------------------------------------------------------------
// Continuous assignments -- 6 total, in source order:
//   implicit_wire = a | b; implicit_net_a = a & b;
//   implicit_net_b = implicit_net_a | a; w0 = a & b; w_bus[0] = a;
//   uwire_net = a ^ b
// ---------------------------------------------------------------------------
TEST_F(AnsiBehaviorTest, SixContAssignsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 6u);
}

TEST_F(AnsiBehaviorTest, FirstContAssignDrivesImplicitWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_FALSE(top->getContAssigns()->empty());
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "implicit_wire");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'implicit_wire' is formally declared";
}

TEST_F(AnsiBehaviorTest, SecondContAssignDrivesImplicitNetA) {
  // Per IEEE 1800 clause 6.10, an undeclared identifier used as a
  // continuous-assignment LHS (with no `default_nettype override, i.e.
  // plain 'wire' applies) is a legally-implicit wire net, not an error.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 2u);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(1)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "implicit_net_a");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'implicit_net_a' is a legally-implicit wire net";
}

TEST_F(AnsiBehaviorTest, LastContAssignDrivesUwireNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_FALSE(top->getContAssigns()->empty());
  const hldb::RefObj *const lhs = top->getContAssigns()->back()->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "uwire_net");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

// ---------------------------------------------------------------------------
// Processes -- always_comb, always_ff, always_latch, initial, always = 5
// total.
// ---------------------------------------------------------------------------
TEST_F(AnsiBehaviorTest, FiveProcessesExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 5u);
}

TEST_F(AnsiBehaviorTest, ProcessTypeCounts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  int initialCount = 0;
  int alwaysCount = 0;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (any_cast<hldb::Initial>(p) != nullptr) initialCount++;
    if (any_cast<hldb::Always>(p) != nullptr) alwaysCount++;
  }
  EXPECT_EQ(initialCount, 1) << "one initial block (var_real/var_string)";
  EXPECT_EQ(alwaysCount, 4) << "always_comb, always_ff, always_latch, always";
}

TEST_F(AnsiBehaviorTest, PlainAlwaysDrivesYFromVarLogic) {
  // Identify the final "always @(posedge clk) y <= var_logic;" block
  // structurally (by its Assignment LHS), rather than assuming process order.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Always *found = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    const hldb::Always *const a = any_cast<hldb::Always>(p);
    if (a == nullptr) continue;
    const hldb::EventControl *const ec = a->getStmt<hldb::EventControl>();
    if (ec == nullptr) continue;
    const hldb::Begin *const begin = ec->getStmt<hldb::Begin>();
    if (begin == nullptr) continue;
    const hldb::AnyCollection *const stmts = begin->getStmts();
    if (stmts == nullptr) continue;
    for (const hldb::Any *stmt : *stmts) {
      const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
      if (assign == nullptr) continue;
      const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
      if (lhs != nullptr && lhs->getName() == "y") {
        found = a;
        break;
      }
      if (found != nullptr) break;
    }
  }
  ASSERT_NE(found, nullptr) << "could not find 'always @(posedge clk) y <= var_logic;'";
  EXPECT_EQ(found->getAlwaysType(), vpiAlways);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
