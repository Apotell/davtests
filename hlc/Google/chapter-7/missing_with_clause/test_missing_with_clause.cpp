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

// IEEE 1800-2023 Sec 7.12: "specifying an iterator_argument without also specifying a with clause
// shall be illegal". Sec 7.12.1: for the array locator methods "the with clause is mandatory".
// Sec 7.12.5: map() requires a with clause. The calls with a with clause, A.sum and IA.unique are
// legal.
// Arguments: missing_with_clause.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class MissingWithClauseTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "missing_with_clause.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(MissingWithClauseTest, ReportsLocatorWithIteratorArgument) {
  EXPECT_NE(findEvent(EventType::CompMissingWithClause, "find", 23, 10), nullptr)
      << "find() always needs a with clause";
}

TEST_F(MissingWithClauseTest, ReportsLocatorWithoutParentheses) {
  EXPECT_NE(findEvent(EventType::CompMissingWithClause, "find_index", 24, 10), nullptr)
      << "find_index needs a with clause";
}

TEST_F(MissingWithClauseTest, ReportsMap) {
  EXPECT_NE(findEvent(EventType::CompMissingWithClause, "map", 25, 9), nullptr) << "map() needs a with clause";
}

TEST_F(MissingWithClauseTest, ReportsIteratorArgumentWithoutWith) {
  EXPECT_NE(findEvent(EventType::CompMissingWithClause, "sum", 26, 9), nullptr)
      << "an iterator argument needs a with clause";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
