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

// Tests for tests/Reachability/loops.sv -- see that file for the source.
//
// A module that instantiates itself, directly or through other modules (or a bind directive),
// with no generate construct to end the recursion, can never be instantiated: ElabInstantiationLoop.
// Recursion through a generate conditional is legal (IEEE 1800-2023 Sec 27.2) and is not reported.
// None of the modules in a loop is top-level (each is instantiated, Sec 23.3.1), and none is
// reachable from loop_top.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class ReachabilityLoopsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Loops.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
};

TEST_F(ReachabilityLoopsTest, SelfInstantiationIsALoop) {
  // loops.sv:7, `self_loop u_self ();` inside self_loop.
  EXPECT_NE(findEvent(EventType::ElabInstantiationLoop, "self_loop", 7), nullptr);
}

TEST_F(ReachabilityLoopsTest, TwoModulesInstantiatingEachOtherAreALoop) {
  // One error for the loop. The standard does not say which member it names: either loop_a,
  // where loop_b instantiates it (loops.sv:15), or loop_b, where loop_a instantiates it (:11).
  EXPECT_TRUE((findEvent(EventType::ElabInstantiationLoop, "loop_a", 15) != nullptr) ||
              (findEvent(EventType::ElabInstantiationLoop, "loop_b", 11) != nullptr));
}

TEST_F(ReachabilityLoopsTest, LoopThroughABindDirectiveIsALoop) {
  // Either bind_host, where bind_src instantiates it (loops.sv:24), or bind_src, where the bind
  // directive instantiates it into bind_host (:27).
  EXPECT_TRUE((findEvent(EventType::ElabInstantiationLoop, "bind_host", 24) != nullptr) ||
              (findEvent(EventType::ElabInstantiationLoop, "bind_src", 27) != nullptr));
}

TEST_F(ReachabilityLoopsTest, RecursionEndedByAGenerateConditionIsNotALoop) {
  // Not a bare absence check: the same event is reported for the unguarded loops above, so this
  // tells the guarded recursion (IEEE 1800-2023 Sec 27.2) apart from them.
  EXPECT_EQ(findEvent(EventType::ElabInstantiationLoop, "rec"), nullptr);
}

TEST_F(ReachabilityLoopsTest, LoopTopIsTheOnlyTopLevel) {
  ASSERT_NE(getModule("loop_top"), nullptr);
  EXPECT_TRUE(isTopLevel(getModule("loop_top")));
  EXPECT_EQ(getTopLevelCount(), 1u);
}

TEST_F(ReachabilityLoopsTest, ModulesInALoopAreNotTopLevel) {
  for (const std::string_view name : {"self_loop", "loop_a", "loop_b", "bind_host", "bind_src"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << name;
    EXPECT_FALSE(isTopLevel(m)) << name;
  }
}

TEST_F(ReachabilityLoopsTest, ModulesInALoopAreNotReachable) {
  for (const std::string_view name : {"self_loop", "loop_a", "loop_b", "bind_host", "bind_src"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << name;
    EXPECT_FALSE(isReachable(m)) << name;
  }
}

TEST_F(ReachabilityLoopsTest, GuardedRecursiveModuleIsReachable) {
  ASSERT_NE(getModule("rec"), nullptr);
  EXPECT_TRUE(isReachable(getModule("rec")));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
