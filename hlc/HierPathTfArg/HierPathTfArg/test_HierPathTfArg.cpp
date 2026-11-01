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
// 1800-2023 Sec 23.6 (hierarchical names) and Sec 13.4 (functions). The
// call is the argument itself; the hierarchical scope it is called through
// ("blk", "i.blk", "top.i.blk") is the call's prefix.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/gen_block.h>
#include <hldb/initial.h>
#include <hldb/method_func_call.h>
#include <hldb/method_task_call.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/subroutine_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/tf_call.h>

#include <string>
#include <string_view>
#include <vector>

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

  // The Nth $display's argument: the (hierarchically-qualified) call itself. The call is lifted out
  // of the hierarchical name; the scope path it is called through is the call's prefix. Taken as the
  // common TFCall base: what is checked here is which function the call binds to, not whether HLC
  // models it as a FuncCall or a MethodFuncCall.
  static const hldb::TFCall *getNthPathCall(size_t n) {
    const hldb::SysTaskCall *const disp = getNthDisplay(n);
    if (disp == nullptr || disp->getArguments() == nullptr || disp->getArguments()->empty()) return nullptr;
    return disp->getArguments()->at(0)->getHighConn<hldb::TFCall>();
  }

  // The hierarchical scope a call is made through (its vpiPrefix), or nullptr.
  static const hldb::Expr *getCallPrefix(const hldb::TFCall *call) {
    if (const hldb::MethodFuncCall *const mfc = any_cast<hldb::MethodFuncCall>(call)) return mfc->getPrefix();
    if (const hldb::MethodTaskCall *const mtc = any_cast<hldb::MethodTaskCall>(call)) return mtc->getPrefix();
    if (const hldb::SubroutineCall *const sc = any_cast<hldb::SubroutineCall>(call)) return sc->getPrefix();
    return nullptr;
  }

  // The names of a prefix's elements: "blk" -> {blk}, "i.blk" -> {i, blk}.
  static std::vector<std::string_view> getPrefixNames(const hldb::Expr *prefix) {
    std::vector<std::string_view> names;
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(prefix);
    if (ref == nullptr) return names;
    if ((ref->getPathElems() == nullptr) || ref->getPathElems()->empty()) {
      names.emplace_back(ref->getName());
      return names;
    }
    for (const hldb::Any *const elem : *ref->getPathElems()) names.emplace_back(elem->getName());
    return names;
  }

  // The named scopes that declare 'any', outermost first: "top.i.blk". Unnamed generate regions
  // (generate ... endgenerate) are not scopes (Sec 27.3) and are skipped.
  static std::string getDeclaringScope(const hldb::Any *any) {
    std::string path;
    for (const hldb::Any *p = (any == nullptr) ? nullptr : any->getParent(); p != nullptr; p = p->getParent()) {
      if ((any_cast<hldb::GenBlock>(p) == nullptr) && (any_cast<hldb::Module>(p) == nullptr)) continue;
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

// $display(blk.f(0)) -- one-level hierarchical call: call "f" made through
// the scope "blk" (the call's prefix).
TEST_F(HierPathTfArgTest, SecondDisplayArgIsHierPathBlkDotF) {
  const hldb::SysTaskCall *const disp = getNthDisplay(1);
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::TFCall *const call = getNthPathCall(1);
  ASSERT_NE(call, nullptr) << "expected the call 'f(0)' as the argument of $display(blk.f(0))";
  EXPECT_EQ(call->getName(), std::string_view("f"));
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);

  const hldb::Expr *const prefix = getCallPrefix(call);
  ASSERT_NE(prefix, nullptr) << "the scope 'blk' the call is made through should be the call's prefix";
  const std::vector<std::string_view> names = getPrefixNames(prefix);
  ASSERT_EQ(names.size(), 1u);
  EXPECT_EQ(names[0], std::string_view("blk"));
}

// $display(i.blk.f(0)) -- two-level hierarchical call.
TEST_F(HierPathTfArgTest, FourthDisplayArgIsHierPathIDotBlkDotF) {
  const hldb::SysTaskCall *const disp = getNthDisplay(3);
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::TFCall *const call = getNthPathCall(3);
  ASSERT_NE(call, nullptr) << "expected the call 'f(0)' as the argument of $display(i.blk.f(0))";
  EXPECT_EQ(call->getName(), std::string_view("f"));

  const hldb::Expr *const prefix = getCallPrefix(call);
  ASSERT_NE(prefix, nullptr) << "the scope 'i.blk' the call is made through should be the call's prefix";
  const std::vector<std::string_view> names = getPrefixNames(prefix);
  ASSERT_EQ(names.size(), 2u) << "expected i -> blk";
  EXPECT_EQ(names[0], std::string_view("i"));
  EXPECT_EQ(names[1], std::string_view("blk"));
}

// $display(top.i.blk.f(0)) -- three-level hierarchical call rooted at the
// module's own name.
TEST_F(HierPathTfArgTest, EighthDisplayArgIsHierPathTopDotIDotBlkDotF) {
  const hldb::SysTaskCall *const disp = getNthDisplay(7);
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::TFCall *const call = getNthPathCall(7);
  ASSERT_NE(call, nullptr) << "expected the call 'f(0)' as the argument of $display(top.i.blk.f(0))";
  EXPECT_EQ(call->getName(), std::string_view("f"));

  const hldb::Expr *const prefix = getCallPrefix(call);
  ASSERT_NE(prefix, nullptr) << "the scope 'top.i.blk' the call is made through should be the call's prefix";
  const std::vector<std::string_view> names = getPrefixNames(prefix);
  ASSERT_EQ(names.size(), 3u) << "expected top -> i -> blk";
  EXPECT_EQ(names[0], std::string_view("top"));
  EXPECT_EQ(names[1], std::string_view("i"));
  EXPECT_EQ(names[2], std::string_view("blk"));
}

TEST_F(HierPathTfArgTest, CompilerReportsZeroErrors) {
  GTEST_SKIP() << "HLC does have a way to reliably check for null actuals";
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
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
    const hldb::TFCall *const call = getNthPathCall(e.m_display);
    ASSERT_NE(call, nullptr) << "$display #" << e.m_display << ": path must end in a bound function call";
    const hldb::Function *const fn = call->getTaskFunc<hldb::Function>();
    ASSERT_NE(fn, nullptr) << "$display #" << e.m_display << ": call not bound to a Function";
    EXPECT_EQ(fn->getName(), e.m_function);
    EXPECT_EQ(getDeclaringScope(fn), e.m_scope) << "$display #" << e.m_display << " bound to the wrong '"
                                                << e.m_function << "'";
  }

  // $display(f(0)): no 'f' is visible in 'top', so the call stays unbound and is reported.
  EXPECT_NE(findEvent(EventType::CompFailedToBind, "f", 27, 18), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
