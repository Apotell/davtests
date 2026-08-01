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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Class.sv,
// split out of the combined NetsAndVariablesAnsi.sv suite so the file-scope
// class testing point stands on its own.
//
// Checked:
//   - work@nets_and_variables_class exists via Design::getAllClasses()
//   - its variables (cls_logic, cls_reg, cls_bits) exist
//   - its rand/randc variables (cls_rand, cls_randc) exist with the matching
//     vpiRand/vpiRandC qualifier

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>

namespace hlc {

class AnsiClassTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AnsiClass.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(AnsiClassTest, ClassExists) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::ClassDefn>("work@nets_and_variables_class", m_design->getAllClasses()), nullptr);
}

TEST_F(AnsiClassTest, ClassHasVariables) {
  const hldb::ClassDefn *const cls =
      hldb::findByName<hldb::ClassDefn>("work@nets_and_variables_class", m_design->getAllClasses());
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls_logic", cls->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls_reg", cls->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls_bits", cls->getVariables()), nullptr);
}

TEST_F(AnsiClassTest, ClassHasRandVariables) {
  const hldb::ClassDefn *const cls =
      hldb::findByName<hldb::ClassDefn>("work@nets_and_variables_class", m_design->getAllClasses());
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);

  const hldb::Variable *const clsRand = hldb::findByName<hldb::Variable>("cls_rand", cls->getVariables());
  ASSERT_NE(clsRand, nullptr);
  EXPECT_EQ(clsRand->getRandType(), vpiRand);

  const hldb::Variable *const clsRandc = hldb::findByName<hldb::Variable>("cls_randc", cls->getVariables());
  ASSERT_NE(clsRandc, nullptr);
  EXPECT_EQ(clsRandc->getRandType(), vpiRandC);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
