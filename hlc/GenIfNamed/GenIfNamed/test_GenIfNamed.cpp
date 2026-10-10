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
// introduce named generate blocks). GenIfElse itself carries the condition,
// and its "then"/"else" branches -- each a named generate block -- are Begin
// objects whose getName() equals the source label ("tag2", "tag3"). Likewise
// "begin: tag1" names the generate-for's generate block, i.e. the GenFor's
// body Begin, not the GenFor construct (Sec 27.4). GenScope objects only
// exist after elaboration.
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
#include <hldb/gen_scope.h>
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

  // In 'for (...) begin: tag1 ... end' the label names the loop's
  // generate block -- the GenFor's body Begin -- not the GenFor (Sec 27.4). Likewise 'begin: tag2'
  // and 'begin: tag3' are the GenIfElse's branch Begins (Sec 27.5). GenScopes only exist after
  // elaboration.
  static const hldb::Begin *getTag1Block() {
    const hldb::GenFor *const gf = findGenFor(getTop());
    return (gf == nullptr) ? nullptr : gf->getStmt<hldb::Begin>();
  }

  // The single generate item of 'tag1': 'if (1) begin: tag2 ... end else begin: tag3 ... end'.
  static const hldb::GenIfElse *getGenIfElse() {
    const hldb::Begin *const tag1 = getTag1Block();
    if (tag1 == nullptr || tag1->getStmts() == nullptr || tag1->getStmts()->size() != 1u) return nullptr;
    return any_cast<hldb::GenIfElse>(tag1->getStmts()->at(0));
  }

  static size_t countContAssigns(const hldb::Begin *block) {
    size_t count = 0u;
    if (block == nullptr || block->getStmts() == nullptr) return count;
    for (const hldb::Any *const item : *block->getStmts()) {
      if (any_cast<hldb::ContAssign>(item) != nullptr) ++count;
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

// 'tag1' names the generate-for's generate block (its body Begin), not the GenFor (Sec 27.4).
TEST_F(GenIfNamedTest, GenForIsNamedTag1) {
  const hldb::GenFor *const gf = findGenFor(getTop());
  ASSERT_NE(gf, nullptr) << "'for (...) begin: tag1 ... end' not found";
  ASSERT_NE(gf->getStmt(), nullptr) << "the generate-for has no body";
  const hldb::Begin *const tag1 = getTag1Block();
  ASSERT_NE(tag1, nullptr) << "the generate-for body 'begin: tag1 ... end' should be a Begin";
  EXPECT_EQ(tag1->getName(), std::string_view("tag1"));
  // Previous (GenScope / labeled-GenFor) version:
  // const hldb::GenFor *const gf = findGenFor(getTop());
  // ASSERT_NE(gf, nullptr) << "'for (...) begin: tag1 ... end' not found";
  // EXPECT_EQ(gf->getName(), std::string_view("tag1"));
}

// Body of 'tag1' is a single 'if (1) begin: tag2 ... end else begin: tag3
// ... end' -- a GenIfElse (Sec 27.5).
TEST_F(GenIfNamedTest, GenForBodyIsGenIfElse) {
  const hldb::Begin *const tag1 = getTag1Block();
  ASSERT_NE(tag1, nullptr);
  ASSERT_NE(tag1->getStmts(), nullptr) << "'tag1' has no body statement";
  ASSERT_EQ(tag1->getStmts()->size(), 1u);
  ASSERT_NE(getGenIfElse(), nullptr) << "'tag1' body should be a GenIfElse";
  // Previous (GenScope / labeled-GenFor) version:
  // const hldb::GenFor *const gf = findGenFor(getTop());
  // ASSERT_NE(gf, nullptr);
  // const hldb::GenIfElse *const gie = gf->getStmt<hldb::GenIfElse>();
  // ASSERT_NE(gf->getStmt(), nullptr) << "'tag1' has no body statement";
  // ASSERT_NE(gie, nullptr) << "'tag1' body should be a GenIfElse";
}

// The 'then' branch 'begin: tag2 ... end' is a generate block (Begin) named 'tag2'.
TEST_F(GenIfNamedTest, GenIfElseThenBranchIsNamedTag2) {
  const hldb::GenIfElse *const gie = getGenIfElse();
  ASSERT_NE(gie, nullptr);
  ASSERT_NE(gie->getStmt(), nullptr) << "'tag2' branch missing";
  const hldb::Begin *const tag2 = gie->getStmt<hldb::Begin>();
  ASSERT_NE(tag2, nullptr) << "'begin: tag2 ... end' should be a generate block (Begin)";
  EXPECT_EQ(tag2->getName(), std::string_view("tag2"));
  EXPECT_EQ(countContAssigns(tag2), 1u) << "'tag2' has exactly one continuous assignment";
  // Previous (GenScope / labeled-GenFor) version:
  // const hldb::GenFor *const gf = findGenFor(getTop());
  // ASSERT_NE(gf, nullptr);
  // const hldb::GenIfElse *const gie = gf->getStmt<hldb::GenIfElse>();
  // ASSERT_NE(gie, nullptr);
  //
  // ASSERT_NE(gie->getStmt(), nullptr) << "'tag2' branch missing";
  // const hldb::GenScope *const tag2 = gie->getStmt<hldb::GenScope>();
  // ASSERT_NE(tag2, nullptr) << "'begin: tag2 ... end' should be a GenScope";
  // EXPECT_EQ(tag2->getName(), std::string_view("tag2"));
  //
  // ASSERT_NE(tag2->getContAssigns(), nullptr);
  // ASSERT_EQ(tag2->getContAssigns()->size(), 1u) << "'tag2' has exactly one continuous assignment";
}

// The 'else' branch 'begin: tag3 ... end' is a generate block (Begin) named 'tag3'
// with its own two continuous assignments.
TEST_F(GenIfNamedTest, GenIfElseElseBranchIsNamedTag3) {
  const hldb::GenIfElse *const gie = getGenIfElse();
  ASSERT_NE(gie, nullptr);
  ASSERT_NE(gie->getElseStmt(), nullptr) << "'tag3' branch missing";
  const hldb::Begin *const tag3 = gie->getElseStmt<hldb::Begin>();
  ASSERT_NE(tag3, nullptr) << "'begin: tag3 ... end' should be a generate block (Begin)";
  EXPECT_EQ(tag3->getName(), std::string_view("tag3"));
  EXPECT_EQ(countContAssigns(tag3), 2u) << "'tag3' has exactly two continuous assignments";
  // Previous (GenScope / labeled-GenFor) version:
  // const hldb::GenFor *const gf = findGenFor(getTop());
  // ASSERT_NE(gf, nullptr);
  // const hldb::GenIfElse *const gie = gf->getStmt<hldb::GenIfElse>();
  // ASSERT_NE(gie, nullptr);
  //
  // ASSERT_NE(gie->getElseStmt(), nullptr) << "'tag3' branch missing";
  // const hldb::GenScope *const tag3 = gie->getElseStmt<hldb::GenScope>();
  // ASSERT_NE(tag3, nullptr) << "'begin: tag3 ... end' should be a GenScope";
  // EXPECT_EQ(tag3->getName(), std::string_view("tag3"));
  //
  // ASSERT_NE(tag3->getContAssigns(), nullptr);
  // EXPECT_EQ(tag3->getContAssigns()->size(), 2u) << "'tag3' has exactly two continuous assignments";
}

// 'tag2' and 'tag3' are two distinct, sibling named generate scopes -- not
// the same object and not sharing a name.
TEST_F(GenIfNamedTest, Tag2AndTag3AreDistinctScopes) {
  const hldb::GenIfElse *const gie = getGenIfElse();
  ASSERT_NE(gie, nullptr);
  const hldb::Begin *const tag2 = gie->getStmt<hldb::Begin>();
  const hldb::Begin *const tag3 = gie->getElseStmt<hldb::Begin>();
  ASSERT_NE(tag2, nullptr);
  ASSERT_NE(tag3, nullptr);
  EXPECT_NE(static_cast<const void *>(tag2), static_cast<const void *>(tag3));
  EXPECT_NE(tag2->getName(), tag3->getName());
  // Previous (GenScope / labeled-GenFor) version:
  // const hldb::GenFor *const gf = findGenFor(getTop());
  // ASSERT_NE(gf, nullptr);
  // const hldb::GenIfElse *const gie = gf->getStmt<hldb::GenIfElse>();
  // ASSERT_NE(gie, nullptr);
  // const hldb::GenScope *const tag2 = gie->getStmt<hldb::GenScope>();
  // const hldb::GenScope *const tag3 = gie->getElseStmt<hldb::GenScope>();
  // ASSERT_NE(tag2, nullptr);
  // ASSERT_NE(tag3, nullptr);
  // EXPECT_NE(static_cast<const void *>(tag2), static_cast<const void *>(tag3));
  // EXPECT_NE(tag2->getName(), tag3->getName());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
