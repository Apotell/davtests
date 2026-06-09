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
// Key finding from UHDM dump:
//   Variable/net attributes are NOT attached to the Net node itself.
//   They are hoisted to the containing Module's vpiAttribute list.
//   The three nets (a, b, c) each have no attributes — all three
//   fsm_state attributes appear on Module::getAttributes() in
//   declaration order (index 0 → a, 1 → b, 2 → c).
//
// Attribute forms:
//   index 0: (* fsm_state *)   → flag, getValue() == nullptr
//   index 1: (* fsm_state=1 *) → Constant, getDecompile() == "1"
//   index 2: (* fsm_state=0 *) → Constant, getDecompile() == "0"

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/attribute.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>

namespace SURELOG {

class AttributesVariable : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.12-attributes-variable.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@top", d->getAllModules());
}

// ---------------------------------------------------------------------------
// Module and nets
// ---------------------------------------------------------------------------
TEST_F(AttributesVariable, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr);
}

TEST_F(AttributesVariable, ThreeNetsExist) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u);

  bool hasA = false, hasB = false, hasC = false;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "a") hasA = true;
    if (n->getName() == "b") hasB = true;
    if (n->getName() == "c") hasC = true;
  }
  EXPECT_TRUE(hasA) << "net 'a' missing";
  EXPECT_TRUE(hasB) << "net 'b' missing";
  EXPECT_TRUE(hasC) << "net 'c' missing";
}

// ---------------------------------------------------------------------------
// Variable attributes are hoisted to the module's attribute list, not the net.
// ---------------------------------------------------------------------------
TEST_F(AttributesVariable, NetsHaveNoDirectAttributes) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  for (const uhdm::Net *const n : *top->getNets()) {
    EXPECT_TRUE(!n->getAttributes() || n->getAttributes()->empty())
        << "net '" << n->getName() << "' should have no direct attributes";
  }
}

TEST_F(AttributesVariable, ModuleHasThreeFsmStateAttributes) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getAttributes(), nullptr);
  ASSERT_EQ(top->getAttributes()->size(), 3u);

  for (size_t i = 0; i < 3u; ++i) {
    EXPECT_EQ((*top->getAttributes())[i]->getName(), "fsm_state")
        << "attribute[" << i << "] should be named 'fsm_state'";
  }
}

// ---------------------------------------------------------------------------
// (* fsm_state *) — attribute for net 'a', flag (no value)
// ---------------------------------------------------------------------------
TEST_F(AttributesVariable, FsmStateForAIsFlagAttribute) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getAttributes(), nullptr);
  ASSERT_EQ(top->getAttributes()->size(), 3u);

  const uhdm::Attribute *const attr = (*top->getAttributes())[0];
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->getName(), "fsm_state");
  EXPECT_EQ(attr->getValue(), nullptr)
      << "(* fsm_state *) is a flag attribute and should have no value";
}

// ---------------------------------------------------------------------------
// (* fsm_state=1 *) — attribute for net 'b', value = 1
// ---------------------------------------------------------------------------
TEST_F(AttributesVariable, FsmStateForBValueIsOne) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getAttributes(), nullptr);
  ASSERT_EQ(top->getAttributes()->size(), 3u);

  const uhdm::Attribute *const attr = (*top->getAttributes())[1];
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->getName(), "fsm_state");

  const uhdm::Constant *const val = attr->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr) << "fsm_state=1 should have a Constant value";
  EXPECT_EQ(val->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// (* fsm_state=0 *) — attribute for net 'c', value = 0
// ---------------------------------------------------------------------------
TEST_F(AttributesVariable, FsmStateForCValueIsZero) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getAttributes(), nullptr);
  ASSERT_EQ(top->getAttributes()->size(), 3u);

  const uhdm::Attribute *const attr = (*top->getAttributes())[2];
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->getName(), "fsm_state");

  const uhdm::Constant *const val = attr->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr) << "fsm_state=0 should have a Constant value";
  EXPECT_EQ(val->getDecompile(), "0");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
