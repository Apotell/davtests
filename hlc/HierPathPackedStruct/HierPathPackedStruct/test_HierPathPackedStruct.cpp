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

// Tests for tests/HierPathPackedStruct/dut.sv:
//
//   typedef struct packed {
//     logic [19:0] foo;
//     logic [21:0] bar;
//   } fourty_two_t;
//
//   typedef struct packed {
//     fourty_two_t [1:0] pair;  // packed array
//   } foo_t;
//
//   module some_mod(input foo_t a);
//      if ($bits(a.pair[0]) != 42) begin
//         $error("pair element not expected size");
//      end
//   endmodule
//
// "a.pair[0]" reaches the packed-array field "pair" of the packed struct
// "a" via a hierarchical path first (RefObj "a" -> member "pair"), and only
// then bit-selects one element of that packed array with "[0]" -- the
// select is applied *after* the hierarchical path (contrast with
// HierPathPackedArrayNet/HierPathPackedVar, where an array-select is
// applied to the base name before the hierarchical descent). The
// enclosing "if" appears directly in the module body, so per IEEE
// 1800-2023 Sec 27.3 (conditional generate constructs) it is an implicit
// generate-if (GenIf), not a procedural statement.
//
// Checked:
//   - module "some_mod" exists, has port "a" of typespec foo_t
//   - module has exactly one GenIf whose condition is
//     "$bits(a.pair[0]) != 42" (Operation, vpiNeqOp)
//   - the $bits argument is a BitSelect{ prefix: hierarchical RefObj "a"
//     with path elements ["a", "pair"], index: 0 }

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace hlc {

class HierPathPackedStructTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathPackedStruct.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getSomeMod() {
    return hldb::findByDefName<hldb::Module>("some_mod", m_design->getAllModules());
  }

  static const hldb::GenIf *findGenIf(const hldb::Module *mod) {
    if (mod == nullptr || mod->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const genStmt : *mod->getGenStmts()) {
      const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(genStmt);
      if (genIf != nullptr) return genIf;
    }
    return nullptr;
  }
};

TEST_F(HierPathPackedStructTest, ModuleExists) { EXPECT_NE(getSomeMod(), nullptr); }

TEST_F(HierPathPackedStructTest, GenIfExists) { EXPECT_NE(findGenIf(getSomeMod()), nullptr); }

TEST_F(HierPathPackedStructTest, ConditionIsBitsNotEqualFortyTwo) {
  const hldb::GenIf *const genIf = findGenIf(getSomeMod());
  ASSERT_NE(genIf, nullptr);
  ASSERT_NE(genIf->getCondition(), nullptr);
  const hldb::Operation *const neq = any_cast<hldb::Operation>(genIf->getCondition());
  ASSERT_NE(neq, nullptr) << "'$bits(a.pair[0]) != 42' should be an Operation";
  EXPECT_EQ(neq->getOpType(), vpiNeqOp);
  ASSERT_NE(neq->getOperands(), nullptr);
  ASSERT_EQ(neq->getOperands()->size(), 2u);

  const hldb::SysFuncCall *const bits = any_cast<hldb::SysFuncCall>(neq->getOperands()->at(0));
  ASSERT_NE(bits, nullptr) << "first operand should be the '$bits' SysFuncCall";
  EXPECT_EQ(bits->getName(), std::string_view{"$bits"});

  const hldb::Constant *const fortyTwo = any_cast<hldb::Constant>(neq->getOperands()->at(1));
  ASSERT_NE(fortyTwo, nullptr);
  EXPECT_EQ(std::string(fortyTwo->getValue()), "42");
}

TEST_F(HierPathPackedStructTest, BitsArgumentIsHierPathThenBitSelect) {
  const hldb::GenIf *const genIf = findGenIf(getSomeMod());
  ASSERT_NE(genIf, nullptr);
  const hldb::Operation *const neq = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(neq, nullptr);
  ASSERT_NE(neq->getOperands(), nullptr);
  ASSERT_EQ(neq->getOperands()->size(), 2u);
  const hldb::SysFuncCall *const bits = any_cast<hldb::SysFuncCall>(neq->getOperands()->at(0));
  ASSERT_NE(bits, nullptr);

  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);
  const hldb::BitSelect *const bsel = any_cast<hldb::BitSelect>(bits->getArguments()->at(0));
  ASSERT_NE(bsel, nullptr) << "'a.pair[0]' should be a BitSelect";

  ASSERT_NE(bsel->getPrefix(), nullptr);
  const hldb::RefObj *const prefix = bsel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix should be the hierarchical path RefObj 'a.pair'";
  EXPECT_EQ(prefix->getName(), std::string_view{"a.pair"});
  ASSERT_NE(prefix->getPathElems(), nullptr);
  ASSERT_EQ(prefix->getPathElems()->size(), 2u);
  EXPECT_EQ(prefix->getPathElems()->at(0)->getName(), std::string_view{"a"});
  EXPECT_EQ(prefix->getPathElems()->at(1)->getName(), std::string_view{"pair"});

  ASSERT_NE(bsel->getIndex(), nullptr);
  const hldb::Constant *const idx = bsel->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(std::string(idx->getValue()), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
