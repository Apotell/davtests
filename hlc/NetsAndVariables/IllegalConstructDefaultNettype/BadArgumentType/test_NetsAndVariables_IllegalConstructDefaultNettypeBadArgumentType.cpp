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

// Tests for tests/NetsAndVariables/illegal_construct/Illegal_construct_default_nettype_bad_argument_type.sv
//
// Illegal construct: `default_nettype logic -- 'logic' is a data type, not
// a net type, so it is not a valid `default_nettype argument.
//
// Checked:
//   - the compiler reports at least one syntax/error diagnostic for this
//     file (the illegal construct is rejected, not silently accepted)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class IllegalConstructDefaultNettypeBadArgumentTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "BadArgumentType.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalConstructDefaultNettypeBadArgumentTypeTest, BadArgumentTypeIsRejected) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "`default_nettype logic is illegal: the directive requires a net type keyword or 'none', not a "
         "data type.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
