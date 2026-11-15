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

// Command-line test: a read-only cache entry written by another hlc version is not used.
// Arguments (CacheRejected.hlc): -conf stale.conf -wd ../../../tests/CmdArgs top.sv -d cache
// stale.conf and the stale cache it names (stalecache/) sit next to this file.
// The design is compiled: the event is raised after the command line is parsed.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class CmdArgsCacheRejectedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "CacheRejected.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsCacheRejectedTest, ReportsCmdCacheRejected) {
  EXPECT_NE(findEvent(EventType::CmdCacheRejected), nullptr)
      << "CmdCacheRejected expected for: a read-only cache entry written by another hlc version is not used";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
