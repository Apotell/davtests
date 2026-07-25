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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Program.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so the program
// testing point stands on its own.
//
// Checked:
//   - work@nets_and_variables_program exists via m_design->getAllPrograms();
//     no test anywhere in this suite exercises Program ports/variables/body,
//     so nothing beyond existence is asserted

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/program.h>

namespace hlc {

class NonAnsiProgramTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Program.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(NonAnsiProgramTest, ProgramExists) {
  ASSERT_NE(m_design->getAllPrograms(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Program>("work@nets_and_variables_program", m_design->getAllPrograms()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
