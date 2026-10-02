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

// Tests for GenIfNamed.hlc (tests/GenIfNamed/dut.sv):
//
//   module top();
//     for (i = 0; i < 3 ; i = i + 1) begin: tag1
//        if (1) begin: tag2
//           assign tmp[i] = 1'b1;
//        end else begin: tag3
//           assign tmp[i] = 1'b0;
//           assign tmp2[i] = 1'b1;
//        end
//     end
//   endmodule
//
// Compiled at "-d ast" level (no "-d inst"), so the generate constructs
// survive as raw GenFor/GenIfElse objects on the module's getGenStmts()
// (IEEE 1800-2023 Sec 27.4 "Generate-loop constructs", Sec 27.5
// "Generate-if constructs") rather than being collapsed into an elaborated,
// per-iteration GenScopeArray/GenScope pair.
//
// What is under test: a *named* generate-if construct (IEEE 1800-2023 Sec
// 27.3 "Generate block": "begin : tag2 ... end" / "begin : tag3 ... end"
// introduce named generate blocks). GenIfElse itself carries the
// condition, and its "then"/"else" branches -- each a named generate
// block -- are, in this unelaborated model, Begin objects whose getName()
// must equal the source label ("tag2", "tag3") and whose getStmts() hold
// the block's continuous assignments. The label "tag1" names the
// generate-for loop's generate block (the Begin that is the GenFor's
// getStmt()), not the loop construct itself -- the same shape the suite
// uses in GenFor/GenFor/test_GenFor.cpp and GenIf/GenIf/test_GenIf.cpp.
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\davtests\davtests_02\build\include\hldb.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if_else.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenIfNamedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenIfNamed.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::GenFor *findGenFor(const hldb::Module *m) {
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenFor *const gf = any_cast<hldb::GenFor>(stmt)) return gf;
    }
    return nullptr;
  }

  // GenFor -> getStmt<Begin>() 'tag1' -> its sole item, the GenIfElse.
  static const hldb::GenIfElse *findGenIfElse() {
    const hldb::GenFor *const gf = findGenFor(getTop());
    if (gf == nullptr) return nullptr;
    const hldb::Begin *const tag1 = gf->getStmt<hldb::Begin>();
    if (tag1 == nullptr || tag1->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *tag1->getStmts()) {
      if (const hldb::GenIfElse *const gie = any_cast<hldb::GenIfElse>(stmt)) return gie;
    }
    return nullptr;
  }

  static size_t countContAssigns(const hldb::Begin *blk) {
    size_t count = 0u;
    if (blk == nullptr || blk->getStmts() == nullptr) return count;
    for (const hldb::Any *const stmt : *blk->getStmts()) {
      if (any_cast<hldb::ContAssign>(stmt) != nullptr) ++count;
    }
    return count;
  }
};

TEST_F(GenIfNamedTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr); }

// 'for (...) begin: tag1 ... end' -- exactly one generate-for on 'top'.
TEST_F(GenIfNamedTest, ModuleTopHasExactlyOneGenFor) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr) << "'top' has no generate statements";
  size_t count = 0u;
  for (const hldb::Any *const stmt : *top->getGenStmts()) {
    if (any_cast<hldb::GenFor>(stmt) != nullptr) ++count;
  }
  EXPECT_EQ(count, 1u);
}

// The generate-for loop's generate block is named 'tag1' (Sec 27.3/27.4).
TEST_F(GenIfNamedTest, GenForIsNamedTag1) {
  const hldb::GenFor *const gf = findGenFor(getTop());
  ASSERT_NE(gf, nullptr) << "'for (...) begin: tag1 ... end' not found";
  // The label names the loop's generate block, not the GenFor construct:
  // EXPECT_EQ(gf->getName(), std::string_view("tag1"));
  ASSERT_NE(gf->getStmt(), nullptr) << "'tag1' has no body";
  const hldb::Begin *const tag1 = gf->getStmt<hldb::Begin>();
  ASSERT_NE(tag1, nullptr) << "loop body 'begin: tag1 ... end' should be a Begin";
  EXPECT_EQ(tag1->getName(), std::string_view("tag1"));
}

// Body of 'tag1' is a single 'if (1) begin: tag2 ... end else begin: tag3
// ... end' -- a GenIfElse (Sec 27.5).
TEST_F(GenIfNamedTest, GenForBodyIsGenIfElse) {
  const hldb::GenFor *const gf = findGenFor(getTop());
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getStmt(), nullptr) << "'tag1' has no body statement";
  const hldb::Begin *const tag1 = gf->getStmt<hldb::Begin>();
  ASSERT_NE(tag1, nullptr);
  ASSERT_NE(tag1->getStmts(), nullptr);
  ASSERT_EQ(tag1->getStmts()->size(), 1u) << "'tag1' holds exactly one generate item";
  const hldb::GenIfElse *const gie = any_cast<hldb::GenIfElse>(tag1->getStmts()->at(0));
  ASSERT_NE(gie, nullptr) << "'tag1' body should be a GenIfElse";
}

// The 'then' branch 'begin: tag2 ... end' must be a Begin named 'tag2'.
TEST_F(GenIfNamedTest, GenIfElseThenBranchIsNamedTag2) {
  const hldb::GenIfElse *const gie = findGenIfElse();
  ASSERT_NE(gie, nullptr);

  ASSERT_NE(gie->getStmt(), nullptr) << "'tag2' branch missing";
  const hldb::Begin *const tag2 = gie->getStmt<hldb::Begin>();
  ASSERT_NE(tag2, nullptr) << "'begin: tag2 ... end' should be a Begin";
  EXPECT_EQ(tag2->getName(), std::string_view("tag2"));

  ASSERT_NE(tag2->getStmts(), nullptr);
  ASSERT_EQ(countContAssigns(tag2), 1u) << "'tag2' has exactly one continuous assignment";
}

// The 'else' branch 'begin: tag3 ... end' must be a Begin named 'tag3'
// with its own two continuous assignments.
TEST_F(GenIfNamedTest, GenIfElseElseBranchIsNamedTag3) {
  const hldb::GenIfElse *const gie = findGenIfElse();
  ASSERT_NE(gie, nullptr);

  ASSERT_NE(gie->getElseStmt(), nullptr) << "'tag3' branch missing";
  const hldb::Begin *const tag3 = gie->getElseStmt<hldb::Begin>();
  ASSERT_NE(tag3, nullptr) << "'begin: tag3 ... end' should be a Begin";
  EXPECT_EQ(tag3->getName(), std::string_view("tag3"));

  ASSERT_NE(tag3->getStmts(), nullptr);
  EXPECT_EQ(countContAssigns(tag3), 2u) << "'tag3' has exactly two continuous assignments";
}

// 'tag2' and 'tag3' are two distinct, sibling named generate scopes -- not
// the same object and not sharing a name.
TEST_F(GenIfNamedTest, Tag2AndTag3AreDistinctScopes) {
  const hldb::GenIfElse *const gie = findGenIfElse();
  ASSERT_NE(gie, nullptr);
  ASSERT_NE(gie->getStmt(), nullptr);
  ASSERT_NE(gie->getElseStmt(), nullptr);
  const hldb::Begin *const tag2 = gie->getStmt<hldb::Begin>();
  const hldb::Begin *const tag3 = gie->getElseStmt<hldb::Begin>();
  ASSERT_NE(tag2, nullptr);
  ASSERT_NE(tag3, nullptr);
  EXPECT_NE(static_cast<const void *>(tag2), static_cast<const void *>(tag3));
  EXPECT_NE(tag2->getName(), tag3->getName());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
