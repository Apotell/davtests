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

// Tests for tests/ElabSysCall/dut.sv -- the four elaboration system tasks
// (IEEE 1800-2023 20.11 "Elaboration system tasks"): '$fatal', '$error',
// '$warning', '$info'. Each is used exactly as 20.11 describes: directly as
// a module item inside a generate-if body (no enclosing initial/always
// procedure), which is legal specifically because these four tasks are
// evaluated during elaboration rather than simulation.
//
//   module top_fatal();
//   if (1) begin
//     $fatal("Cache way size must be at most 4kB");
//   end
//   endmodule
//
//   (and identically shaped top_error/$error, top_warning/$warning,
//    top_info/$info)
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 20.11: '$fatal', '$error', '$warning', '$info' may appear
// directly within a module, interface, program, or generate block (i.e.
// outside any procedural block), and are each a system task call taking an
// optional finish_number ('$fatal' only) followed by a format/message
// argument list -- here a single string-literal argument.
// IEEE 1800-2023 27.5 "Generate-if constructs": 'if (1) begin ... end' is a
// GenIf whose condition is the constant expression '1' and whose body is an
// anonymous generate block (Begin).
//
// This file checks only the structural shape of the parsed elaboration
// system-task calls (name, single string argument). Whether HLC's
// elaboration pass actually surfaces '$fatal'/'$error'/'$warning'/'$info'
// as compiler diagnostics (vs. purely structural SysTaskCall nodes) is not
// asserted here -- confirming the exact ErrorDefinition code(s), if any,
// would require guessing rather than reading the standard or the headers,
// which the guide directs against.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ElabSysCallTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ElabSysCall.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::GenIf *findGenIf(const hldb::Module *m) {
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(stmt)) return gi;
    }
    return nullptr;
  }

  // Finds the (single) SysTaskCall statement inside the module's generate-if
  // body, whether the body is a bare Begin{stmts} block or (defensively)
  // the SysTaskCall itself with no wrapping block.
  static const hldb::SysTaskCall *findSysTaskCallInGenIf(const hldb::Module *m) {
    const hldb::GenIf *const gi = findGenIf(m);
    if (gi == nullptr) return nullptr;
    if (const hldb::SysTaskCall *const direct = gi->getStmt<hldb::SysTaskCall>()) return direct;
    const hldb::Begin *const body = gi->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const s : *body->getStmts()) {
      if (const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(s)) return call;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

TEST_F(ElabSysCallTest, ModulesExist) {
  EXPECT_NE(getModule("top_fatal"), nullptr) << "module 'top_fatal' not found";
  EXPECT_NE(getModule("top_error"), nullptr) << "module 'top_error' not found";
  EXPECT_NE(getModule("top_warning"), nullptr) << "module 'top_warning' not found";
  EXPECT_NE(getModule("top_info"), nullptr) << "module 'top_info' not found";
}

// ---------------------------------------------------------------------------
// 'if (1) begin ... end' -- generate-if with a literal-true condition,
// common to all four modules.
// ---------------------------------------------------------------------------

TEST_F(ElabSysCallTest, EachModuleHasGenerateIfWithConstantTrueCondition) {
  const char *const names[4] = {"top_fatal", "top_error", "top_warning", "top_info"};
  for (const char *const name : names) {
    SCOPED_TRACE(name);
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr);
    ASSERT_NE(m->getGenStmts(), nullptr) << "'" << name << "' has no generate statements";
    const hldb::GenIf *const gi = findGenIf(m);
    ASSERT_NE(gi, nullptr) << "no GenIf found in '" << name << "'";
    const hldb::Constant *const cond = gi->getCondition<hldb::Constant>();
    ASSERT_NE(cond, nullptr) << "'if (1)': condition must be a Constant";
    EXPECT_EQ(std::string(cond->getDecompile()), "1");
  }
}

// ---------------------------------------------------------------------------
// '$fatal("...")' -- top_fatal
// ---------------------------------------------------------------------------

TEST_F(ElabSysCallTest, TopFatal_HasFatalSysTaskCallWithOneStringArgument) {
  const hldb::Module *const m = getModule("top_fatal");
  ASSERT_NE(m, nullptr);
  const hldb::SysTaskCall *const call = findSysTaskCallInGenIf(m);
  ASSERT_NE(call, nullptr) << "'$fatal(...)' SysTaskCall not found inside top_fatal's generate-if body";
  EXPECT_EQ(call->getName(), "$fatal");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'$fatal(\"...\")' called with a single message argument";
  EXPECT_NE(any_cast<hldb::Constant>(call->getArguments()->at(0)), nullptr)
      << "'$fatal's message argument must be a (string) Constant";
}

// ---------------------------------------------------------------------------
// '$error("...")' -- top_error
// ---------------------------------------------------------------------------

TEST_F(ElabSysCallTest, TopError_HasErrorSysTaskCallWithOneStringArgument) {
  const hldb::Module *const m = getModule("top_error");
  ASSERT_NE(m, nullptr);
  const hldb::SysTaskCall *const call = findSysTaskCallInGenIf(m);
  ASSERT_NE(call, nullptr) << "'$error(...)' SysTaskCall not found inside top_error's generate-if body";
  EXPECT_EQ(call->getName(), "$error");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Constant>(call->getArguments()->at(0)), nullptr)
      << "'$error's message argument must be a (string) Constant";
}

// ---------------------------------------------------------------------------
// '$warning("...")' -- top_warning
// ---------------------------------------------------------------------------

TEST_F(ElabSysCallTest, TopWarning_HasWarningSysTaskCallWithOneStringArgument) {
  const hldb::Module *const m = getModule("top_warning");
  ASSERT_NE(m, nullptr);
  const hldb::SysTaskCall *const call = findSysTaskCallInGenIf(m);
  ASSERT_NE(call, nullptr) << "'$warning(...)' SysTaskCall not found inside top_warning's generate-if body";
  EXPECT_EQ(call->getName(), "$warning");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Constant>(call->getArguments()->at(0)), nullptr)
      << "'$warning's message argument must be a (string) Constant";
}

// ---------------------------------------------------------------------------
// '$info("...")' -- top_info
// ---------------------------------------------------------------------------

TEST_F(ElabSysCallTest, TopInfo_HasInfoSysTaskCallWithOneStringArgument) {
  const hldb::Module *const m = getModule("top_info");
  ASSERT_NE(m, nullptr);
  const hldb::SysTaskCall *const call = findSysTaskCallInGenIf(m);
  ASSERT_NE(call, nullptr) << "'$info(...)' SysTaskCall not found inside top_info's generate-if body";
  EXPECT_EQ(call->getName(), "$info");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Constant>(call->getArguments()->at(0)), nullptr)
      << "'$info's message argument must be a (string) Constant";
}

// ---------------------------------------------------------------------------
// The four calls are distinct task names -- confirms the parser
// distinguishes the four elaboration system tasks rather than collapsing
// them to a single generic severity call.
// ---------------------------------------------------------------------------

TEST_F(ElabSysCallTest, AllFourElaborationSysTasksHaveDistinctNames) {
  const hldb::SysTaskCall *const fatalCall = findSysTaskCallInGenIf(getModule("top_fatal"));
  const hldb::SysTaskCall *const errorCall = findSysTaskCallInGenIf(getModule("top_error"));
  const hldb::SysTaskCall *const warningCall = findSysTaskCallInGenIf(getModule("top_warning"));
  const hldb::SysTaskCall *const infoCall = findSysTaskCallInGenIf(getModule("top_info"));
  ASSERT_NE(fatalCall, nullptr);
  ASSERT_NE(errorCall, nullptr);
  ASSERT_NE(warningCall, nullptr);
  ASSERT_NE(infoCall, nullptr);

  EXPECT_NE(fatalCall->getName(), errorCall->getName());
  EXPECT_NE(fatalCall->getName(), warningCall->getName());
  EXPECT_NE(fatalCall->getName(), infoCall->getName());
  EXPECT_NE(errorCall->getName(), warningCall->getName());
  EXPECT_NE(errorCall->getName(), infoCall->getName());
  EXPECT_NE(warningCall->getName(), infoCall->getName());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
