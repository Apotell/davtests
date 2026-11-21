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

// IEEE 1800-2023 Sec 25.7 / 25.7.1: subroutines can be defined within an interface and called by a
// connected module relative to the interface port ("b.initiatorRead(raddr);").
// Source: tests/Google/chapter-25/ch25_suite2/25.7.1--if_task_in_interface.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/interface.h>
#include <hldb/method_task_call.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/ref_obj.h>
#include <hldb/task.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {

class IfTaskInInterfaceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.7.1--if_task_in_interface.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Task *getInterfaceTask() {
    const hldb::Interface *const bus = hldb::findByDefName<hldb::Interface>("simple_bus", m_design->getAllInterfaces());
    return (bus == nullptr) ? nullptr : hldb::findByName<hldb::Task>("initiatorRead", bus->getTaskFuncs());
  }
  static const hldb::Module *getCpu() { return hldb::findByDefName<hldb::Module>("cpuMod", m_design->getAllModules()); }
  // The call statement of "always @(posedge b.clk) b.initiatorRead(raddr);".
  static const hldb::MethodTaskCall *getCall() {
    const hldb::Module *const cpu = getCpu();
    if ((cpu == nullptr) || (cpu->getProcesses() == nullptr) || (cpu->getProcesses()->size() != 1)) return nullptr;
    const hldb::Always *const al = any_cast<hldb::Always>(cpu->getProcesses()->at(0));
    if (al == nullptr) return nullptr;
    const hldb::EventControl *const ec = al->getStmt<hldb::EventControl>();
    return (ec == nullptr) ? nullptr : any_cast<hldb::MethodTaskCall>(ec->getStmt());
  }
};

TEST_F(IfTaskInInterfaceTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfTaskInInterfaceTest, InterfaceDefinesTaskInitiatorRead) {
  // Lines 26-28: "task initiatorRead(input logic [7:0] raddr); ... endtask : initiatorRead"
  EXPECT_NE(getInterfaceTask(), nullptr);
}

TEST_F(IfTaskInInterfaceTest, CallIsTaskCallToInterfaceTask) {
  // Line 34: "b.initiatorRead(raddr);" calls the task defined by simple_bus.
  const hldb::MethodTaskCall *const call = getCall();
  ASSERT_NE(call, nullptr) << "line 34 is not a task call";
  EXPECT_EQ(call->getName(), std::string_view("initiatorRead"));
  ASSERT_NE(getInterfaceTask(), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getInterfaceTask());
}

TEST_F(IfTaskInInterfaceTest, CallPassesCpuModRaddr) {
  // Line 34: the actual argument raddr is the variable of cpuMod (line 32).
  const hldb::MethodTaskCall *const call = getCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = call->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg, nullptr);
  ASSERT_NE(getCpu(), nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::Variable>("raddr", getCpu()->getVariables());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(arg->getActual(), expected);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
