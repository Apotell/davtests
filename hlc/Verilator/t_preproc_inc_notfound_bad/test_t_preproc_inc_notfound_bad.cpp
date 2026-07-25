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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/source_file.h>

namespace hlc {
class PreprocIncNotfoundBadTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc_inc_notfound_bad.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// A file that only `includes a non-existent file has no compilable module.
TEST_F(PreprocIncNotfoundBadTest, NoModules) {
  ASSERT_TRUE((m_design->getAllModules() == nullptr) || (m_design->getAllModules()->size() == 0u))
      << "t_preproc_inc_notfound_bad contains no module declarations";
}

// LRM 22.4: even when the included file cannot be found, the compiler must
// record the attempted include path in the source file's include list.
TEST_F(PreprocIncNotfoundBadTest, MissingFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_inc_notfound_bad.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr) << "the include directive must be recorded even though the file was not found";
  const hldb::SourceFile *const inc =
      hldb::findByName<hldb::SourceFile>("this_file_is_not_found.vh", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "this_file_is_not_found.vh must appear in the include list";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
