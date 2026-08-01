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

// Validates module-level attributes in all three syntactic forms:
//   (* optimize_power *)     module topa -- flag (no value)
//   (* optimize_power=0 *)   module topb -- valued, 0
//   (* optimize_power=1 *)   module topc -- valued, 1
//
// Module attributes are accessible via Scope::getAttributes() on the Module
// node itself -- they are NOT inside any process or statement.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/attribute.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class AttributesModule : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.12-attributes-module.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// Helper: find the named Attribute on a Module, or nullptr.
static const hldb::Attribute *findAttr(const hldb::Module *mod, std::string_view name) {
  if (!mod->getAttributes()) return nullptr;
  for (const hldb::Attribute *const a : *mod->getAttributes()) {
    if (a->getName() == name) return a;
  }
  return nullptr;
}

// ----
// Three modules compiled from the same file
// ----
TEST_F(AttributesModule, ThreeModulesExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 3u) << "expected 3 modules: topa, topb, topc";
}

TEST_F(AttributesModule, TopaExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("topa", m_design->getAllModules()), nullptr);
}

TEST_F(AttributesModule, TopbExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("topb", m_design->getAllModules()), nullptr);
}

TEST_F(AttributesModule, TopcExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("topc", m_design->getAllModules()), nullptr);
}

// ----
// (* optimize_power *) -- flag attribute, no value
// ----
TEST_F(AttributesModule, TopaHasOptimizePowerAttribute) {
  const hldb::Module *const topa = hldb::findByName<hldb::Module>("topa", m_design->getAllModules());
  ASSERT_NE(topa, nullptr);
  ASSERT_NE(topa->getAttributes(), nullptr);
  ASSERT_EQ(topa->getAttributes()->size(), 1u);
  EXPECT_EQ((*topa->getAttributes())[0]->getName(), "optimize_power");
}

TEST_F(AttributesModule, TopaOptimizePowerIsFlagAttribute) {
  const hldb::Module *const topa = hldb::findByName<hldb::Module>("topa", m_design->getAllModules());
  ASSERT_NE(topa, nullptr);

  const hldb::Attribute *const attr = findAttr(topa, "optimize_power");
  ASSERT_NE(attr, nullptr) << "topa should have 'optimize_power' attribute";
  EXPECT_EQ(attr->getValue(), nullptr) << "(* optimize_power *) is a flag -- getValue() should be null";
}

// ----
// (* optimize_power=0 *) -- valued attribute, Constant "0"
// ----
TEST_F(AttributesModule, TopbHasOptimizePowerAttribute) {
  const hldb::Module *const topb = hldb::findByName<hldb::Module>("topb", m_design->getAllModules());
  ASSERT_NE(topb, nullptr);
  ASSERT_NE(topb->getAttributes(), nullptr);
  ASSERT_EQ(topb->getAttributes()->size(), 1u);
  EXPECT_EQ((*topb->getAttributes())[0]->getName(), "optimize_power");
}

TEST_F(AttributesModule, TopbOptimizePowerValueIsZero) {
  const hldb::Module *const topb = hldb::findByName<hldb::Module>("topb", m_design->getAllModules());
  ASSERT_NE(topb, nullptr);

  const hldb::Attribute *const attr = findAttr(topb, "optimize_power");
  ASSERT_NE(attr, nullptr) << "topb should have 'optimize_power' attribute";

  const hldb::Constant *const val = attr->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "optimize_power=0 should have a Constant value";
  EXPECT_EQ(val->getDecompile(), "0");
}

// ----
// (* optimize_power=1 *) -- valued attribute, Constant "1"
// ----
TEST_F(AttributesModule, TopcHasOptimizePowerAttribute) {
  const hldb::Module *const topc = hldb::findByName<hldb::Module>("topc", m_design->getAllModules());
  ASSERT_NE(topc, nullptr);
  ASSERT_NE(topc->getAttributes(), nullptr);
  ASSERT_EQ(topc->getAttributes()->size(), 1u);
  EXPECT_EQ((*topc->getAttributes())[0]->getName(), "optimize_power");
}

TEST_F(AttributesModule, TopcOptimizePowerValueIsOne) {
  const hldb::Module *const topc = hldb::findByName<hldb::Module>("topc", m_design->getAllModules());
  ASSERT_NE(topc, nullptr);

  const hldb::Attribute *const attr = findAttr(topc, "optimize_power");
  ASSERT_NE(attr, nullptr) << "topc should have 'optimize_power' attribute";

  const hldb::Constant *const val = attr->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "optimize_power=1 should have a Constant value";
  EXPECT_EQ(val->getDecompile(), "1");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
