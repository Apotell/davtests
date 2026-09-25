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

// Tests for FuncBindGen.hlc (tests/FuncBindGen/dut.sv):
//
//   module top(output int o);
//      parameter  int unsigned Depth  = 500;
//      localparam int unsigned PTR_WIDTH = 3;
//
//      if (Depth > 2) begin : gen_block
//         function automatic [PTR_WIDTH-1:0] get_casted_param();
//            logic [PTR_WIDTH-1:0] dec_tmp_sub = (PTR_WIDTH)'(Depth);
//            return dec_tmp_sub;
//         endfunction
//
//         assign o = int'(get_casted_param());
//      end
//   endmodule
//
// Compiled at "-d ast" level (no "-d inst"), so the generate-if survives as
// a GenIf on the module's getGenStmts() (IEEE 1800-2023 Sec 27.5
// "Generate-if constructs") rather than being collapsed to an elaborated
// GenScopeArray/GenScope pair.
//
// What is under test: binding of a function *call* to a function
// *declaration* that is declared inside the very same named generate block
// (IEEE 1800-2023 Sec 27.3 "Generate block": a named generate block, here
// "gen_block", introduces a new scope; Sec 23.9 "Scope rules": identifiers
// declared within a scope, including a subroutine, are visible within that
// scope). "assign o = int'(get_casted_param());" is declared in the same
// "gen_block" scope as "function ... get_casted_param();", so the call
// must resolve back to that exact declaration rather than failing to bind
// or resolving to some other candidate.
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\hlc\hlc_03\out\install\x64-Debug\include\hldb.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/return_stmt.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncBindGenTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncBindGen.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::GenIf *findGenIf(const hldb::Module *m) {
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(stmt)) return gi;
    }
    return nullptr;
  }

  template <typename ScopeT>
  static const hldb::Function *findFunc(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName<hldb::Function>(name, scope->getTaskFuncs());
  }
};

TEST_F(FuncBindGenTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(FuncBindGenTest, DepthParamDefaultsTo500NotLocal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName("Depth", top->getParamAssigns());
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'Depth' not found";
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'Depth = 500': default RHS must be a Constant";
  EXPECT_EQ(c->getDecompile(), std::string_view("500"));

  ASSERT_NE(top->getParameters(), nullptr);
  const hldb::Parameter *const depth = hldb::findByName<hldb::Parameter>("Depth", top->getParameters());
  ASSERT_NE(depth, nullptr) << "'parameter Depth' not found";
  EXPECT_FALSE(depth->getLocalParam()) << "'parameter Depth' must not be a localparam";
}

TEST_F(FuncBindGenTest, PtrWidthIsLocalParamWithValue3) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  const hldb::Parameter *const ptrWidth = hldb::findByName<hldb::Parameter>("PTR_WIDTH", top->getParameters());
  ASSERT_NE(ptrWidth, nullptr) << "'localparam PTR_WIDTH' not found";
  EXPECT_TRUE(ptrWidth->getLocalParam()) << "Sec 6.20.4: 'localparam' must be marked as a localparam";

  const hldb::ParamAssign *const pa = hldb::findByName("PTR_WIDTH", top->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'PTR_WIDTH = 3': default RHS must be a Constant";
  EXPECT_EQ(c->getDecompile(), std::string_view("3"));
}

// 'if (Depth > 2) begin : gen_block ... end' -- a single generate-if with no
// else (Sec 27.5).
TEST_F(FuncBindGenTest, ModuleTopHasExactlyOneGenIf) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr) << "'top' has no generate statements";
  size_t count = 0u;
  for (const hldb::Any *const stmt : *top->getGenStmts()) {
    if (any_cast<hldb::GenIf>(stmt) != nullptr) ++count;
  }
  EXPECT_EQ(count, 1u);
}

// 'Depth > 2' -- IEEE 1800-2023 11.4.4 relational operator '>' is vpiGtOp.
TEST_F(FuncBindGenTest, GenIfConditionIsDepthGreaterThan2) {
  const hldb::GenIf *const gi = findGenIf(getTop());
  ASSERT_NE(gi, nullptr) << "GenIf for 'if (Depth > 2)' not found";
  const hldb::Operation *const cond = gi->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'Depth > 2' condition should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiGtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(lhs, nullptr) << "left operand should be a RefObj (reference to Depth)";
  EXPECT_EQ(lhs->getName(), std::string_view("Depth"));

  const hldb::Constant *const rhs = any_cast<hldb::Constant>((*cond->getOperands())[1]);
  ASSERT_NE(rhs, nullptr) << "right operand should be the Constant '2'";
  EXPECT_EQ(rhs->getDecompile(), std::string_view("2"));
}

// Body: 'begin : gen_block function ... assign ... end' -- a named generate
// block housing a function declaration and a continuous assignment, per
// Sec 27.3; GenScope is the object that carries both getTaskFuncs() and
// getContAssigns().
TEST_F(FuncBindGenTest, GenIfBodyIsNamedGenScopeGenBlock) {
  const hldb::GenIf *const gi = findGenIf(getTop());
  ASSERT_NE(gi, nullptr);
  const hldb::GenScope *const body = gi->getStmt<hldb::GenScope>();
  ASSERT_NE(body, nullptr) << "'begin : gen_block ... end' body should be a GenScope";
  EXPECT_EQ(body->getName(), std::string_view("gen_block"));
}

// 'function automatic [PTR_WIDTH-1:0] get_casted_param();' declared inside
// 'gen_block' -- must be found via gen_block's own scope, not the module's.
TEST_F(FuncBindGenTest, GetCastedParamDeclaredInsideGenBlock) {
  const hldb::GenIf *const gi = findGenIf(getTop());
  ASSERT_NE(gi, nullptr);
  const hldb::GenScope *const body = gi->getStmt<hldb::GenScope>();
  ASSERT_NE(body, nullptr);

  const hldb::Function *const fn = findFunc(body, "get_casted_param");
  ASSERT_NE(fn, nullptr) << "'get_casted_param' not found inside 'gen_block'";
  EXPECT_TRUE(fn->getAutomatic()) << "'function automatic' must set the automatic flag (Sec 13.4.2)";
  EXPECT_NE(fn->getReturn(), nullptr) << "function has a packed-range return type";

  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getTaskFuncs() == nullptr || hldb::findByName<hldb::Function>("get_casted_param",
                                                                                  top->getTaskFuncs()) == nullptr)
      << "'get_casted_param' is scoped to 'gen_block'; it must not leak into 'top's own function list";
}

// function body: 'logic [PTR_WIDTH-1:0] dec_tmp_sub = ...; return dec_tmp_sub;'
// -- one local variable declaration plus a single ReturnStmt (Sec 13.4.4:
// the block_item_declaration for 'dec_tmp_sub' is not itself a statement).
TEST_F(FuncBindGenTest, GetCastedParamBodyReturnsLocalVariable) {
  const hldb::GenIf *const gi = findGenIf(getTop());
  ASSERT_NE(gi, nullptr);
  const hldb::GenScope *const body = gi->getStmt<hldb::GenScope>();
  ASSERT_NE(body, nullptr);
  const hldb::Function *const fn = findFunc(body, "get_casted_param");
  ASSERT_NE(fn, nullptr);

  ASSERT_NE(fn->getVariables(), nullptr);
  const hldb::Variable *const decTmpSub = hldb::findByName<hldb::Variable>("dec_tmp_sub", fn->getVariables());
  ASSERT_NE(decTmpSub, nullptr) << "'logic [PTR_WIDTH-1:0] dec_tmp_sub' not found among the function's variables";
  EXPECT_NE(decTmpSub->getExpr(), nullptr) << "'dec_tmp_sub' has an initializer expression";

  const hldb::ReturnStmt *const ret = fn->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "'return dec_tmp_sub;' should be the function's sole statement";
  const hldb::RefObj *const retVal = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(retVal, nullptr) << "'return dec_tmp_sub;' should reference 'dec_tmp_sub' via a RefObj";
  EXPECT_EQ(retVal->getName(), std::string_view("dec_tmp_sub"));
  EXPECT_NE(retVal->getActual(), nullptr);
  EXPECT_EQ(retVal->getActual<hldb::Variable>(), decTmpSub)
      << "'dec_tmp_sub' reference should resolve back to its own declaration";
}

// 'assign o = int'(get_casted_param());' -- the core binding under test:
// the call to 'get_casted_param' must resolve to the Function declared
// earlier in the same 'gen_block' scope (Sec 23.9).
TEST_F(FuncBindGenTest, ContAssignCallsGetCastedParamBoundToGenBlockDecl) {
  const hldb::GenIf *const gi = findGenIf(getTop());
  ASSERT_NE(gi, nullptr);
  const hldb::GenScope *const body = gi->getStmt<hldb::GenScope>();
  ASSERT_NE(body, nullptr);
  const hldb::Function *const fn = findFunc(body, "get_casted_param");
  ASSERT_NE(fn, nullptr);

  ASSERT_NE(body->getContAssigns(), nullptr);
  ASSERT_EQ(body->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const assign = body->getContAssigns()->at(0);
  ASSERT_NE(assign, nullptr);

  ASSERT_NE(assign->getLhs(), nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'assign o = ...': LHS must be a RefObj (reference to o)";
  EXPECT_EQ(lhs->getName(), std::string_view("o"));

  // 'int'(get_casted_param())' -- a type cast (Sec 6.24.1) wrapping the
  // function call; if HLC does not preserve the outer cast as an Operation,
  // fall back to locating the FuncCall directly.
  ASSERT_NE(assign->getRhs(), nullptr);
  const hldb::FuncCall *call = assign->getRhs<hldb::FuncCall>();
  if (call == nullptr) {
    const hldb::Operation *const castOp = assign->getRhs<hldb::Operation>();
    ASSERT_NE(castOp, nullptr) << "'int'(get_casted_param())': RHS should be a cast Operation or a bare FuncCall";
    ASSERT_NE(castOp->getOperands(), nullptr);
    ASSERT_EQ(castOp->getOperands()->size(), 1u);
    call = any_cast<hldb::FuncCall>((*castOp->getOperands())[0]);
  }
  ASSERT_NE(call, nullptr) << "'get_casted_param()' call not found";
  EXPECT_EQ(call->getName(), std::string_view("get_casted_param"));
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), fn)
      << "the call must bind to the 'get_casted_param' function declared in the same 'gen_block' scope";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
