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

// IEEE 1800-2023 Sec 33.4.1.4: "It shall be an error if a library name is included in a cell
// selection clause and the corresponding expansion clause is a library list expansion clause."
// Sec 33.4.1.3: the instance name of an instance clause starts at a top-level cell of the design
// statement. cell m liblist lib2 and instance top.u2 are legal.
// Arguments: illegal_config.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalConfigTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_config.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalConfigTest, ReportsLibraryQualifiedCellWithLiblist) {
  EXPECT_NE(findEvent(EventType::LibIllegalCellName, "lib2.m", 22, 3), nullptr) << "cell lib2.m liblist lib1";
}

TEST_F(IllegalConfigTest, ReportsInstanceNotUnderTopCell) {
  EXPECT_NE(findEvent(EventType::LibIllegalConfig, "other.u1", 24, 3), nullptr) << "other is not the top cell top";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
