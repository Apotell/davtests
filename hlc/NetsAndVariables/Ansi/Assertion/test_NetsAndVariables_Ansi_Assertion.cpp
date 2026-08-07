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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Assertion.sv.
//
// Checked:
//   - nets_and_variables_assertion exists with three input ports
//     (clk, a, b)
//   - it has exactly one property declaration (p_sampled_value)
//   - that property declaration has a local variable (local_val)
//   - it has exactly one concurrent assertion (the 'assert property' stmt)
//
// Not checked: the internal PropertySpec/ClockedProperty expression tree of
// the property body (how the local variable's sampled assignment and the
// '|=>' implication are represented). No test anywhere in this codebase
// exercises a named property declaration with a local variable, so that
// deeper shape is left undocumented rather than guessed.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/property_decl.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiAssertionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Assertion.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("nets_and_variables_assertion", m_design->getAllModules());
  }
};

TEST_F(AnsiAssertionTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(AnsiAssertionTest, ModuleHasThreeInputPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 3u);

  const char *const names[3] = {"clk", "a", "b"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(names[i], top->getPorts());
    ASSERT_NE(port, nullptr) << "port " << names[i];
    EXPECT_EQ(port->getDirection(), vpiInput);
  }
}

TEST_F(AnsiAssertionTest, ModuleHasOnePropertyDecl) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPropertyDecls(), nullptr);
  EXPECT_EQ(top->getPropertyDecls()->size(), 1u);
}

TEST_F(AnsiAssertionTest, PropertyDeclHasLocalVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPropertyDecls(), nullptr);
  ASSERT_EQ(top->getPropertyDecls()->size(), 1u);
  const hldb::PropertyDecl *const prop = top->getPropertyDecls()->at(0);
  ASSERT_NE(prop, nullptr);
  ASSERT_NE(prop->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("local_val", prop->getVariables()), nullptr);
}

TEST_F(AnsiAssertionTest, ModuleHasOneConcurrentAssertion) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(top->getConcurrentAssertions()->size(), 1u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
