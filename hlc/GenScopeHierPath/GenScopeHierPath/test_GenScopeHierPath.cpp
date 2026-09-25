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

// Tests for GenScopeHierPath.hlc (tests/GenScopeHierPath/dut.sv):
//
//   package prim_pad_wrapper_pkg;
//      typedef enum  logic [2:0] { A = 3'h0, B = 3'h1 } pad_type_e;
//   endpackage : prim_pad_wrapper_pkg
//
//   package pinmux_pkg;
//      import prim_pad_wrapper_pkg::*;
//      parameter int NDioPads = 4;
//      typedef struct packed {
//         pad_type_e [NDioPads-1:0] dio_pad_type;
//      } target_cfg_t;
//      parameter target_cfg_t DefaultTargetCfg = '{ dio_pad_type: {NDioPads{B}} };
//   endpackage : pinmux_pkg
//
//   module prim_generic_pad_attr(output int a);
//      import prim_pad_wrapper_pkg::*;
//      parameter pad_type_e PadType = A;
//      if (PadType == B) begin : gen_assign
//         assign a = 1;
//      end
//   endmodule : prim_generic_pad_attr
//
//   module prim_pad_attr(output int b);
//      import prim_pad_wrapper_pkg::*;
//      parameter pad_type_e PadType = A;
//      if (1) begin : gen_generic
//         prim_generic_pad_attr #( .PadType(PadType) ) u_impl_generic(.a(b));
//      end
//   endmodule
//
//   module top(output int o);
//      import pinmux_pkg::*;
//      parameter target_cfg_t TargetCfg = DefaultTargetCfg;
//     for (genvar k = 0; k < NDioPads; k++) begin : gen_dio_attr
//        prim_pad_attr #( .PadType(TargetCfg.dio_pad_type[k]) ) u_prim_pad_attr(.b(o));
//     end
//   endmodule
//
// Compiled at "-d ast" level (no "-d inst"), so the generate-for loop
// survives as a raw GenFor on 'top's getGenStmts() (IEEE 1800-2023 Sec
// 27.4) rather than being unrolled into elaborated, per-iteration
// GenScopeArray/GenScope objects.
//
// What is under test: a hierarchical/indexed path reference into a *plain*
// generate scope (as opposed to a module instantiated inside one, covered
// separately by GenModHierPath). 'TargetCfg.dio_pad_type[k]' selects a
// packed-struct member and then bit-selects it with 'k' -- the genvar
// implicitly declared and scoped to the enclosing 'for (genvar k = ...)
// begin : gen_dio_attr ... end' generate-for construct (IEEE 1800-2023 Sec
// 27.4 "Generate-loop constructs": "The loop variable shall be implicitly
// declared ... its scope is local to the generate block"). The 'k' used
// inside the parameter override expression must resolve back to that
// generate-scope-local genvar, not to some unrelated identifier.
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\davtests\davtests_02\build\include\hldb.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/ref_obj.h>
#include <hldb/select.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenScopeHierPathTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenScopeHierPath.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::GenFor *findGenFor(const hldb::Module *m) {
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenFor *const gf = any_cast<hldb::GenFor>(stmt)) return gf;
    }
    return nullptr;
  }

  // Walk a Select/RefObj expression chain looking for a leaf RefObj named
  // 'k' (the loop's own genvar reference used as the bit-select index).
  static const hldb::RefObj *findKRef(const hldb::Any *expr) {
    if (expr == nullptr) return nullptr;
    if (const hldb::RefObj *const ro = any_cast<hldb::RefObj>(expr)) {
      if (ro->getName() == std::string_view("k")) return ro;
      return nullptr;
    }
    if (const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(expr)) {
      if (const hldb::RefObj *const found = findKRef(bs->getIndex())) return found;
      return findKRef(bs->getPrefix());
    }
    if (const hldb::Select *const sel = any_cast<hldb::Select>(expr)) {
      return findKRef(sel->getPrefix());
    }
    return nullptr;
  }
};

TEST_F(GenScopeHierPathTest, AllThreeModulesExist) {
  ASSERT_NE(getModule("prim_generic_pad_attr"), nullptr);
  ASSERT_NE(getModule("prim_pad_attr"), nullptr);
  ASSERT_NE(getModule("top"), nullptr);
}

// 'for (genvar k = 0; k < NDioPads; k++) begin : gen_dio_attr ... end' --
// exactly one generate-for on 'top', named 'gen_dio_attr' (Sec 27.4).
TEST_F(GenScopeHierPathTest, TopHasExactlyOneGenForNamedGenDioAttr) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr);
  size_t count = 0u;
  for (const hldb::Any *const stmt : *top->getGenStmts()) {
    if (any_cast<hldb::GenFor>(stmt) != nullptr) ++count;
  }
  EXPECT_EQ(count, 1u);

  const hldb::GenFor *const gf = findGenFor(getModule("top"));
  ASSERT_NE(gf, nullptr);
  EXPECT_EQ(gf->getName(), std::string_view("gen_dio_attr"));
}

// 'k < NDioPads' -- relational less-than condition (Sec 11.4.4, vpiLtOp).
TEST_F(GenScopeHierPathTest, GenForConditionIsKLessThanNDioPads) {
  const hldb::GenFor *const gf = findGenFor(getModule("top"));
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getCondition(), nullptr);
  const hldb::Operation *const cond = gf->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'k < NDioPads' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
}

// 'prim_pad_attr #(...) u_prim_pad_attr(.b(o));' -- one module instance
// declared inside the generate-for scope (GenScope::getModules()).
TEST_F(GenScopeHierPathTest, UPrimPadAttrDeclaredInsideGenDioAttr) {
  const hldb::GenFor *const gf = findGenFor(getModule("top"));
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getModules(), nullptr) << "'gen_dio_attr' should carry the 'u_prim_pad_attr' instance";
  const hldb::Module *const inst = hldb::findByName<hldb::Module>("u_prim_pad_attr", gf->getModules());
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getDefName(), std::string_view("prim_pad_attr"));
}

// '.PadType(TargetCfg.dio_pad_type[k])' -- the parameter override
// expression must reference 'k', the genvar implicitly declared and scoped
// to 'gen_dio_attr' itself (Sec 27.4): a hierarchical path back into the
// enclosing plain generate scope, not into a module instance.
TEST_F(GenScopeHierPathTest, PadTypeOverrideReferencesLoopScopedK) {
  const hldb::GenFor *const gf = findGenFor(getModule("top"));
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getModules(), nullptr);
  const hldb::Module *const inst = hldb::findByName<hldb::Module>("u_prim_pad_attr", gf->getModules());
  ASSERT_NE(inst, nullptr);

  ASSERT_NE(inst->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName("PadType", inst->getParamAssigns());
  ASSERT_NE(pa, nullptr) << "'.PadType(TargetCfg.dio_pad_type[k])' ParamAssign not found";
  ASSERT_NE(pa->getRhs(), nullptr);

  const hldb::RefObj *const kRef = findKRef(pa->getRhs());
  ASSERT_NE(kRef, nullptr) << "'TargetCfg.dio_pad_type[k]' should reference 'k' somewhere in its expression tree";
  EXPECT_EQ(kRef->getName(), std::string_view("k"));

  // 'k' must resolve to a declared item (the loop's own genvar), not be
  // left unbound.
  EXPECT_NE(kRef->getActual(), nullptr) << "'k' should resolve to the generate-for's own genvar declaration";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
