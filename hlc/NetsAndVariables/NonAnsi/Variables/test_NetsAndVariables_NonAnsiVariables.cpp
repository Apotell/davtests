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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Variables.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so the
// variable-keyword testing point stands on its own.
//
// Checked:
//   - var_logic / var_reg resolve to a LogicTypespec

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class NonAnsiVariablesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Variables.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("nets_and_variables_nonansi", m_design->getAllModules());
  }
};

TEST_F(NonAnsiVariablesTest, VarLogicIsLogicTypespec) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_logic", mod->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(NonAnsiVariablesTest, VarRegIsLogicTypespec) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("var_reg", mod->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr) << "reg is an alias of logic";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
