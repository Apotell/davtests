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

// Tests for tests/Reachability/lib.sv -- a library: packages and classes, no module or program.
// A library is a collection of objects, not a design: it has no top-level module and needs none.
// Everything in it is compiled and bound, nothing is reachable, and the missing top-level module
// is not reported.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/package.h>
#include <hldb/unit.h>

#include <gtest/gtest.h>

namespace hlc {

class ReachabilityLibraryTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Library.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ReachabilityLibraryTest, LibraryIsCompiled) {
  const hldb::Package *const pkg = hldb::findByName<hldb::Package>("lib_pkg", m_design->getAllPackages());
  ASSERT_NE(pkg, nullptr);
  EXPECT_NE(hldb::findByDefName<hldb::ClassDefn>("LibItem", pkg->getClassDefns()), nullptr);
  ASSERT_NE(m_unit, nullptr);
  EXPECT_NE(hldb::findByDefName<hldb::ClassDefn>("LibStandalone", m_unit->getClassDefns()), nullptr);
}

TEST_F(ReachabilityLibraryTest, LibraryHasNoTopLevel) { EXPECT_EQ(getTopLevelCount(), 0u); }

TEST_F(ReachabilityLibraryTest, NothingInTheLibraryIsReachable) {
  const hldb::Package *const pkg = hldb::findByName<hldb::Package>("lib_pkg", m_design->getAllPackages());
  ASSERT_NE(pkg, nullptr);
  EXPECT_FALSE(isReachable(pkg));
  ASSERT_NE(m_unit, nullptr);
  const hldb::ClassDefn *const cls = hldb::findByDefName<hldb::ClassDefn>("LibStandalone", m_unit->getClassDefns());
  ASSERT_NE(cls, nullptr);
  EXPECT_FALSE(isReachable(cls));
}

TEST_F(ReachabilityLibraryTest, MissingTopLevelIsNotReportedForALibrary) {
  // Not a bare absence check: the same event is reported for a design whose modules include no
  // top-level module (Reachability/TopMissing); a library is told apart from that case.
  EXPECT_EQ(findEvent(EventType::ElabNoTopLevelModule), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
