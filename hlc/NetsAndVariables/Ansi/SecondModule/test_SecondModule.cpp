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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/SecondModule.sv,
// split out of the combined NetsAndVariablesAnsi.sv suite so the second
// module's interface/class instantiation testing point stands on its own.
// The interface and class it instantiates are duplicated into this file
// (see SecondModule.sv) rather than shared across files.
//
// Checked:
//   - work@nets_and_variables_second exists
//   - it instantiates the interface (ref instance "if0")
//   - it declares a class-handle variable ("cls0")

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/ref_instance.h>
#include <hldb/variable.h>

namespace hlc {

class AnsiSecondModuleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SecondModule.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getSecond() {
    return hldb::findByName<hldb::Module>("work@nets_and_variables_second", m_design->getAllModules());
  }
};

TEST_F(AnsiSecondModuleTest, SecondModuleExists) { ASSERT_NE(getSecond(), nullptr); }

TEST_F(AnsiSecondModuleTest, SecondModuleInstantiatesInterface) {
  const hldb::Module *const second = getSecond();
  ASSERT_NE(second, nullptr);
  ASSERT_NE(second->getRefInstances(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::RefInstance>("if0", second->getRefInstances()), nullptr)
      << "interface instance 'if0' not found";
}

TEST_F(AnsiSecondModuleTest, SecondModuleHasCls0Variable) {
  const hldb::Module *const second = getSecond();
  ASSERT_NE(second, nullptr);
  ASSERT_NE(second->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cls0", second->getVariables()), nullptr)
      << "class-handle variable 'cls0' not found";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
