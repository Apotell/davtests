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
//   - var_logic / var_reg have no net-type keyword -- per IEEE 1800 Annex
//     A.2.1.3 they go through data_declaration, never net_declaration, so
//     each must be a hldb::Variable resolving to a LogicTypespec, and must
//     NOT also appear as a hldb::Net (no duplicate)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class NonAnsiVariablesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Variables.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("nets_and_variables_nonansi", m_design->getAllModules());
  }

  // Asserts 'name' is a hldb::Variable in mod->getVariables() and that no
  // hldb::Net with the same name exists in mod->getNets() (no duplicate).
  static const hldb::Variable *getVarNoNetDuplicate(const hldb::Module *mod, std::string_view name) {
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, mod->getVariables());
    EXPECT_NE(v, nullptr) << "'" << name << "' should appear in vpiVariables";
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, mod->getNets()), nullptr)
        << "'" << name << "' has no net-type keyword -- it must not also appear in vpiNet";
    return v;
  }
};

TEST_F(NonAnsiVariablesTest, VarLogicIsLogicTypespec) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const v = getVarNoNetDuplicate(mod, "var_logic");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(NonAnsiVariablesTest, VarRegIsLogicTypespec) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const v = getVarNoNetDuplicate(mod, "var_reg");
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
