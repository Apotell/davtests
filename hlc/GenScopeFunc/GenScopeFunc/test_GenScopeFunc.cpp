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

// Tests for GenScopeFunc.hlc (tests/GenScopeFunc/dut.sv):
//
//   module mod ();
//
//     generate
//       if (1) begin : cscope
//         function automatic logic local_function(input  x);
//           local_function = x + 1;
//         endfunction
//       end
//     endgenerate
//
//     assign foo = cscope.local_function(counter);
//   endmodule
//
//   module top ();
//     mod  c ();
//   endmodule
//
// Compiled at "-d ast" level (no "-d inst"), so the generate-if survives as
// a GenIf on 'mod's getGenStmts() (IEEE 1800-2023 Sec 27.5) rather than
// being collapsed into an elaborated GenScopeArray/GenScope pair.
//
// What is under test: a function declared inside a named generate scope
// (IEEE 1800-2023 Sec 27.3 "Generate block": 'begin : cscope ... end'
// introduces a new scope that may contain subroutine declarations, Sec
// 13.4), and a subsequent call to that function through an explicit
// hierarchical/scope-qualified reference 'cscope.local_function(...)' (Sec
// 23.9 "Scope rules"). The call must bind back to the Function declared
// inside 'cscope', not fail to bind or resolve elsewhere.
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\davtests\davtests_02\build\include\hldb.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenScopeFuncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenScopeFunc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::GenIf *findGenIf(const hldb::Module *m) {
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(stmt)) return gi;
    }
    return nullptr;
  }

  static const hldb::GenScope *getCscope() {
    const hldb::GenIf *const gi = findGenIf(getModule("mod"));
    return (gi == nullptr) ? nullptr : gi->getStmt<hldb::GenScope>();
  }
};

TEST_F(GenScopeFuncTest, BothModulesExist) {
  ASSERT_NE(getModule("mod"), nullptr);
  ASSERT_NE(getModule("top"), nullptr);
}

// 'top' instantiates 'mod' via instance 'c'.
TEST_F(GenScopeFuncTest, TopInstantiatesModAsC) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getModules(), nullptr);
  const hldb::Module *const c = hldb::findByName<hldb::Module>("c", top->getModules());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDefName(), std::string_view("mod"));
}

// 'if (1) begin : cscope ... end' -- exactly one generate-if, named
// 'cscope' (Sec 27.5).
TEST_F(GenScopeFuncTest, ModHasExactlyOneGenIfNamedCscope) {
  const hldb::Module *const mod = getModule("mod");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getGenStmts(), nullptr);
  size_t count = 0u;
  for (const hldb::Any *const stmt : *mod->getGenStmts()) {
    if (any_cast<hldb::GenIf>(stmt) != nullptr) ++count;
  }
  EXPECT_EQ(count, 1u);

  const hldb::GenScope *const cscope = getCscope();
  ASSERT_NE(cscope, nullptr) << "'begin : cscope ... end' should be a GenScope";
  EXPECT_EQ(cscope->getName(), std::string_view("cscope"));
}

// 'function automatic logic local_function(input x);' declared inside
// 'cscope' -- must be reachable through cscope's own scope, not 'mod's.
TEST_F(GenScopeFuncTest, LocalFunctionDeclaredInsideCscope) {
  const hldb::GenScope *const cscope = getCscope();
  ASSERT_NE(cscope, nullptr);
  ASSERT_NE(cscope->getTaskFuncs(), nullptr) << "'cscope' should carry 'local_function'";
  const hldb::Function *const fn = hldb::findByName<hldb::Function>("local_function", cscope->getTaskFuncs());
  ASSERT_NE(fn, nullptr) << "'local_function' not found inside 'cscope'";
  EXPECT_TRUE(fn->getAutomatic()) << "'function automatic' must set the automatic flag (Sec 13.4.2)";
  EXPECT_NE(fn->getReturn(), nullptr) << "function has a 'logic' return type";

  ASSERT_NE(fn->getIODecls(), nullptr);
  const hldb::IODecl *const x = hldb::findByName<hldb::IODecl>("x", fn->getIODecls());
  ASSERT_NE(x, nullptr) << "'input x' not found among the function's IO declarations";
  EXPECT_EQ(x->getDirection(), vpiInput);

  const hldb::Module *const mod = getModule("mod");
  ASSERT_NE(mod, nullptr);
  EXPECT_TRUE(mod->getTaskFuncs() == nullptr ||
              hldb::findByName<hldb::Function>("local_function", mod->getTaskFuncs()) == nullptr)
      << "'local_function' is scoped to 'cscope'; it must not leak into 'mod's own function list";
}

// 'assign foo = cscope.local_function(counter);' -- the core binding under
// test: the scope-qualified call must resolve to the Function declared
// inside 'cscope' (Sec 23.9).
TEST_F(GenScopeFuncTest, ContAssignCallsLocalFunctionBoundToCscope) {
  const hldb::Module *const mod = getModule("mod");
  ASSERT_NE(mod, nullptr);
  const hldb::GenScope *const cscope = getCscope();
  ASSERT_NE(cscope, nullptr);
  const hldb::Function *const fn = hldb::findByName<hldb::Function>("local_function", cscope->getTaskFuncs());
  ASSERT_NE(fn, nullptr);

  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_EQ(mod->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const assign = mod->getContAssigns()->at(0);
  ASSERT_NE(assign, nullptr);

  ASSERT_NE(assign->getLhs(), nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'assign foo = ...': LHS should be a RefObj (reference to foo)";
  EXPECT_EQ(lhs->getName(), std::string_view("foo"));

  ASSERT_NE(assign->getRhs(), nullptr);
  const hldb::FuncCall *const call = assign->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "'cscope.local_function(counter)' should be a FuncCall";
  EXPECT_EQ(call->getName(), std::string_view("local_function"));
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), fn)
      << "the call must bind to the 'local_function' declared inside the 'cscope' generate scope";

  // The call is reached through the 'cscope.' scope qualifier.
  if (call->getScope() != nullptr) {
    const hldb::GenScope *const scopeQualifier = call->getScope<hldb::GenScope>();
    ASSERT_NE(scopeQualifier, nullptr) << "'cscope' qualifier should resolve to the GenScope itself";
    EXPECT_EQ(scopeQualifier->getName(), std::string_view("cscope"));
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
