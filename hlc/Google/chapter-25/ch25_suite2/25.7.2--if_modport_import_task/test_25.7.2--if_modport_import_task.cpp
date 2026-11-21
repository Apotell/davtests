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

// IEEE 1800-2023 Sec 25.7 / 25.7.2: tasks defined in the interface are declared as import tasks in
// a modport, here by identifier only ("import targetRead, targetWrite"). memMod, connected through
// modport target, calls the imported tasks.
// Source: tests/Google/chapter-25/ch25_suite2/25.7.2--if_modport_import_task.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/if_else.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/method_task_call.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/task.h>

#include <string_view>

namespace hlc {

class IfModportImportTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.7.2--if_modport_import_task.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getBus() {
    return hldb::findByDefName<hldb::Interface>("simple_bus", m_design->getAllInterfaces());
  }
  static const hldb::Task *getTask(std::string_view name) {
    const hldb::Interface *const bus = getBus();
    return (bus == nullptr) ? nullptr : hldb::findByName<hldb::Task>(name, bus->getTaskFuncs());
  }
  static const hldb::IODecl *getModportItem(std::string_view mpName, std::string_view item) {
    const hldb::Interface *const bus = getBus();
    if (bus == nullptr) return nullptr;
    const hldb::Modport *const mp = hldb::findByName<hldb::Modport>(mpName, bus->getModports());
    return (mp == nullptr) ? nullptr : hldb::findByName<hldb::IODecl>(item, mp->getIODecls());
  }
  // The if/else of "always @(posedge a.clk) if (a.req) a.targetRead; else a.targetWrite;".
  static const hldb::IfElse *getIfElse() {
    const hldb::Module *const mem = hldb::findByDefName<hldb::Module>("memMod", m_design->getAllModules());
    if ((mem == nullptr) || (mem->getProcesses() == nullptr) || (mem->getProcesses()->size() != 1)) return nullptr;
    const hldb::Always *const al = any_cast<hldb::Always>(mem->getProcesses()->at(0));
    if (al == nullptr) return nullptr;
    const hldb::EventControl *const ec = al->getStmt<hldb::EventControl>();
    return (ec == nullptr) ? nullptr : any_cast<hldb::IfElse>(ec->getStmt());
  }
};

TEST_F(IfModportImportTaskTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportImportTaskTest, TargetImportsTargetReadAndTargetWrite) {
  // Line 26: "import targetRead, targetWrite);"
  for (std::string_view name : {"targetRead", "targetWrite"}) {
    const hldb::IODecl *const io = getModportItem("target", name);
    ASSERT_NE(io, nullptr) << name;
    EXPECT_EQ(io->getImportExport(), vpiImported) << name;
  }
}

TEST_F(IfModportImportTaskTest, InitiatorImportsInitiatorRead) {
  // Line 28: "import initiatorRead);"
  const hldb::IODecl *const io = getModportItem("initiator", "initiatorRead");
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getImportExport(), vpiImported);
}

TEST_F(IfModportImportTaskTest, TargetDoesNotListInitiatorRead) {
  // Line 25-26: modport target lists no initiatorRead.
  EXPECT_EQ(getModportItem("target", "initiatorRead"), nullptr);
}

TEST_F(IfModportImportTaskTest, MemModCallsImportedTargetRead) {
  // Line 39: "if (a.req) a.targetRead;" calls the interface task of lines 31-32.
  const hldb::IfElse *const ie = getIfElse();
  ASSERT_NE(ie, nullptr);
  const hldb::MethodTaskCall *const call = any_cast<hldb::MethodTaskCall>(ie->getStmt());
  ASSERT_NE(call, nullptr);
  ASSERT_NE(getTask("targetRead"), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getTask("targetRead"));
}

TEST_F(IfModportImportTaskTest, MemModCallsImportedTargetWrite) {
  // Line 40: "else a.targetWrite;" calls the interface task of lines 33-34.
  const hldb::IfElse *const ie = getIfElse();
  ASSERT_NE(ie, nullptr);
  const hldb::MethodTaskCall *const call = any_cast<hldb::MethodTaskCall>(ie->getElseStmt());
  ASSERT_NE(call, nullptr);
  ASSERT_NE(getTask("targetWrite"), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getTask("targetWrite"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
