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

// Tests for tests/HierPathOverride/dut.sv:
//
//   module top();
//      import pinmux_pkg::*;
//      parameter target_cfg_t TargetCfg = DefaultTargetCfg;
//       prim_pad_attr #(
//          .PadType(TargetCfg.dio_pad_type[0])
//       ) u_prim_pad_attr();
//   endmodule
//
// The parameter override ".PadType(TargetCfg.dio_pad_type[0])" combines a
// hierarchical path (TargetCfg.dio_pad_type -- a member select into a
// packed-struct parameter) with a bit-select ([0]) of a packed-array field,
// used as the override expression of a parameter override
// (IEEE 1800-2023 Sec 23.10, "Overriding module parameters").
//
// Checked:
//   - module "top" exists and has a non-local parameter "TargetCfg"
//   - "top" instantiates a child module named "u_prim_pad_attr"
//     (def name "prim_pad_attr")
//   - the override expression shape: a hierarchical-path RefObj with 2 path
//     elements -- RefObj "TargetCfg" (the parameter) and BitSelect
//     "dio_pad_type[0]" (prefix RefObj "dio_pad_type" -> struct member, index 0).
//     The design is unelaborated, so the instance is a RefInstance and the
//     override is a ParamAssign on its ModuleTypespec.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec_member.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace hlc {

class HierPathOverrideTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathOverride.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  // This design is not elaborated (no -d inst): 'u_prim_pad_attr' is a RefInstance in top's
  // getRefInstances(), and its parameter overrides are the ParamAssigns of the ModuleTypespec its
  // typespec resolves to.
  static const hldb::RefInstance *getPrimPadAttrInstance() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_prim_pad_attr", top->getRefInstances());
  }

  static const hldb::ModuleTypespec *getPrimPadAttrTypespec() {
    const hldb::RefInstance *const inst = getPrimPadAttrInstance();
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    return inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  }
};

TEST_F(HierPathOverrideTest, TopModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(HierPathOverrideTest, TopHasTargetCfgParameter) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  const hldb::Parameter *targetCfg = nullptr;
  for (const hldb::Any *const p : *top->getParameters()) {
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
    if (param != nullptr && param->getName() == std::string_view{"TargetCfg"}) {
      targetCfg = param;
      break;
    }
  }
  ASSERT_NE(targetCfg, nullptr) << "parameter 'TargetCfg' not found on module 'top'";
  EXPECT_FALSE(targetCfg->getLocalParam());
}

TEST_F(HierPathOverrideTest, PrimPadAttrInstanceExists) {
  const hldb::RefInstance *const inst = getPrimPadAttrInstance();
  ASSERT_NE(inst, nullptr) << "instance 'u_prim_pad_attr' not found under module 'top'";
  // EXPECT_EQ(inst->getDefName(), std::string_view{"prim_pad_attr"});
  const hldb::ModuleTypespec *const mt = getPrimPadAttrTypespec();
  ASSERT_NE(mt, nullptr) << "'u_prim_pad_attr' should be an instance of a module";
  EXPECT_EQ(mt->getDefName(), std::string_view{"prim_pad_attr"});
}

TEST_F(HierPathOverrideTest, PadTypeOverrideIsHierPathSelectExpression) {
  const hldb::ModuleTypespec *const mt = getPrimPadAttrTypespec();
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const padType = hldb::findByName("PadType", mt->getParamAssigns());
  ASSERT_NE(padType, nullptr) << "override '.PadType(...)' not found on 'u_prim_pad_attr'";
  EXPECT_TRUE(padType->getOverridden());
  EXPECT_TRUE(padType->getConnByName());

  // 'TargetCfg.dio_pad_type[0]' is a hierarchical path whose last element is the bit-select:
  // path elements "TargetCfg" (the parameter) and "dio_pad_type[0]" (a BitSelect of the
  // struct member). This is HLDB's hierarchical-path convention (also HierPathLhs,
  // HierPathSelect, HierPathPackedStruct); IEEE 1800 VPI defines no hierarchical-path object.
  ASSERT_NE(padType->getRhs(), nullptr);
  const hldb::RefObj *const path = padType->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "override RHS should be a hierarchical path RefObj";
  EXPECT_EQ(path->getName(), std::string_view{"TargetCfg.dio_pad_type[0]"});
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const head = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->getName(), std::string_view{"TargetCfg"});
  ASSERT_NE(head->getActual(), nullptr);
  EXPECT_NE(head->getActual<hldb::Parameter>(), nullptr) << "'TargetCfg' should resolve to top's parameter";

  const hldb::BitSelect *const bsel = any_cast<hldb::BitSelect>(path->getPathElems()->at(1));
  ASSERT_NE(bsel, nullptr) << "'dio_pad_type[0]' should be a BitSelect";
  ASSERT_NE(bsel->getPrefix(), nullptr);
  const hldb::RefObj *const member = bsel->getPrefix<hldb::RefObj>();
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), std::string_view{"dio_pad_type"});
  ASSERT_NE(member->getActual(), nullptr);
  EXPECT_NE(member->getActual<hldb::TypespecMember>(), nullptr)
      << "'dio_pad_type' should resolve to the target_cfg_t struct member";
  ASSERT_NE(bsel->getIndex(), nullptr);
  const hldb::Constant *const idx = bsel->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr) << "'[0]' index should be a Constant";
  EXPECT_EQ(idx->getDecompile(), std::string_view{"0"});

  // Previous elaborated-model / BitSelect-of-path version:
  //   const hldb::Module *const inst = getPrimPadAttrInstance();
  //   ASSERT_NE(inst, nullptr);
  //   ASSERT_NE(inst->getParamAssigns(), nullptr);
  //   const hldb::ParamAssign *padType = nullptr;
  //   for (const hldb::ParamAssign *const pa : *inst->getParamAssigns()) {
  //     const hldb::Any *const lhs = pa->getLhs();
  //     if (lhs != nullptr && lhs->getName() == std::string_view{"PadType"}) {
  //       padType = pa;
  //       break;
  //     }
  //   }
  //   if (padType == nullptr || !padType->getOverridden()) {
  //     // Overridden ParamAssigns not being carried onto the elaborated child
  //     // scope is a known, previously-flagged limitation (see
  //     // hlc/ArianeElab/ArianeElab/test_ArianeElab.cpp T*: "Overriden
  //     // ParamAssigns are not being retained yet. Part of static
  //     // elaboration."). Skip the shape assertion rather than lock in the gap.
  //     GTEST_SKIP() << "ParamAssign for overridden 'PadType' not retained on the elaborated "
  //                      "'u_prim_pad_attr' scope; per IEEE 1800-2023 Sec 23.10 it should carry the "
  //                      "override expression 'TargetCfg.dio_pad_type[0]'. Fix pending.";
  //   }
  //
  //   ASSERT_NE(padType->getRhs(), nullptr);
  //   const hldb::BitSelect *const bsel = padType->getRhs<hldb::BitSelect>();
  //   ASSERT_NE(bsel, nullptr) << "'TargetCfg.dio_pad_type[0]': override RHS should be a BitSelect";
  //
  //   ASSERT_NE(bsel->getPrefix(), nullptr);
  //   const hldb::RefObj *const prefix = bsel->getPrefix<hldb::RefObj>();
  //   ASSERT_NE(prefix, nullptr) << "BitSelect prefix should be the hierarchical path RefObj";
  //   EXPECT_EQ(prefix->getName(), std::string_view{"TargetCfg.dio_pad_type"});
  //   ASSERT_NE(prefix->getPathElems(), nullptr);
  //   ASSERT_EQ(prefix->getPathElems()->size(), 2u);
  //   EXPECT_EQ(prefix->getPathElems()->at(0)->getName(), std::string_view{"TargetCfg"});
  //   EXPECT_EQ(prefix->getPathElems()->at(1)->getName(), std::string_view{"dio_pad_type"});
  //
  //   ASSERT_NE(bsel->getIndex(), nullptr);
  //   const hldb::Constant *const idx = bsel->getIndex<hldb::Constant>();
  //   ASSERT_NE(idx, nullptr) << "'[0]' index should be a Constant";
  //   EXPECT_EQ(std::string(idx->getValue()), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
