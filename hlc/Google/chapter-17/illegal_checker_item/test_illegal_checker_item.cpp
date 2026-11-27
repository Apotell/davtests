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

// IEEE 1800-2023 Sec 17.7.1: in the always_ff procedures of a checker "only nonblocking assignments
// are allowed": the blocking v = in1 on line 21 (v <= in1 on line 22 is legal). Sec 17.2: "An input
// checker formal argument shall not be modified by a checker": in1 on line 24. The assignment to the
// output formal out1 (line 25) is legal.
// Arguments: illegal_checker_item.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/checker_decl.h>
#include <hldb/checker_port.h>
#include <hldb/design.h>
#include <hldb/unit.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class IllegalCheckerItemTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_checker_item.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::CheckerDecl *findChecker(std::string_view name) {
    if (m_design->getUnits() == nullptr) return nullptr;
    for (const hldb::Unit *unit : *m_design->getUnits()) {
      if (const hldb::CheckerDecl *const c = hldb::findByName<hldb::CheckerDecl>(name, unit->getCheckerDecls())) {
        return c;
      }
    }
    return nullptr;
  }
};

TEST_F(IllegalCheckerItemTest, BlockingAssignmentInAlwaysFf) {
  EXPECT_NE(findEvent(EventType::CompIllegalCheckerItem, "ck", 21, 5), nullptr);
}

TEST_F(IllegalCheckerItemTest, InputFormalModified) {
  EXPECT_NE(findEvent(EventType::CompIllegalCheckerItem, "ck", 24, 15), nullptr);
}

TEST_F(IllegalCheckerItemTest, FormalDirections) {
  const hldb::CheckerDecl *const ck = findChecker("ck");
  ASSERT_NE(ck, nullptr);
  const hldb::CheckerPort *const in1 = hldb::findByName<hldb::CheckerPort>("in1", ck->getPorts());
  const hldb::CheckerPort *const out1 = hldb::findByName<hldb::CheckerPort>("out1", ck->getPorts());
  ASSERT_NE(in1, nullptr);
  ASSERT_NE(out1, nullptr);
  EXPECT_EQ(in1->getDirection(), vpiInput);
  EXPECT_EQ(out1->getDirection(), vpiOutput);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
