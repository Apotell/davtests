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

// IEEE 1800-2023 Sec 31.9.1: "A set of negative timing check values might be mutually inconsistent
// and produce no solution for the delay values of the delayed signals"; the tool shall warn. The
// check on TI has no solution by itself (setup + hold < 0); the $recrem on A is consistent alone but
// not together with the $setuphold on A. The checks on D and B are consistent.
// Arguments: inconsistent_timing_check.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class InconsistentTimingCheckTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "inconsistent_timing_check.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(InconsistentTimingCheckTest, ReportsSingleCheckWithoutSolution) {
  EXPECT_NE(findEvent(EventType::CompInconsistentTimingCheck, "$setuphold(CP, TI)", 23, 5), nullptr)
      << "setup -30 and hold -20 leave no window";
}

TEST_F(InconsistentTimingCheckTest, ReportsMutuallyInconsistentChecks) {
  EXPECT_NE(findEvent(EventType::CompInconsistentTimingCheck, "$recrem(CP, A)", 25, 5), nullptr)
      << "removal -11 conflicts with setup -10 on the same signals";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
