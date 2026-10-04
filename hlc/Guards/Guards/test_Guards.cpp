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

// Guards.hlc compiles two identical source files as one compilation:
//   rtl1/ast_pkg.sv:
//     `ifdef __AST_PKG_SV
//     `else
//     `define __AST_PKG_SV
//     package ast_pkg;
//     endpackage
//     `endif
//   rtl2/ast_pkg.sv: (byte-for-byte identical to rtl1/ast_pkg.sv)
//
// IEEE 1800-2023 Sec 22.5 ("`ifdef", "`ifndef", "`define" conditional
// compilation): text macros persist for the entire compilation (all files on
// the command line, processed in order), not just within the file that
// defines them. When rtl1/ast_pkg.sv is processed first, __AST_PKG_SV is not
// yet defined, so the `ifdef branch is false, the `else branch runs, and it
// both `defines __AST_PKG_SV and declares 'package ast_pkg'. When
// rtl2/ast_pkg.sv is processed next, __AST_PKG_SV is now defined (it
// persisted across the file boundary), so the `ifdef branch is taken (its
// body is empty) and 'package ast_pkg' is NOT declared a second time. This
// is the classic C-style include-guard idiom applied to conditional
// compilation, and it must prevent a duplicate declaration of 'ast_pkg'.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/package.h>

#include <string_view>

namespace hlc {

class GuardsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Guards.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// The `ifdef/`define/`endif guard must have suppressed the second
// 'package ast_pkg;' declaration from rtl2/ast_pkg.sv, so exactly one
// 'ast_pkg' Package object should exist in the design.
TEST_F(GuardsTest, ExactlyOneAstPkgPackageExists) {
  ASSERT_NE(m_design->getAllPackages(), nullptr) << "design has no packages at all";

  uint32_t matchCount = 0;
  for (const hldb::Package *const pkg : *m_design->getAllPackages()) {
    ASSERT_NE(pkg, nullptr);
    if (pkg->getName() == std::string_view{"ast_pkg"}) {
      ++matchCount;
    }
  }
  EXPECT_EQ(matchCount, 1u) << "expected exactly one 'ast_pkg' package (guard should prevent a duplicate "
                               "declaration from rtl2/ast_pkg.sv per IEEE 1800-2023 Sec 22.5)";
}

// findByName<Package> should resolve to a single, well-formed 'ast_pkg'.
TEST_F(GuardsTest, AstPkgIsFindableByName) {
  const hldb::Package *const pkg = hldb::findByName<hldb::Package>("ast_pkg", m_design->getAllPackages());
  ASSERT_NE(pkg, nullptr) << "package 'ast_pkg' not found";
  EXPECT_EQ(pkg->getName(), std::string_view{"ast_pkg"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
