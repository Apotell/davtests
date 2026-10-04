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

// Tests for HierPathEval/dut.sv (tags: HierPathEval)
//   package prim_pad_wrapper_pkg;
//      typedef enum logic { BidirStd = 1'h1 } pad_type_e;
//   endpackage : prim_pad_wrapper_pkg
//   package pinmux_pkg;
//      import prim_pad_wrapper_pkg::*;
//      typedef struct packed { logic a; pad_type_e dio_pad_type; } target_cfg_t;
//   endpackage : pinmux_pkg
//   module GOOD(); endmodule
//   module prim_generic_pad_attr();
//      import prim_pad_wrapper_pkg::*;
//      parameter pad_type_e PadTypeInGeneric = 0;
//      if (PadTypeInGeneric == 1) begin: gen_inner_if_true GOOD good(); end
//      else begin: gen_inner_if_false end
//   endmodule : prim_generic_pad_attr
//   module prim_pad_attr();
//      import prim_pad_wrapper_pkg::*;
//      parameter pad_type_e PadType = 0;
//      if (1) begin : gen_outer_if
//         prim_generic_pad_attr #(.PadTypeInGeneric(PadType)) u_impl_generic();
//      end
//   endmodule
//   module top();
//      import pinmux_pkg::*;
//      parameter target_cfg_t TargetCfg = 2;
//      prim_pad_attr #(.PadType(TargetCfg.dio_pad_type)) u_prim_pad_attr();
//   endmodule
//
// Checked (per IEEE 1800-2023 Sec 6.20.2 -- Constant expressions, Sec 23.10
// -- Overriding module parameters, Sec 23.6 -- Hierarchical names):
//   - "TargetCfg.dio_pad_type" (a member-select of the packed-struct-typed
//     parameter "TargetCfg") is a hierarchical-path-shaped constant
//     expression used to override "PadType" of the "prim_pad_attr" instance.
//     Because a `parameter` override actual must be an elaboration-time
//     constant expression, resolving it requires HLC to both walk the hier
//     path AND fold it to a constant value at elaboration
//   - it is modeled as any other hierarchical path: a RefObj with
//     getPathElems() == ["TargetCfg", "dio_pad_type"], getActual() resolving
//     to the TypespecMember "dio_pad_type"
//   - the chained override "PadTypeInGeneric(PadType)" is a plain
//     (non-hierarchical, single-segment) RefObj to the Parameter "PadType"
//   - the compiler reports no errors resolving/evaluating either override

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

namespace hlc {

class HierPathEvalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathEval.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ParamAssign *findParamAssign(const hldb::Module *scope, std::string_view lhsName) {
    if ((scope == nullptr) || (scope->getParamAssigns() == nullptr)) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::Any *const lhs = pa->getLhs();
      if ((lhs != nullptr) && (lhs->getName() == lhsName)) return pa;
    }
    return nullptr;
  }
};

TEST_F(HierPathEvalTest, ModulesExist) {
  EXPECT_NE(getTop(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Module>("prim_pad_attr", m_design->getAllModules()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Module>("prim_generic_pad_attr", m_design->getAllModules()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Module>("GOOD", m_design->getAllModules()), nullptr);
}

TEST_F(HierPathEvalTest, PadAttrInstanceParamOverrideIsHierPathToDioPadType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getModules(), nullptr);
  const hldb::Module *const padAttrInst = hldb::findByName<hldb::Module>("u_prim_pad_attr", top->getModules());
  ASSERT_NE(padAttrInst, nullptr) << "instance 'u_prim_pad_attr' not found under 'top'";

  const hldb::ParamAssign *const pa = findParamAssign(padAttrInst, "PadType");
  ASSERT_NE(pa, nullptr) << "param override 'PadType' not found on instance 'u_prim_pad_attr'";
  ASSERT_NE(pa->getRhs(), nullptr);

  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'PadType' override rhs is not a RefObj hierarchical path";
  EXPECT_EQ(rhs->getName(), std::string_view{"TargetCfg.dio_pad_type"});

  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  EXPECT_EQ(rhs->getPathElems()->at(0)->getName(), std::string_view{"TargetCfg"});
  EXPECT_EQ(rhs->getPathElems()->at(1)->getName(), std::string_view{"dio_pad_type"});

  ASSERT_NE(rhs->getActual(), nullptr);
  const hldb::TypespecMember *const member = rhs->getActual<hldb::TypespecMember>();
  ASSERT_NE(member, nullptr) << "'TargetCfg.dio_pad_type' should resolve to the struct member 'dio_pad_type'";
  EXPECT_EQ(member->getName(), std::string_view{"dio_pad_type"});
}

TEST_F(HierPathEvalTest, GenericPadAttrInstanceParamOverrideIsPlainRefToPadType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getModules(), nullptr);
  const hldb::Module *const padAttrInst = hldb::findByName<hldb::Module>("u_prim_pad_attr", top->getModules());
  ASSERT_NE(padAttrInst, nullptr);
  ASSERT_NE(padAttrInst->getModules(), nullptr);
  const hldb::Module *const genericInst = hldb::findByName<hldb::Module>("u_impl_generic", padAttrInst->getModules());
  ASSERT_NE(genericInst, nullptr) << "instance 'u_impl_generic' not found under 'u_prim_pad_attr'";

  const hldb::ParamAssign *const pa = findParamAssign(genericInst, "PadTypeInGeneric");
  ASSERT_NE(pa, nullptr) << "param override 'PadTypeInGeneric' not found on instance 'u_impl_generic'";
  ASSERT_NE(pa->getRhs(), nullptr);

  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'PadTypeInGeneric' override rhs is not a RefObj";
  EXPECT_EQ(rhs->getName(), std::string_view{"PadType"});
  // A single, non-dotted reference to a sibling parameter is not itself a
  // hierarchical path: no path elements to walk.
  EXPECT_EQ(rhs->getPathElems(), nullptr);

  ASSERT_NE(rhs->getActual(), nullptr);
  const hldb::Parameter *const param = rhs->getActual<hldb::Parameter>();
  ASSERT_NE(param, nullptr) << "'PadType' should resolve to the Parameter 'PadType'";
  EXPECT_EQ(param->getName(), std::string_view{"PadType"});
}

TEST_F(HierPathEvalTest, CompilerReportsZeroErrors) {
  // A nonzero error count here would mean either the hier-path member-select
  // itself failed to bind, or HLC could not fold it to a constant for the
  // parameter override -- both are exactly what this test exists to guard.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
