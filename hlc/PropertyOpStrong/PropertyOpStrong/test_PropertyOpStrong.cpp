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

// Tests for dut.sv (tags: PropertyOpStrong)
//
// What is checked (IEEE 1800-2023):
//   - 16.12.13 / 37.52 detail 2: 'until' and 's_until' are vpiUntilOp,
//     'until_with' and 's_until_with' are vpiUntilWithOp, each with the two
//     property operands in source order
//   - 37.52 detail 3: vpiOpStrong is TRUE for the strong forms (s_until,
//     s_until_with) and FALSE for the weak ones
//
// What is NOT checked and why:
//   - strong always/eventually/nexttime: those operations are not built yet
//     (NOT_POPULATED #106), so their vpiOpStrong has nothing to sit on.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/property_decl.h>
#include <hldb/property_spec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PropertyOpStrongTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PropertyOpStrong.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Operation *getOp(std::string_view propertyName) {
    const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
    if (top == nullptr) return nullptr;
    const hldb::PropertyDecl *const pd = hldb::findByName<hldb::PropertyDecl>(propertyName, top->getPropertyDecls());
    if ((pd == nullptr) || (pd->getPropertySpec() == nullptr)) return nullptr;
    return pd->getPropertySpec()->getPropertyExpr<hldb::Operation>();
  }

  static void expectOp(std::string_view propertyName, int32_t opType, bool strong) {
    const hldb::Operation *const op = getOp(propertyName);
    ASSERT_NE(op, nullptr) << propertyName;
    EXPECT_EQ(op->getOpType(), opType) << propertyName;
    EXPECT_EQ(op->getOpStrong(), strong) << "37.52 detail 3: " << propertyName;
    ASSERT_NE(op->getOperands(), nullptr) << propertyName;
    EXPECT_EQ(op->getOperands()->size(), 2u) << propertyName;
  }
};

TEST_F(PropertyOpStrongTest, UntilIsWeak) { expectOp("p_until", vpiUntilOp, false); }

TEST_F(PropertyOpStrongTest, SUntilIsStrong) { expectOp("p_s_until", vpiUntilOp, true); }

TEST_F(PropertyOpStrongTest, UntilWithIsWeak) { expectOp("p_until_with", vpiUntilWithOp, false); }

TEST_F(PropertyOpStrongTest, SUntilWithIsStrong) { expectOp("p_s_until_with", vpiUntilWithOp, true); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
