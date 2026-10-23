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

// Named event lifetime (tests/NamedEventLifetime/dut.sv):
//
//   IEEE 1800-2023 Sec 37.27 detail 4 / Sec 37.3.7  vpiAutomatic is false for a static object
//   and true for a non-static one. event_declaration takes no lifetime keyword, so a named
//   event takes the default lifetime of its scope (Sec 6.21): static in a module or a static
//   task, automatic in an automatic task/function or a class method; a class property is
//   automatic unless it is declared 'static' (Sec 8.9).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/named_event.h>
#include <hldb/task_func.h>

#include <gtest/gtest.h>

namespace hlc {

class NamedEventLifetimeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NamedEventLifetime.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Module *top() const { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  const hldb::ClassDefn *cls() const { return hldb::findByName<hldb::ClassDefn>("C", m_design->getAllClasses()); }

  static const hldb::NamedEvent *eventIn(const hldb::Scope *scope, std::string_view name) {
    if ((scope == nullptr) || (scope->getNamedEvents() == nullptr)) return nullptr;
    return hldb::findByName<hldb::NamedEvent>(name, scope->getNamedEvents());
  }

  const hldb::NamedEvent *moduleTfEvent(std::string_view tfName, std::string_view name) const {
    const hldb::Module *const m = top();
    if ((m == nullptr) || (m->getTaskFuncs() == nullptr)) return nullptr;
    return eventIn(hldb::findByName<hldb::TaskFunc>(tfName, m->getTaskFuncs()), name);
  }
};

TEST_F(NamedEventLifetimeTest, ModuleEventIsStatic) {
  const hldb::NamedEvent *const e = eventIn(top(), "m_ev");
  ASSERT_NE(e, nullptr);
  EXPECT_FALSE(e->getAutomatic());
}

TEST_F(NamedEventLifetimeTest, StaticTaskEventIsStatic) {
  const hldb::NamedEvent *const e = moduleTfEvent("st", "st_ev");
  ASSERT_NE(e, nullptr);
  EXPECT_FALSE(e->getAutomatic());
}

TEST_F(NamedEventLifetimeTest, AutomaticTaskFunctionEventsAreAutomatic) {
  const hldb::NamedEvent *const t = moduleTfEvent("at", "at_ev");
  ASSERT_NE(t, nullptr);
  EXPECT_TRUE(t->getAutomatic());
  const hldb::NamedEvent *const f = moduleTfEvent("af", "af_ev");
  ASSERT_NE(f, nullptr);
  EXPECT_TRUE(f->getAutomatic());
}

TEST_F(NamedEventLifetimeTest, ClassPropertyEvents) {
  const hldb::NamedEvent *const e = eventIn(cls(), "c_ev");
  ASSERT_NE(e, nullptr);
  EXPECT_TRUE(e->getAutomatic());
  const hldb::NamedEvent *const s = eventIn(cls(), "c_sev");
  ASSERT_NE(s, nullptr);
  EXPECT_FALSE(s->getAutomatic());
}

TEST_F(NamedEventLifetimeTest, ClassMethodEventIsAutomatic) {
  const hldb::ClassDefn *const c = cls();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  const hldb::NamedEvent *const e = eventIn(hldb::findByName<hldb::TaskFunc>("t", c->getMethods()), "ct_ev");
  ASSERT_NE(e, nullptr);
  EXPECT_TRUE(e->getAutomatic());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
