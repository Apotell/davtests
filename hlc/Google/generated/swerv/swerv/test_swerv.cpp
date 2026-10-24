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

// Source under test: tests/Google/generated/swerv/swerv.sv
//
//   /*
//   :name: swerv
//   :description: Full swerv core test
//   :files: .../common_defines.vh .../swerv_wrapper.sv ... (46 paths)
//   :incdirs: .../design/include .../design/lib ... (4 paths)
//   :tags: swerv
//   :top_module: swerv_wrapper
//   */
//
// The whole file is a single block comment (lines 1-8). The ":files:",
// ":incdirs:" and ":top_module:" lines are sv-tests runner metadata inside
// that comment, not SystemVerilog. swerv.hlc compiles swerv.sv alone (no
// -f file list, no +incdir+, no -top), so none of the 46 listed core files
// are part of this compilation.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today):
//   - Sec 5.4: "/* ... */" is a block comment and is ignored by the
//     language, so the file is an empty compilation unit (Sec 3.12.1).
//     A source_text with no descriptions is legal (A.1.2), so zero
//     compiler diagnostics are expected.
//   - Because nothing is declared, the design holds no modules, top
//     modules, interfaces, programs, packages, classes or UDPs. In
//     particular "swerv_wrapper" named by ":top_module:" does not exist:
//     the metadata is comment text, not a declaration.
//   - The file itself is still a source file of the design.
//
// Not checked:
//   - The block comment itself. Sec 5.4 says comments are ignored, and
//     the object model has no standard comment object, so there is no
//     LRM-backed property of the comment to assert.
//   - Anything about the SweRV core RTL listed in ":files:". Those files
//     are not compiled by swerv.hlc, so there is nothing to validate here.
//   - vpiFullName on any object.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/source_file.h>

namespace hlc {

class SwervTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "swerv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::SourceFile *getSwervSourceFile() {
    if (m_design->getSourceFiles() == nullptr) {
      return nullptr;
    }
    for (const hldb::SourceFile *const sf : *m_design->getSourceFiles()) {
      if ((sf != nullptr) && (sf->getName() == "swerv.sv")) {
        return sf;
      }
    }
    return nullptr;
  }

  // A collection HLC never allocated holds zero elements.
  template <typename T>
  static size_t countOf(const std::vector<T *> *collection) {
    if (collection == nullptr) {
      return 0u;
    }
    return collection->size();
  }
};

// --- the file is a legal, empty compilation unit ----

TEST_F(SwervTest, CompilerReportsZeroErrors) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << "Sec A.1.2: a source_text with only a comment is legal";
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

TEST_F(SwervTest, SwervSourceFileIsPartOfDesign) {
  EXPECT_NE(getSwervSourceFile(), nullptr) << "swerv.sv should be listed among the design's source files";
}

// --- nothing is declared ----

TEST_F(SwervTest, DesignHasNoModules) {
  EXPECT_EQ(countOf(m_design->getAllModules()), 0u) << "the file declares no module";
  EXPECT_EQ(countOf(m_design->getTopModules()), 0u) << "with no modules there is no top module";
}

TEST_F(SwervTest, TopModuleMetadataIsNotADeclaration) {
  EXPECT_EQ(hldb::findByName<hldb::Module>("swerv_wrapper", m_design->getAllModules()), nullptr)
      << "':top_module: swerv_wrapper' is comment text, not a module declaration";
}

TEST_F(SwervTest, DesignHasNoOtherDesignElements) {
  EXPECT_EQ(countOf(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countOf(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countOf(m_design->getAllPackages()), 0u) << "-nobuiltin: no builtin package is expected";
  EXPECT_EQ(countOf(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countOf(m_design->getAllUdps()), 0u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
