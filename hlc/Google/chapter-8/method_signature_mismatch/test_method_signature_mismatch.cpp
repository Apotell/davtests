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

// IEEE 1800-2023 Sec 8.24: "The out-of-block method declaration shall match the prototype
// declaration exactly" (a default value may be omitted, as base::ok does). Sec 8.20: "Virtual
// method overrides in subclasses shall have matching argument types, identical argument names,
// identical qualifiers, and identical directions to the prototype." derived::v1 matches, and
// derived::nv overrides a non-virtual method, which needs no match.
// Arguments: method_signature_mismatch.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class MethodSignatureMismatchTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "method_signature_mismatch.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(MethodSignatureMismatchTest, ReportsOutOfBlockArgumentRenamed) {
  EXPECT_NE(findEvent(EventType::CompExternPrototypeMismatch, "f", 28, 1), nullptr)
      << "base::f names its second argument c; the prototype names it b";
}

TEST_F(MethodSignatureMismatchTest, ReportsOutOfBlockDirectionChanged) {
  EXPECT_NE(findEvent(EventType::CompExternPrototypeMismatch, "t", 29, 1), nullptr)
      << "base::t declares x output; the prototype declares it input";
}

TEST_F(MethodSignatureMismatchTest, ReportsOverrideWithExtraArgument) {
  EXPECT_NE(findEvent(EventType::CompVirtualOverrideMismatch, "v2", 33, 11), nullptr)
      << "derived::v2 adds an argument to the virtual base::v2";
}

TEST_F(MethodSignatureMismatchTest, ReportsOverrideArgumentRenamed) {
  EXPECT_NE(findEvent(EventType::CompVirtualOverrideMismatch, "v3", 34, 3), nullptr)
      << "derived::v3 names its argument z; the pure virtual base::v3 names it a";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
