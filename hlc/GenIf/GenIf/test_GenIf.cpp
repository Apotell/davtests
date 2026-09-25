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

// Tests for tests/GenIf/dut.sv, module 'gen_test4':
//
//   module gen_test4();
//     genvar i;
//     generate
//       for (i=0; i < 2; i=i+1) begin : foo
//         if (i == 0)
//           assign temp1 = a;
//         else
//           assign temp2 = b;
//       end
//     endgenerate
//   endmodule
//
// Note: despite this test's name, the conditional generate construct in
// the actual source has an 'else' branch. Per IEEE 1800-2023 Sec 27.3's
// grammar, "conditional_generate_construct ::= if_generate_construct |
// case_generate_construct" and "if_generate_construct ::= if ( constant_
// expression ) generate_block_or_null [ else generate_block_or_null ]" --
// the presence of the 'else' clause means this is modeled as a GenIfElse
// node (gen_if_else.h), not a bare GenIf (gen_if.h): the two classes
// aren't distinguished by which keyword introduced the block but by
// whether an 'else' branch is actually present. This file is written from
// what tests/GenIf/dut.sv actually contains, per the instruction to verify
// actual file content rather than assume from a name.
//
// -- rules under test ---------------------------------------------------
//
// Sec 27.4/27.5: the outer construct is "generate for (i=0; i<2; i=i+1)
// begin : foo ... end endgenerate" (predeclared genvar 'i', matching the
// DoubleLoop-established GenRegion->GenFor shape); its single body item
// is the if/else conditional generate construct itself -- since the loop
// body 'begin : foo ... end' is a named generate_block whose sole
// generate_item is the if-else, that if-else appears as a direct item of
// 'foo's own statement list (Sec 27.3's "generate_block_or_null").
// The if-else's condition 'i == 0' (Sec 11.4.5, vpiEqOp) selects between
// two un-blocked single-statement branches (no begin/end needed per
// Sec 27.5, mirroring 12.4's single-statement if/else) -- 'assign temp1 =
// a;' when true, 'assign temp2 = b;' when false.
//
// GenIf.hlc uses "-d db -d ast" (no "-d inst", and no "-nobuiltin"), so no
// elaboration/unrolling is checked here.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenIfTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenIf.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // module -> getGenStmts() -> GenRegion -> getStmt<GenFor>()
  static const hldb::GenFor *getLoop() {
    const hldb::Module *const m = getModule("gen_test4");
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(stmt);
      if (region == nullptr) continue;
      const hldb::GenFor *const loop = region->getStmt<hldb::GenFor>();
      if (loop != nullptr) return loop;
    }
    return nullptr;
  }

  // loop -> getStmt<Begin>() named "foo" -> its sole statement, the
  // GenIfElse.
  static const hldb::GenIfElse *getIfElse() {
    const hldb::GenFor *const loop = getLoop();
    if (loop == nullptr) return nullptr;
    const hldb::Begin *const foo = loop->getStmt<hldb::Begin>();
    if (foo == nullptr || foo->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *foo->getStmts()) {
      const hldb::GenIfElse *const ifElse = any_cast<hldb::GenIfElse>(stmt);
      if (ifElse != nullptr) return ifElse;
    }
    return nullptr;
  }
};

TEST_F(GenIfTest, ModuleExists) { ASSERT_NE(getModule("gen_test4"), nullptr) << "module 'gen_test4' not found"; }

TEST_F(GenIfTest, GenvarIExists) {
  const hldb::Module *const m = getModule("gen_test4");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr) << "'genvar i;' should produce a Variable at module scope";
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", m->getVariables()), nullptr) << "genvar 'i' not found";
}

TEST_F(GenIfTest, LoopExistsAndBodyIsNamedBeginFoo) {
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr) << "'for (i=0; i<2; i=i+1) begin : foo ... end' not found";
  const hldb::Begin *const foo = loop->getStmt<hldb::Begin>();
  ASSERT_NE(loop->getStmt(), nullptr);
  ASSERT_NE(foo, nullptr) << "loop body 'begin : foo ... end' must be a Begin";
  EXPECT_EQ(foo->getName(), std::string_view{"foo"});
}

TEST_F(GenIfTest, IfElseExistsInsideFoo) {
  const hldb::GenIfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr) << "'if (i == 0) ... else ...' not found inside 'begin : foo'";
}

// 'if (i == 0)' -- Sec 11.4.5: '==' is vpiEqOp.
TEST_F(GenIfTest, ConditionIsIEqualsZero) {
  const hldb::GenIfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Operation *const cond = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(ifElse->getCondition(), nullptr);
  ASSERT_NE(cond, nullptr) << "'i == 0' condition must be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"i"});
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

// 'assign temp1 = a;' -- taken branch, no begin/end.
TEST_F(GenIfTest, ThenBranchIsContAssignTemp1FromA) {
  const hldb::GenIfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::ContAssign *const assign = ifElse->getStmt<hldb::ContAssign>();
  ASSERT_NE(ifElse->getStmt(), nullptr);
  ASSERT_NE(assign, nullptr) << "'assign temp1 = a;' must be a ContAssign, directly the then-branch (no begin/end)";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"temp1"});
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"a"});
}

// 'assign temp2 = b;' -- else branch, no begin/end.
TEST_F(GenIfTest, ElseBranchIsContAssignTemp2FromB) {
  const hldb::GenIfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::ContAssign *const assign = ifElse->getElseStmt<hldb::ContAssign>();
  ASSERT_NE(ifElse->getElseStmt(), nullptr);
  ASSERT_NE(assign, nullptr) << "'assign temp2 = b;' must be a ContAssign, directly the else-branch (no begin/end)";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"temp2"});
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"b"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
