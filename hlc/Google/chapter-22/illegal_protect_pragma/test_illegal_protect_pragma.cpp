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

// IEEE 1800-2023 Sec 34.3: an encryption envelope is opened by `pragma protect begin and closed by
// `pragma protect end, and a decryption envelope by begin_protected/end_protected. An end_protected
// with no open decryption envelope, and an envelope still open at the end of the file, are malformed.
// The begin/end pair on lines 20-22 and the author keyword on line 25 are legal. The second end on
// line 23 is not an error: Sec 34.5.2 uses end only to mark, in the cleartext input to encryption,
// where the region to encrypt ends ("DECRYPTION INPUT: none"), and no clause makes an end without a
// begin an error.
// Arguments: illegal_protect_pragma.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalProtectPragmaTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_protect_pragma.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalProtectPragmaTest, ReportsEndProtectedWithoutBegin) {
  EXPECT_NE(findEvent(EventType::PpIllegalProtectPragma, "protect end_protected", 24, 1), nullptr) << "no begin_protected";
}

TEST_F(IllegalProtectPragmaTest, ReportsUnclosedBegin) {
  EXPECT_NE(findEvent(EventType::PpIllegalProtectPragma, "protect begin", 26, 1), nullptr) << "begin never closed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
