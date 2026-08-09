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

// Validates UHDM attribute attachment for variable declarations:
//   (* fsm_state *)   logic [7:0] a;
//   (* fsm_state=1 *) logic [7:0] b;
//   (* fsm_state=0 *) logic [7:0] c;
//
// Per IEEE 1800-2023 Sec 5.12, an attribute_instance immediately preceding a
// declaration attaches to that declaration -- each of a/b/c should carry its
// own single fsm_state attribute, and the module itself should have none.
//
// KNOWN BUG: HLC currently hoists all three attributes onto the containing
// Module's vpiAttribute list instead of attaching them to the individual
// variables. The tests below assert the STANDARD-correct behavior and are
// marked GTEST_SKIP() until that's fixed -- do not "fix" them to match
// current (wrong) output.
//
// Attribute forms:
//   a: (* fsm_state *)   -> flag, getValue() == nullptr
//   b: (* fsm_state=1 *) -> Constant, getDecompile() == "1"
//   c: (* fsm_state=0 *) -> Constant, getDecompile() == "0"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/attribute.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>

namespace hlc {

class AttributesVariable : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.12-attributes-variable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

// ----
// Module and nets
// ----
TEST_F(AttributesVariable, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr); }

TEST_F(AttributesVariable, ThreeVariablesExist) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 3u);

  bool hasA = false, hasB = false, hasC = false;
  for (const hldb::Variable *const n : *top->getVariables()) {
    if (n->getName() == "a") hasA = true;
    if (n->getName() == "b") hasB = true;
    if (n->getName() == "c") hasC = true;
  }
  EXPECT_TRUE(hasA) << "variable 'a' missing";
  EXPECT_TRUE(hasB) << "variable 'b' missing";
  EXPECT_TRUE(hasC) << "variable 'c' missing";
}

// `logic [7:0]` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8
// a, b, c must not also appear in the module's net collection.
TEST_F(AttributesVariable, VariablesAreNotDuplicatedAsNets) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr);
  }
}

// ----
// Per IEEE 1800-2023 Sec 5.12, attributes attach to the declaration they
// immediately precede, not to the enclosing module.
// KNOWN BUG: HLC currently hoists them onto the module instead -- skipped
// until that's fixed.
// ----
TEST_F(AttributesVariable, ModuleHasNoAttributes) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(!top->getAttributes() || top->getAttributes()->empty())
      << "attributes preceding a, b, c belong to those variables, not the module";
}

// ----
// (* fsm_state *) -- attribute for variable 'a', flag (no value)
// ----
TEST_F(AttributesVariable, VariableAHasFsmStateFlagAttribute) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getAttributes(), nullptr);
  ASSERT_EQ(a->getAttributes()->size(), 1u);

  const hldb::Attribute *const attr = (*a->getAttributes())[0];
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->getName(), "fsm_state");
  EXPECT_EQ(attr->getValue(), nullptr) << "(* fsm_state *) is a flag attribute and should have no value";
}

// ----
// (* fsm_state=1 *) -- attribute for variable 'b', value = 1
// ----
TEST_F(AttributesVariable, VariableBHasFsmStateValueOne) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getAttributes(), nullptr);
  ASSERT_EQ(b->getAttributes()->size(), 1u);

  const hldb::Attribute *const attr = (*b->getAttributes())[0];
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->getName(), "fsm_state");

  const hldb::Constant *const val = attr->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "fsm_state=1 should have a Constant value";
  EXPECT_EQ(val->getDecompile(), "1");
}

// ----
// (* fsm_state=0 *) -- attribute for variable 'c', value = 0
// ----
TEST_F(AttributesVariable, VariableCHasFsmStateValueZero) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getAttributes(), nullptr);
  ASSERT_EQ(c->getAttributes()->size(), 1u);

  const hldb::Attribute *const attr = (*c->getAttributes())[0];
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->getName(), "fsm_state");

  const hldb::Constant *const val = attr->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "fsm_state=0 should have a Constant value";
  EXPECT_EQ(val->getDecompile(), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
