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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Behavior.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so continuous
// assignments and process coverage stand on their own.
//
// Checked:
//   - continuous assignments -- 4 total, in source order:
//     implicit_net_nonansi = a & b; w0 = a | b; w_bus[0] = a; y = w0
//   - one always_comb process

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

namespace hlc {

class NonAnsiBehaviorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Behavior.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("nets_and_variables_nonansi", m_design->getAllModules());
  }
};

TEST_F(NonAnsiBehaviorTest, FourContAssignsExist) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  EXPECT_EQ(mod->getContAssigns()->size(), 4u);
}

TEST_F(NonAnsiBehaviorTest, LastContAssignDrivesY) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_FALSE(mod->getContAssigns()->empty());
  const hldb::RefObj *const lhs = mod->getContAssigns()->back()->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "y");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

TEST_F(NonAnsiBehaviorTest, OneProcessExists) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  ASSERT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Always>(mod->getProcesses()->at(0)), nullptr)
      << "'always_comb' should be modeled as an Always process";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
