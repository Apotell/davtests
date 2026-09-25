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

// Tests for FuncDeclScope.hlc (tests/FuncDeclScope/dut.sv):
//
//   interface intf;
//      parameter int unsigned Depth = 4;
//      int                    o;
//
//      function automatic int get2(); return 1; endfunction
//
//      if (Depth > 2) begin : gen_block
//         function automatic int get3(); return 1; endfunction
//         assign o = get3();
//      end : gen_block
//      else
//        assign o = 0;
//   endinterface
//
//   module top;
//      parameter int unsigned Depth = 4;
//      int                    o;
//
//      function automatic int get4(); return 1; endfunction
//      intf interf();
//      if (Depth > 2) begin : gen_block
//         function automatic int get1(); return 1; endfunction // get1
//         assign o = get1();
//      end : gen_block
//      else
//        assign o = 0;
//   endmodule
//
// What is under test: IEEE 1800-2023 Sec 23.9 "Scope rules" -- each
// function declaration belongs to the *lexical scope* it is written in
// (interface top scope, an interface's own generate block, a module top
// scope, or a module's own generate block) and must be reachable there,
// but must NOT leak into any of the sibling scopes that happen to share the
// same names ("gen_block", "Depth") or the same enclosing design:
//   - 'get2' lives in 'intf's own (interface) scope.
//   - 'get3' lives inside 'intf's own 'gen_block' generate scope.
//   - 'get4' lives in 'top's own (module) scope.
//   - 'get1' lives inside 'top's own 'gen_block' generate scope.
// None of the four names may appear in any scope other than the one that
// declares it -- 'top' has no local 'get2'/'get3' (those belong to
// 'intf'), and 'intf' has no local 'get1'/'get4' (those belong to 'top'),
// even though 'top' instantiates 'intf' as 'interf'.
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\hlc\hlc_03\out\install\x64-Debug\include\hldb.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/return_stmt.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncDeclScopeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncDeclScope.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getIntf() {
    return hldb::findByName<hldb::Interface>("intf", m_design->getAllInterfaces());
  }

  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  template <typename ScopeT>
  static const hldb::Function *findFunc(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName<hldb::Function>(name, scope->getTaskFuncs());
  }

  template <typename ScopeT>
  static const hldb::GenIf *findGenIf(const ScopeT *scope) {
    if (scope == nullptr || scope->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *scope->getGenStmts()) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(stmt)) return gi;
    }
    return nullptr;
  }

  static const hldb::GenScope *genBlockOf(const hldb::GenIf *gi) {
    return (gi == nullptr) ? nullptr : gi->getStmt<hldb::GenScope>();
  }
};

TEST_F(FuncDeclScopeTest, InterfaceAndModuleExist) {
  ASSERT_NE(getIntf(), nullptr) << "interface 'intf' not found";
  ASSERT_NE(getTop(), nullptr) << "module 'top' not found";
}

// 'function automatic int get2(); return 1; endfunction' -- declared
// directly in 'intf's own scope.
TEST_F(FuncDeclScopeTest, Get2LivesInInterfaceOwnScope) {
  const hldb::Interface *const intf = getIntf();
  ASSERT_NE(intf, nullptr);
  const hldb::Function *const get2 = findFunc(intf, "get2");
  ASSERT_NE(get2, nullptr) << "'get2' not found directly in 'intf'";
  EXPECT_TRUE(get2->getAutomatic());
  const hldb::ReturnStmt *const ret = get2->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "'return 1;' should be the function's sole statement";
  const hldb::Constant *const val = ret->getCondition<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getDecompile(), std::string_view("1"));
}

// 'function automatic int get4(); return 1; endfunction' -- declared
// directly in 'top's own scope.
TEST_F(FuncDeclScopeTest, Get4LivesInModuleOwnScope) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Function *const get4 = findFunc(top, "get4");
  ASSERT_NE(get4, nullptr) << "'get4' not found directly in 'top'";
  EXPECT_TRUE(get4->getAutomatic());
}

// 'top' instantiates 'intf' as 'interf ()'.
TEST_F(FuncDeclScopeTest, TopInstantiatesIntfAsInterf) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  const hldb::RefInstance *const interf = hldb::findByName<hldb::RefInstance>("interf", top->getRefInstances());
  ASSERT_NE(interf, nullptr) << "'intf interf();' RefInstance not found";
}

// 'intf's own 'if (Depth > 2) begin : gen_block function get3(); ... end'
TEST_F(FuncDeclScopeTest, Get3LivesInsideInterfaceOwnGenBlock) {
  const hldb::Interface *const intf = getIntf();
  ASSERT_NE(intf, nullptr);
  const hldb::GenIf *const gi = findGenIf(intf);
  ASSERT_NE(gi, nullptr) << "'intf' has no generate-if";
  const hldb::GenScope *const gb = genBlockOf(gi);
  ASSERT_NE(gb, nullptr) << "'begin : gen_block ... end' body should be a GenScope";
  EXPECT_EQ(gb->getName(), std::string_view("gen_block"));

  const hldb::Function *const get3 = findFunc(gb, "get3");
  ASSERT_NE(get3, nullptr) << "'get3' not found inside intf's 'gen_block'";
  EXPECT_TRUE(get3->getAutomatic());

  // Must not also be reachable directly off 'intf's own top-level scope.
  EXPECT_EQ(findFunc(intf, "get3"), nullptr)
      << "'get3' is scoped to 'gen_block'; it must not leak into 'intf's own top-level function list";
}

// 'top's own 'if (Depth > 2) begin : gen_block function get1(); ... end'
TEST_F(FuncDeclScopeTest, Get1LivesInsideModuleOwnGenBlock) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::GenIf *const gi = findGenIf(top);
  ASSERT_NE(gi, nullptr) << "'top' has no generate-if";
  const hldb::GenScope *const gb = genBlockOf(gi);
  ASSERT_NE(gb, nullptr) << "'begin : gen_block ... end' body should be a GenScope";
  EXPECT_EQ(gb->getName(), std::string_view("gen_block"));

  const hldb::Function *const get1 = findFunc(gb, "get1");
  ASSERT_NE(get1, nullptr) << "'get1' not found inside top's 'gen_block'";
  EXPECT_TRUE(get1->getAutomatic());

  EXPECT_EQ(findFunc(top, "get1"), nullptr)
      << "'get1' is scoped to 'gen_block'; it must not leak into 'top's own top-level function list";
}

// Cross-scope isolation: none of 'get1'/'get2'/'get3'/'get4' should be
// visible from a scope other than the one that declares it (Sec 23.9).
TEST_F(FuncDeclScopeTest, FunctionsDoNotLeakAcrossInterfaceAndModuleScopes) {
  const hldb::Interface *const intf = getIntf();
  const hldb::Module *const top = getTop();
  ASSERT_NE(intf, nullptr);
  ASSERT_NE(top, nullptr);

  // 'top's own scope: only 'get4' is its own; 'get1' lives one level down
  // in top's 'gen_block', not at top's own scope.
  EXPECT_EQ(findFunc(top, "get2"), nullptr) << "'get2' belongs to 'intf', not 'top'";
  EXPECT_EQ(findFunc(top, "get3"), nullptr) << "'get3' belongs to 'intf's gen_block, not 'top'";
  EXPECT_EQ(findFunc(top, "get1"), nullptr) << "'get1' belongs to top's own gen_block, not top's own scope";

  // 'intf's own scope: only 'get2' is its own.
  EXPECT_EQ(findFunc(intf, "get1"), nullptr) << "'get1' belongs to 'top's gen_block, not 'intf'";
  EXPECT_EQ(findFunc(intf, "get4"), nullptr) << "'get4' belongs to 'top', not 'intf'";
  EXPECT_EQ(findFunc(intf, "get3"), nullptr) << "'get3' belongs to intf's own gen_block, not intf's own scope";
}

// Both 'Depth' parameters (one on 'intf', one on 'top') independently
// default to 4 -- same name, distinct declarations in distinct scopes.
TEST_F(FuncDeclScopeTest, DepthParameterIndependentlyDefaultsToFourInBothScopes) {
  const hldb::Interface *const intf = getIntf();
  const hldb::Module *const top = getTop();
  ASSERT_NE(intf, nullptr);
  ASSERT_NE(top, nullptr);

  const hldb::ParamAssign *const paIntf = hldb::findByName("Depth", intf->getParamAssigns());
  ASSERT_NE(paIntf, nullptr) << "'intf's own 'Depth' ParamAssign not found";
  const hldb::Constant *const cIntf = paIntf->getRhs<hldb::Constant>();
  ASSERT_NE(cIntf, nullptr);
  EXPECT_EQ(cIntf->getDecompile(), std::string_view("4"));

  const hldb::ParamAssign *const paTop = hldb::findByName("Depth", top->getParamAssigns());
  ASSERT_NE(paTop, nullptr) << "'top's own 'Depth' ParamAssign not found";
  const hldb::Constant *const cTop = paTop->getRhs<hldb::Constant>();
  ASSERT_NE(cTop, nullptr);
  EXPECT_EQ(cTop->getDecompile(), std::string_view("4"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
