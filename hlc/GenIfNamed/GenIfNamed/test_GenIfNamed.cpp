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
// introduce named generate blocks). Per the object model, GenIfElse itself
// carries the condition, and its "then"/"else" branches -- each a named
// block -- are represented as GenScope objects whose getName() (Scope::
// getName(), Sec 23.9 "Scope rules") must equal the source label ("tag2",
// "tag3"). The enclosing generate-for loop is itself a named generate
// construct ("begin: tag1"); GenFor derives from GenScope and so also
// exposes getName() == "tag1".
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\davtests\davtests_02\build\include\hldb.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
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

// The generate-for loop itself is named 'tag1' (Sec 27.3/27.4).
TEST_F(GenIfNamedTest, GenForIsNamedTag1) {
  const hldb::GenFor *const gf = findGenFor(getTop());
  ASSERT_NE(gf, nullptr) << "'for (...) begin: tag1 ... end' not found";
  EXPECT_EQ(gf->getName(), std::string_view("tag1"));
}

// Body of 'tag1' is a single 'if (1) begin: tag2 ... end else begin: tag3
// ... end' -- a GenIfElse (Sec 27.5).
TEST_F(GenIfNamedTest, GenForBodyIsGenIfElse) {
  const hldb::GenFor *const gf = findGenFor(getTop());
  ASSERT_NE(gf, nullptr);
  const hldb::GenIfElse *const gie = gf->getStmt<hldb::GenIfElse>();
  ASSERT_NE(gf->getStmt(), nullptr) << "'tag1' has no body statement";
  ASSERT_NE(gie, nullptr) << "'tag1' body should be a GenIfElse";
}

// The 'then' branch 'begin: tag2 ... end' must be a GenScope named 'tag2'.
TEST_F(GenIfNamedTest, GenIfElseThenBranchIsNamedTag2) {
  const hldb::GenFor *const gf = findGenFor(getTop());
  ASSERT_NE(gf, nullptr);
  const hldb::GenIfElse *const gie = gf->getStmt<hldb::GenIfElse>();
  ASSERT_NE(gie, nullptr);

  ASSERT_NE(gie->getStmt(), nullptr) << "'tag2' branch missing";
  const hldb::GenScope *const tag2 = gie->getStmt<hldb::GenScope>();
  ASSERT_NE(tag2, nullptr) << "'begin: tag2 ... end' should be a GenScope";
  EXPECT_EQ(tag2->getName(), std::string_view("tag2"));

  ASSERT_NE(tag2->getContAssigns(), nullptr);
  ASSERT_EQ(tag2->getContAssigns()->size(), 1u) << "'tag2' has exactly one continuous assignment";
}

// The 'else' branch 'begin: tag3 ... end' must be a GenScope named 'tag3'
// with its own two continuous assignments.
TEST_F(GenIfNamedTest, GenIfElseElseBranchIsNamedTag3) {
  const hldb::GenFor *const gf = findGenFor(getTop());
  ASSERT_NE(gf, nullptr);
  const hldb::GenIfElse *const gie = gf->getStmt<hldb::GenIfElse>();
  ASSERT_NE(gie, nullptr);

  ASSERT_NE(gie->getElseStmt(), nullptr) << "'tag3' branch missing";
  const hldb::GenScope *const tag3 = gie->getElseStmt<hldb::GenScope>();
  ASSERT_NE(tag3, nullptr) << "'begin: tag3 ... end' should be a GenScope";
  EXPECT_EQ(tag3->getName(), std::string_view("tag3"));

  ASSERT_NE(tag3->getContAssigns(), nullptr);
  EXPECT_EQ(tag3->getContAssigns()->size(), 2u) << "'tag3' has exactly two continuous assignments";
}

// 'tag2' and 'tag3' are two distinct, sibling named generate scopes -- not
// the same object and not sharing a name.
TEST_F(GenIfNamedTest, Tag2AndTag3AreDistinctScopes) {
  const hldb::GenFor *const gf = findGenFor(getTop());
  ASSERT_NE(gf, nullptr);
  const hldb::GenIfElse *const gie = gf->getStmt<hldb::GenIfElse>();
  ASSERT_NE(gie, nullptr);
  const hldb::GenScope *const tag2 = gie->getStmt<hldb::GenScope>();
  const hldb::GenScope *const tag3 = gie->getElseStmt<hldb::GenScope>();
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
