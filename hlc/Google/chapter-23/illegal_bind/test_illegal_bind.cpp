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

// IEEE 1800-2023 Sec 23.11: "A bind target scope shall be a module or an interface": the bind into
// program prg (line 40) is illegal. "It shall be an error for a bind_instantiation to introduce an
// instance name that clashes with another name in the module name space of the target scope":
// u_clash (41) and sig (42) are already declared in module target. The binds into module target
// (u_ok, line 38) and interface itf (u_itf, 39) are legal.
// Arguments: illegal_bind.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bind_directive.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/unit.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class IllegalBindTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_bind.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::BindDirective *findBind(std::string_view instance) {
    if (m_design->getUnits() == nullptr) return nullptr;
    for (const hldb::Unit *unit : *m_design->getUnits()) {
      if (unit->getBindDirectives() == nullptr) continue;
      for (const hldb::BindDirective *b : *unit->getBindDirectives()) {
        const hldb::RefInstance *const source = b->getBindSourceInstance<hldb::RefInstance>();
        if ((source != nullptr) && (source->getName() == instance)) return b;
      }
    }
    return nullptr;
  }
};

TEST_F(IllegalBindTest, TargetScopeIsAProgram) {
  EXPECT_NE(findEvent(EventType::CompIllegalBind, "prg", 40, 6), nullptr);
}

TEST_F(IllegalBindTest, InstanceNameClashesWithVariable) {
  EXPECT_NE(findEvent(EventType::CompIllegalBind, "u_clash", 41, 21), nullptr);
  EXPECT_NE(findEvent(EventType::CompIllegalBind, "sig", 42, 21), nullptr);
}

TEST_F(IllegalBindTest, BindIntoModule) {
  const hldb::BindDirective *const b = findBind("u_ok");
  ASSERT_NE(b, nullptr);
  const hldb::RefObj *const scope = b->getBindTargetScope<hldb::RefObj>();
  ASSERT_NE(scope, nullptr);
  ASSERT_NE(scope->getActual(), nullptr);
  EXPECT_EQ(scope->getActual()->getAnyType(), hldb::AnyType::Module);
  EXPECT_EQ(hldb::getDefName(scope->getActual()), std::string_view("target"));
}

TEST_F(IllegalBindTest, BindIntoInterface) {
  const hldb::BindDirective *const b = findBind("u_itf");
  ASSERT_NE(b, nullptr);
  const hldb::RefObj *const scope = b->getBindTargetScope<hldb::RefObj>();
  ASSERT_NE(scope, nullptr);
  ASSERT_NE(scope->getActual(), nullptr);
  EXPECT_EQ(scope->getActual()->getAnyType(), hldb::AnyType::Interface);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
