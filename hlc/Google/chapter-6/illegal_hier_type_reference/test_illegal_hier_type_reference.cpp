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

// IEEE 1800-2023 Sec 6.18 and 25.10: a type may be referenced hierarchically only as
// typedef port.type_name, where the prefix is an interface port. typedef p.T ok_t is legal;
// a prefix that is an interface or module instance is illegal.
// Arguments: illegal_hier_type_reference.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalHierTypeReferenceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_hier_type_reference.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalHierTypeReferenceTest, ReportsInterfaceInstancePrefix) {
  EXPECT_NE(findEvent(EventType::CompIllegalHierTypeReference, "i.T", 29, 11), nullptr)
      << "i is an interface instance, not an interface port";
}

TEST_F(IllegalHierTypeReferenceTest, ReportsModuleInstancePrefix) {
  EXPECT_NE(findEvent(EventType::CompIllegalHierTypeReference, "m.U", 30, 11), nullptr)
      << "m is a module instance";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
