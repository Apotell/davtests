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

// Source encoding (an HLC rule, not an IEEE 1800 one): a UTF-16 source file cannot be
// read: only UTF-8 is supported.
// Reported at line 1 of utf16_source.sv.
// Arguments: Utf16Source.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class EncodingUtf16SourceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Utf16Source.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EncodingUtf16SourceTest, ReportsPpCannotReadFileContent) {
  EXPECT_NE(findEvent(EventType::PpCannotReadFileContent, 1, 1), nullptr)
      << "a UTF-16 source file cannot be read: only UTF-8 is supported";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
