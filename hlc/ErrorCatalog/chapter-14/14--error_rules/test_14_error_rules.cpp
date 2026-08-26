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

// Tests for the IEEE 1800-2023 Clause 14 (Clocking blocks) error scenarios
// catalogued in sv_error_catalog_Latest.xlsx (rows 439, 444).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// both modules compile. The only diagnostic is a CP5851 "Failed to bind cb"
// warning at 15:15 -- HLC does not resolve the clockvar reference cb.b at all,
// which is a binding gap, not the direction check row 439 calls for. It is not
// asserted here: it would pass for the wrong reason and would keep passing
// after cb.b starts binding correctly, at which point the real rule would be
// silently unchecked.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter14ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "14--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 439: output clockvars are write-only (14.3) ------------------------

TEST_F(Chapter14ErrorRulesTest, Row439_ReadingAnOutputClockvarIsRejected) {
  // catalog row 439 | 14.3 | COMP
  // "It shall be illegal to read the value of any clockvar whose
  // clocking_direction is output." cb declares b as output on line 13; line 15
  // reads cb.b.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 14.3 makes reading a clockvar whose "
                  "clocking direction is output illegal";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_READ, "b", 15, 15), nullptr)
      << "an output clockvar cannot be read (IEEE 1800-2023 14.3)";
}

// --- row 444: cycle delays need a default clocking (14.11) ------------------

TEST_F(Chapter14ErrorRulesTest, Row444_CycleDelayWithoutDefaultClockingIsRejected) {
  // catalog row 444 | 14.11 | COMP
  // "If no default clocking has been specified for the current module,
  // interface, checker, or program, use of the ## operator shall cause the
  // compiler to issue an error." r444_m declares cb on line 24 but never makes
  // it the default, and uses ##5 on line 27.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 14.11 requires an error when a ## "
                  "cycle delay is used with no default clocking in scope";
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_DEFAULT_CLOCKING, "r444_m", 27, 5), nullptr)
      << "a ## cycle delay needs a default clocking in scope (IEEE 1800-2023 14.11)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
