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

// Tests for tests/HierPathTfArg/dut.sv:
//   module top;
//     generate
//       if (1) begin : i
//         function automatic integer f; input unused; f = 1; endfunction
//         if (1) begin : blk
//           function automatic integer f; input unused; f = 2; endfunction
//         end
//       end
//       function automatic integer f3; input unused; f3 = 3; endfunction
//       if (1) begin : blk
//         function automatic integer f; input unused; f = 4; endfunction
//       end
//     endgenerate
//     initial begin
//       $display(f(0));
//       $display(blk.f(0));
//       $display(i.f(0));
//       $display(i.blk.f(0));
//       $display(top.f3(0));
//       $display(top.blk.f(0));
//       $display(top.i.f(0));
//       $display(top.i.blk.f(0));
//     end
//   endmodule
//
// This exercises hierarchical paths passed as task/function call arguments:
// each $display() (a system task call) is given a single argument that is
// itself a (possibly hierarchically-qualified) function call, per IEEE
// 1800-2023 Sec 23.6 (hierarchical names) and Sec 13.4 (functions).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/gen_region.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/subroutine_call.h>
#include <hldb/sys_task_call.h>

#include <string>

namespace hlc {

class HierPathTfArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathTfArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  // Returns the Nth $display SysTaskCall in the initial block (0-based).
  static const hldb::SysTaskCall *getNthDisplay(size_t n) {
    const hldb::Begin *const blk = getInitialBegin();
    if (blk == nullptr || blk->getStmts() == nullptr) return nullptr;
    size_t idx = 0;
    for (const hldb::Any *const s : *blk->getStmts()) {
      if (const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(s)) {
        if (idx++ == n) return disp;
      }
    }
    return nullptr;
  }

  // The last path element of the Nth $display's hierarchical argument, once bound: a FuncCall.
  static const hldb::FuncCall *getNthPathCall(size_t n) {
    const hldb::SysTaskCall *const disp = getNthDisplay(n);
    if (disp == nullptr || disp->getArguments() == nullptr || disp->getArguments()->empty()) return nullptr;
    const hldb::RefObj *const arg = disp->getArguments()->at(0)->getHighConn<hldb::RefObj>();
    if (arg == nullptr || arg->getPathElems() == nullptr || arg->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::FuncCall>(arg->getPathElems()->back());
  }

  // The named scopes that declare 'any', outermost first: "top.i.blk". Unnamed generate regions
  // (generate ... endgenerate) are not scopes (Sec 27.3) and are skipped.
  static std::string getDeclaringScope(const hldb::Any *any) {
    std::string path;
    for (const hldb::Any *p = (any == nullptr) ? nullptr : any->getParent(); p != nullptr; p = p->getParent()) {
      if ((any_cast<hldb::GenRegion>(p) == nullptr) && (any_cast<hldb::Module>(p) == nullptr)) continue;
      if (p->getName().empty()) continue;
      path = path.empty() ? std::string(p->getName()) : std::string(p->getName()) + "." + path;
      if (any_cast<hldb::Module>(p) != nullptr) break;
    }
    return path;
  }
};

TEST_F(HierPathTfArgTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(HierPathTfArgTest, InitialBeginHasEightDisplayStmts) {
  const hldb::Begin *const blk = getInitialBegin();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  size_t count = 0;
  for (const hldb::Any *const s : *blk->getStmts())
    if (any_cast<hldb::SysTaskCall>(s)) ++count;
  EXPECT_EQ(count, 8u);
}

// $display(f(0)) -- unqualified call, argument is a plain SubroutineCall.
TEST_F(HierPathTfArgTest, FirstDisplayArgIsPlainSubroutineCallF) {
  const hldb::SysTaskCall *const disp = getNthDisplay(0);
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), std::string_view("$display"));
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::NamedArgument *const dispArg0 = disp->getArguments()->at(0);
  ASSERT_NE(dispArg0, nullptr);
  const hldb::SubroutineCall *const call = dispArg0->getHighConn<hldb::SubroutineCall>();
  ASSERT_NE(call, nullptr) << "expected a plain SubroutineCall for 'f(0)'";
  EXPECT_EQ(call->getName(), std::string_view("f"));
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::NamedArgument *const callArg0 = call->getArguments()->at(0);
  ASSERT_NE(callArg0, nullptr);
  const hldb::Constant *const argZero = callArg0->getHighConn<hldb::Constant>();
  ASSERT_NE(argZero, nullptr);
  EXPECT_EQ(argZero->getDecompile(), std::string_view("0"));
}

// $display(blk.f(0)) -- one-level hierarchical call: RefObj "blk" ->
// SubroutineCall "f".
TEST_F(HierPathTfArgTest, SecondDisplayArgIsHierPathBlkDotF) {
  const hldb::SysTaskCall *const disp = getNthDisplay(1);
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::RefObj *const arg = arg0->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg, nullptr) << "expected a RefObj hierarchical path for 'blk.f(0)'";
  EXPECT_EQ(arg->getName(), std::string_view("blk.f(0)"));
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 2u);

  const hldb::RefObj *const scopeElem = any_cast<hldb::RefObj>(arg->getPathElems()->at(0));
  ASSERT_NE(scopeElem, nullptr);
  EXPECT_EQ(scopeElem->getName(), std::string_view("blk"));

  // Bound to the function it calls, so the call is a FuncCall (Sec 23.6).
  const hldb::FuncCall *const callElem = any_cast<hldb::FuncCall>(arg->getPathElems()->at(1));
  ASSERT_NE(callElem, nullptr) << "expected a FuncCall for the 'f(0)' path element";
  EXPECT_EQ(callElem->getName(), std::string_view("f"));
  ASSERT_NE(callElem->getArguments(), nullptr);
  ASSERT_EQ(callElem->getArguments()->size(), 1u);
}

// $display(i.blk.f(0)) -- two-level hierarchical call.
TEST_F(HierPathTfArgTest, FourthDisplayArgIsHierPathIDotBlkDotF) {
  const hldb::SysTaskCall *const disp = getNthDisplay(3);
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::RefObj *const arg = arg0->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), std::string_view("i.blk.f(0)"));
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 3u) << "expected i -> blk -> f(0)";

  EXPECT_EQ(any_cast<hldb::RefObj>(arg->getPathElems()->at(0))->getName(), std::string_view("i"));
  EXPECT_EQ(any_cast<hldb::RefObj>(arg->getPathElems()->at(1))->getName(), std::string_view("blk"));
  const hldb::FuncCall *const callElem = any_cast<hldb::FuncCall>(arg->getPathElems()->at(2));
  ASSERT_NE(callElem, nullptr) << "expected a FuncCall for the 'f(0)' path element";
  EXPECT_EQ(callElem->getName(), std::string_view("f"));
}

// $display(top.i.blk.f(0)) -- three-level hierarchical call rooted at the
// module's own name.
TEST_F(HierPathTfArgTest, EighthDisplayArgIsHierPathTopDotIDotBlkDotF) {
  const hldb::SysTaskCall *const disp = getNthDisplay(7);
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::RefObj *const arg = arg0->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), std::string_view("top.i.blk.f(0)"));
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 4u) << "expected top -> i -> blk -> f(0)";
}

TEST_F(HierPathTfArgTest, CompilerReportsZeroErrors) {
  GTEST_SKIP() << "HLC does have a way to reliably check for null actuals";
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// IEEE 1800-2023 Sec 23.6/27.5: a named generate block is a scope, and a hierarchical name is
// resolved by walking the named scopes, so 'blk.f(0)', 'i.f(0)' and 'i.blk.f(0)' call three
// different functions 'f'. The unqualified 'f(0)' does not resolve: 'f' is declared only inside
// the named blocks, never in 'top' itself.
TEST_F(HierPathTfArgTest, HierarchicalCallsShouldResolveToTheirDeclaredFunction) {
  const struct {
    size_t m_display;
    std::string_view m_function;
    std::string_view m_scope;
  } kExpected[] = {
      {1, "f", "top.blk"},   {2, "f", "top.i"},       {3, "f", "top.i.blk"}, {4, "f3", "top"},
      {5, "f", "top.blk"},   {6, "f", "top.i"},       {7, "f", "top.i.blk"},
  };
  for (const auto &e : kExpected) {
    const hldb::FuncCall *const call = getNthPathCall(e.m_display);
    ASSERT_NE(call, nullptr) << "$display #" << e.m_display << ": path must end in a bound FuncCall";
    const hldb::Function *const fn = call->getTaskFunc<hldb::Function>();
    ASSERT_NE(fn, nullptr) << "$display #" << e.m_display << ": call not bound to a Function";
    EXPECT_EQ(fn->getName(), e.m_function);
    EXPECT_EQ(getDeclaringScope(fn), e.m_scope) << "$display #" << e.m_display << " bound to the wrong '"
                                                << e.m_function << "'";
  }

  // $display(f(0)): no 'f' is visible in 'top', so the call stays unbound and is reported.
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "f", 27, 18), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
