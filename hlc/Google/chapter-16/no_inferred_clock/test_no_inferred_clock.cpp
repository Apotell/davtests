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

// IEEE 1800-2023 Sec 16.16: a concurrent assertion shall have a clock: its own clocking event, a
// clock inferred from an enclosing procedure with an event control (Sec 16.14.6), or the default
// clocking of its scope (Sec 14.12). A_PLAIN, C_PLAIN, A_INIT (an initial procedure has no event
// control) and A_PROP (a named property without a clock) have none. A_EXPL, A_NAMED, A_PROC and
// A_DEF are clocked.
// Arguments: no_inferred_clock.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assert_stmt.h>
#include <hldb/clocking_block.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/property_decl.h>
#include <hldb/property_inst.h>
#include <hldb/property_spec.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class NoInferredClockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "no_inferred_clock.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *findModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  // Assertions are named by their label, which is not their vpiName.
  static const hldb::ConcurrentAssertions *findAssertion(const hldb::Module *module, std::string_view label) {
    if ((module == nullptr) || (module->getConcurrentAssertions() == nullptr)) return nullptr;
    for (const hldb::ConcurrentAssertions *assertion : *module->getConcurrentAssertions()) {
      if ((assertion != nullptr) && (assertion->getLabel() == label)) return assertion;
    }
    return nullptr;
  }
};

TEST_F(NoInferredClockTest, ReportsUnclockedAssert) {
  EXPECT_NE(findEvent(EventType::CompNoInferredClock, "A_PLAIN", 19, 12), nullptr);
}

TEST_F(NoInferredClockTest, ReportsUnclockedCover) {
  EXPECT_NE(findEvent(EventType::CompNoInferredClock, "C_PLAIN", 20, 12), nullptr);
}

TEST_F(NoInferredClockTest, ReportsAssertInInitialProcedure) {
  EXPECT_NE(findEvent(EventType::CompNoInferredClock, "A_INIT", 22, 13), nullptr);
}

TEST_F(NoInferredClockTest, ReportsInstanceOfUnclockedProperty) {
  EXPECT_NE(findEvent(EventType::CompNoInferredClock, "A_PROP", 42, 11), nullptr);
}

TEST_F(NoInferredClockTest, ExplicitClockIsOnThePropertySpec) {
  const hldb::ConcurrentAssertions *const a = findAssertion(findModule("m_clocked"), "A_EXPL");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getProperty(), nullptr);
  ASSERT_EQ(a->getProperty()->getAnyType(), hldb::AnyType::PropertySpec);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>()->getClockingEvent(), nullptr);
}

TEST_F(NoInferredClockTest, NamedPropertyCarriesItsClock) {
  const hldb::ConcurrentAssertions *const a = findAssertion(findModule("m_clocked"), "A_NAMED");
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *const spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  ASSERT_NE(spec->getPropertyExpr(), nullptr);
  ASSERT_EQ(spec->getPropertyExpr()->getAnyType(), hldb::AnyType::PropertyInst);
  const hldb::PropertyDecl *const decl = spec->getPropertyExpr<hldb::PropertyInst>()->getPropertyDecl();
  ASSERT_NE(decl, nullptr);
  EXPECT_EQ(decl->getName(), std::string_view("p_clk"));
  ASSERT_NE(decl->getPropertySpec(), nullptr);
  EXPECT_NE(decl->getPropertySpec()->getClockingEvent(), nullptr);
}

TEST_F(NoInferredClockTest, DefaultClockingOfTheScope) {
  const hldb::Module *const m = findModule("m_default");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getDefaultClocking(), nullptr);
  EXPECT_EQ(m->getDefaultClocking()->getName(), std::string_view("cb"));
  EXPECT_NE(findAssertion(m, "A_DEF"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
