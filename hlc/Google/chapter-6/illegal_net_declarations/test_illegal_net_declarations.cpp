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

// IEEE 1800-2023 Sec 6.7.1, net_declaration footnotes:
//   "A charge strength shall only be used with the trireg keyword."           (w_charge, line 22)
//   A drive strength shall only be used with a net declaration assignment.   (w_drive, line 24)
//   "When the vectored or scalared keyword is used, there shall be at least
//   one packed dimension."                                       (v_scalar, s_scalar, lines 26-27)
// Arguments: illegal_net_declarations.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalNetDeclarationsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_net_declarations.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalNetDeclarationsTest, ReportsChargeStrengthOnNonTriregNet) {
  EXPECT_NE(findEvent(EventType::CompIllegalChargeStrength, "w_charge", 22, 16), nullptr)
      << "a charge strength on a wire";
}

TEST_F(IllegalNetDeclarationsTest, ReportsDriveStrengthWithoutAssignment) {
  EXPECT_NE(findEvent(EventType::CompIllegalDriveStrength, "w_drive", 24, 25), nullptr)
      << "a drive strength with no net declaration assignment";
}

TEST_F(IllegalNetDeclarationsTest, ReportsVectoredWithoutPackedDimension) {
  EXPECT_NE(findEvent(EventType::CompIllegalVectoredNet, "v_scalar", 26, 17), nullptr)
      << "vectored on a net with no packed dimension";
}

TEST_F(IllegalNetDeclarationsTest, ReportsScalaredWithoutPackedDimension) {
  EXPECT_NE(findEvent(EventType::CompIllegalVectoredNet, "s_scalar", 27, 17), nullptr)
      << "scalared on a net with no packed dimension";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
