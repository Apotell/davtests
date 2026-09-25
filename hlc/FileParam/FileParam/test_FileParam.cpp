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

// Tests for tests/FileParam/dut.sv:
//
//   localparam P1 = 1;
//   localparam P2 = P1 + 2;
//
//   typedef enum int unsigned {
//       SCR1_SCU_DR_SYSCTRL_OP_BIT_R = 'h0,
//       SCR1_SCU_DR_SYSCTRL_OP_BIT_L = P2+4
//   } type_scr1_scu_sysctrl_dr_bits_e;
//
//   module top();
//   endmodule
//
// 'P1' and 'P2' are declared outside any module, at compilation-unit
// ("$unit") scope (IEEE 1800-2023 Sec 3.12.2). Per Sec 6.20.4 both are
// 'localparam's -- non-overridable, file/compilation-unit-scoped constants
// -- and 'P2's initializer references the earlier constant 'P1' (Sec
// 11.2.1 "Constant expressions"). The enum member
// 'SCR1_SCU_DR_SYSCTRL_OP_BIT_L' (Sec 6.19 "Enumerations") further
// references 'P2' in its own value expression, all resolved purely from
// compilation-unit scope, without qualification, from inside 'module top'.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

#include <string>
#include <string_view>

namespace hlc {

class FileParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FileParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Parameter *findUnitParam(std::string_view name) {
    if (m_design->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *m_design->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }
  static const hldb::ParamAssign *findUnitParamAssign(std::string_view name) {
    return hldb::findByName(name, m_design->getParamAssigns());
  }
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::EnumTypespec *getBitsEnum() {
    if (m_design->getTypedefs() == nullptr) return nullptr;
    const hldb::Typedef *const td =
        hldb::findByName<hldb::Typedef>("type_scr1_scu_sysctrl_dr_bits_e", m_design->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    return td->getAlias()->getActual<hldb::EnumTypespec>();
  }
};

TEST_F(FileParamTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr) << "module 'top' not found"; }

// 'localparam P1 = 1;'
TEST_F(FileParamTest, P1IsUnitScopedLocalParamWithValueOne) {
  const hldb::Parameter *const p1 = findUnitParam("P1");
  ASSERT_NE(p1, nullptr) << "'P1' not found at compilation-unit scope";
  EXPECT_TRUE(p1->getLocalParam()) << "6.20.4: 'localparam P1' must not be overridable";

  const hldb::ParamAssign *const pa = findUnitParamAssign("P1");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(pa->getRhs(), nullptr);
  ASSERT_NE(rhs, nullptr) << "'P1 = 1': RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "1");
}

// 'localparam P2 = P1 + 2;' -- references the earlier compilation-unit
// scoped constant 'P1'.
TEST_F(FileParamTest, P2IsUnitScopedLocalParamReferencingP1) {
  const hldb::Parameter *const p2 = findUnitParam("P2");
  ASSERT_NE(p2, nullptr) << "'P2' not found at compilation-unit scope";
  EXPECT_TRUE(p2->getLocalParam());

  const hldb::ParamAssign *const pa = findUnitParamAssign("P2");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs(), nullptr);
  const hldb::Operation *const addOp = pa->getRhs<hldb::Operation>();
  ASSERT_NE(addOp, nullptr) << "'P1 + 2': RHS must be an Operation(vpiAddOp)";
  EXPECT_EQ(addOp->getOpType(), vpiAddOp);
  ASSERT_NE(addOp->getOperands(), nullptr);
  ASSERT_EQ(addOp->getOperands()->size(), 2u);
  const hldb::RefObj *const p1Ref = any_cast<hldb::RefObj>((*addOp->getOperands())[0]);
  ASSERT_NE(p1Ref, nullptr) << "'P1' operand must be a RefObj";
  EXPECT_EQ(p1Ref->getName(), std::string_view{"P1"});
}

// 6.19: the enum has exactly two members, in declaration order.
TEST_F(FileParamTest, BitsEnumHasTwoMembersInOrder) {
  const hldb::EnumTypespec *const enumTs = getBitsEnum();
  ASSERT_NE(enumTs, nullptr) << "'type_scr1_scu_sysctrl_dr_bits_e' should resolve to an EnumTypespec";
  const hldb::Enum *const e = enumTs->getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view{"SCR1_SCU_DR_SYSCTRL_OP_BIT_R"});
  EXPECT_EQ(e->getEnumConsts()->at(1)->getName(), std::string_view{"SCR1_SCU_DR_SYSCTRL_OP_BIT_L"});
}

// 'SCR1_SCU_DR_SYSCTRL_OP_BIT_L = P2+4' -- references the compilation-unit
// scoped 'P2', proving cross-scope constant resolution reaches into an enum
// value expression too.
TEST_F(FileParamTest, SecondEnumMemberReferencesP2) {
  const hldb::EnumTypespec *const enumTs = getBitsEnum();
  ASSERT_NE(enumTs, nullptr);
  const hldb::Enum *const e = enumTs->getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  const hldb::EnumConst *const bitL = e->getEnumConsts()->at(1);
  ASSERT_NE(bitL->getValue(), nullptr);
  const hldb::Operation *const addOp = bitL->getValue<hldb::Operation>();
  ASSERT_NE(addOp, nullptr) << "'P2+4': value must be an Operation(vpiAddOp)";
  EXPECT_EQ(addOp->getOpType(), vpiAddOp);
  ASSERT_NE(addOp->getOperands(), nullptr);
  ASSERT_EQ(addOp->getOperands()->size(), 2u);
  const hldb::RefObj *const p2Ref = any_cast<hldb::RefObj>((*addOp->getOperands())[0]);
  ASSERT_NE(p2Ref, nullptr) << "'P2' operand must be a RefObj";
  EXPECT_EQ(p2Ref->getName(), std::string_view{"P2"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
