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

// Tests for tests/FileLocalParam/dut.sv:
//
//   localparam AWIDTH = 16;
//   localparam [AWIDTH:0] MAP = { AWIDTH };
//
//   module GOOD();
//   endmodule
//
//   module top();
//     parameter D = MAP;
//     if (D == 17'b00000000000000000000000000010000) begin
//        GOOD good();
//     end
//   endmodule
//
// 'AWIDTH' and 'MAP' are declared outside any module, at the compilation
// unit ("$unit") scope (IEEE 1800-2023 Sec 3.12.2 "Compilation-unit
// scope"). A 'localparam' declared there is a file/compilation-unit scoped
// constant: it is visible without qualification everywhere in the same
// compilation unit (here, to 'module top'), but per Sec 6.20.4 it is never
// overridable at instantiation, matching the general 'localparam' rule.
// UHDM represents compilation-unit scope items directly under Design
// (Design::getParameters()), not under any Module.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/unit.h>

#include <string>
#include <string_view>

namespace hlc {

class FileLocalParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FileLocalParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Parameter *findUnitParam(std::string_view name) {
    return hldb::findByName<hldb::Parameter>(name, m_unit->getParameters());
  }
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Module *getGood() {
    return hldb::findByDefName<hldb::Module>("GOOD", m_design->getAllModules());
  }
};

TEST_F(FileLocalParamTest, ModuleGoodExists) { EXPECT_NE(getGood(), nullptr) << "module 'GOOD' not found"; }

TEST_F(FileLocalParamTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr) << "module 'top' not found"; }

// 'localparam AWIDTH = 16;' at $unit scope.
TEST_F(FileLocalParamTest, AwidthIsUnitScopedLocalParam) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::Parameter *const p = findUnitParam("AWIDTH");
  ASSERT_NE(p, nullptr) << "'AWIDTH' not found at compilation-unit scope";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: 'localparam AWIDTH' must not be overridable";
  ASSERT_NE(p->getExpr(), nullptr);
  const hldb::Constant *const rhs = p->getExpr<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'AWIDTH = 16': RHS must be a Constant";
  EXPECT_EQ(rhs->getDecompile(), std::string_view("16"));
}

// 'localparam [AWIDTH:0] MAP = { AWIDTH };' at $unit scope -- also
// non-overridable per 6.20.4, referencing 'AWIDTH' in its own packed range.
TEST_F(FileLocalParamTest, MapIsUnitScopedLocalParam) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::Parameter *const p = findUnitParam("MAP");
  ASSERT_NE(p, nullptr) << "'MAP' not found at compilation-unit scope";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: 'localparam MAP' must not be overridable";
  ASSERT_NE(p->getExpr(), nullptr) << "'MAP = { AWIDTH }': RHS must be non-null";
}

// 'parameter D = MAP;' inside 'top' is an overridable module parameter
// (Sec 6.20.2) whose default references the file-scoped localparam 'MAP'
// declared outside the module.
TEST_F(FileLocalParamTest, TopParameterDReferencesUnitScopedMap) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  const hldb::Parameter *d = nullptr;
  for (const hldb::Any *const p : *top->getParameters()) {
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
    if (param != nullptr && param->getName() == std::string_view{"D"}) {
      d = param;
      break;
    }
  }
  ASSERT_NE(d, nullptr) << "parameter 'D' not found in module 'top'";
  EXPECT_FALSE(d->getLocalParam()) << "6.20.2: 'parameter D' must be overridable, unlike a localparam";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
