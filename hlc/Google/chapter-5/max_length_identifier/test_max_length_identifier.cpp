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

// IEEE 1800-2023 Sec 5.6: "The maximum length of an identifier is implementation-specific, but shall be
// at least 1024 characters." HLC supports exactly 1024, so the 1025-character identifier on line 20 is
// reported and the 1024-character one on line 21 is accepted.
// Arguments: max_length_identifier.hlc

#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

#include <string>

namespace hlc {

class MaxLengthIdentifierTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "max_length_identifier.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(MaxLengthIdentifierTest, ReportsIdentifierOverMaximum) {
  // Sec 5.6: "If an identifier exceeds the implementation-specific length limit, an error shall be reported."
  const Event *const event = findEvent(EventType::PaMaxLengthIdentifier, 20);
  ASSERT_NE(event, nullptr) << "the 1025-character identifier on line 20 must be reported";
  ASSERT_FALSE(event->getArguments().empty());
  EXPECT_EQ(event->getArguments().front().m_value, std::string(1025, 'a'));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
