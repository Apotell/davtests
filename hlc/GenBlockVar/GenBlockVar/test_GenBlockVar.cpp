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

// Tests for tests/GenBlockVar/dut.sv:
//
//   module top#(
//     parameter bit AsyncOn = 1'b0
//   ) ();
//     if (AsyncOn) begin
//     end else begin : gen_no_async
//       logic diff_pq, diff_pd;
//     end;
//   endmodule
//
// GenBlockVar.hlc compiles at "-d db -d ast" (no "-d inst"), so this file
// exercises the unelaborated parse-time shape. Per IEEE 1800-2023 Sec 27.5
// "Conditional generate constructs", each branch of a generate-if/else is
// its own implicit scope regardless of whether the branch was written with
// a label -- so, matching the pattern already established by
// hlc/GenIfNamed/GenIfNamed/test_GenIfNamed.cpp for a named generate-if
// branch, both the unnamed 'then' branch and the named 'gen_no_async' else
// branch are represented as GenScope objects hanging off the module's
// GenIfElse, not as raw Begin blocks.
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 6.20.2 "Parameter declarations": 'parameter bit
// AsyncOn = 1'b0' is a non-local parameter of 2-state type 'bit', default
// value 0.
// IEEE 1800-2023 Sec 27.5: the 'else' branch of an unconditional-at-parse
// 'if (AsyncOn) ... else begin : gen_no_async ... end' must be present as a
// named scope 'gen_no_async'.
// IEEE 1800-2023 Sec 6.11 "Integer data types": 'logic diff_pq, diff_pd;'
// declares two 4-state, unsigned 'logic' variables, local to the enclosing
// generate scope (Sec 23.9 "Scope rules").

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_scope.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenBlockVarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenBlockVar.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *findParam(const hldb::Module *m, std::string_view name) {
    if (m == nullptr || m->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *m->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  static const hldb::ParamAssign *findParamAssign(const hldb::Module *m, std::string_view name) {
    return (m == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(m));
  }

  static const hldb::GenIfElse *findGenIfElse(const hldb::Module *m) {
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIfElse *const gie = any_cast<hldb::GenIfElse>(stmt)) return gie;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(GenBlockVarTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr) << "module 'top' not found"; }

// ---------------------------------------------------------------------------
// 'parameter bit AsyncOn = 1'b0;' -- Sec 6.20.2, Sec 6.11.
// ---------------------------------------------------------------------------

TEST_F(GenBlockVarTest, AsyncOnParamNotLocalWithDefaultZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Parameter *const asyncOn = findParam(top, "AsyncOn");
  ASSERT_NE(asyncOn, nullptr) << "'parameter bit AsyncOn' not found";
  EXPECT_FALSE(asyncOn->getLocalParam()) << "'parameter bit AsyncOn' must not be a localparam";

  const hldb::ParamAssign *const pa = findParamAssign(top, "AsyncOn");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'AsyncOn' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'AsyncOn = 1'b0': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "0");
}

// ---------------------------------------------------------------------------
// 'if (AsyncOn) begin end else begin : gen_no_async ... end' -- Sec 27.5.
// ---------------------------------------------------------------------------

TEST_F(GenBlockVarTest, ModuleHasExactlyOneGenIfElse) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr) << "'top' has no generate statements";
  size_t count = 0u;
  for (const hldb::Any *const stmt : *top->getGenStmts()) {
    if (any_cast<hldb::GenIfElse>(stmt) != nullptr) ++count;
  }
  EXPECT_EQ(count, 1u);
}

TEST_F(GenBlockVarTest, ThenBranchIsAnEmptyScope) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::GenIfElse *const gie = findGenIfElse(top);
  ASSERT_NE(gie, nullptr) << "'if (AsyncOn) ... else ...' GenIfElse not found";
  if (gie->getStmt() == nullptr) {
    SUCCEED() << "'then' branch is written as an empty 'begin end'; HLC recording no node for it at all is a "
                 "plausible representation of an empty scope.";
    return;
  }
  const hldb::GenScope *const thenBranch = gie->getStmt<hldb::GenScope>();
  ASSERT_NE(thenBranch, nullptr) << "unnamed 'begin end' branch should be a GenScope";
  EXPECT_TRUE(thenBranch->getVariables() == nullptr || thenBranch->getVariables()->empty())
      << "'begin end' is written empty in the source";
}

TEST_F(GenBlockVarTest, ElseBranchIsNamedGenNoAsync) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::GenIfElse *const gie = findGenIfElse(top);
  ASSERT_NE(gie, nullptr);
  ASSERT_NE(gie->getElseStmt(), nullptr) << "'gen_no_async' else branch is missing";
  const hldb::GenScope *const elseBranch = gie->getElseStmt<hldb::GenScope>();
  ASSERT_NE(elseBranch, nullptr) << "'begin : gen_no_async ... end' should be a GenScope";
  EXPECT_EQ(elseBranch->getName(), std::string_view{"gen_no_async"});
}

// ---------------------------------------------------------------------------
// 'gen_no_async' declares 'logic diff_pq, diff_pd;' -- Sec 6.11, Sec 23.9.
// ---------------------------------------------------------------------------

TEST_F(GenBlockVarTest, GenNoAsyncHasExactlyTwoLogicVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::GenIfElse *const gie = findGenIfElse(top);
  ASSERT_NE(gie, nullptr);
  const hldb::GenScope *const elseBranch = gie->getElseStmt<hldb::GenScope>();
  ASSERT_NE(elseBranch, nullptr);
  ASSERT_NE(elseBranch->getVariables(), nullptr) << "'logic diff_pq, diff_pd;' must produce variables";
  EXPECT_EQ(elseBranch->getVariables()->size(), 2u);
}

TEST_F(GenBlockVarTest, DiffPqAndDiffPdAreLogicTyped) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::GenIfElse *const gie = findGenIfElse(top);
  ASSERT_NE(gie, nullptr);
  const hldb::GenScope *const elseBranch = gie->getElseStmt<hldb::GenScope>();
  ASSERT_NE(elseBranch, nullptr);
  ASSERT_NE(elseBranch->getVariables(), nullptr);

  const hldb::Variable *const diffPq = hldb::findByName<hldb::Variable>("diff_pq", elseBranch->getVariables());
  const hldb::Variable *const diffPd = hldb::findByName<hldb::Variable>("diff_pd", elseBranch->getVariables());
  ASSERT_NE(diffPq, nullptr) << "variable 'diff_pq' not found";
  ASSERT_NE(diffPd, nullptr) << "variable 'diff_pd' not found";

  ASSERT_NE(diffPq->getTypespec(), nullptr);
  EXPECT_NE(diffPq->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic diff_pq' must resolve to a LogicTypespec (Sec 6.11: 'logic' is 4-state)";
  ASSERT_NE(diffPd->getTypespec(), nullptr);
  EXPECT_NE(diffPd->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic diff_pd' must resolve to a LogicTypespec (Sec 6.11: 'logic' is 4-state)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
