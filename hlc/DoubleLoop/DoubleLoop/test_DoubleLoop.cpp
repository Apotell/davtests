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

// Tests for dut.sv (tags: DoubleLoop)
//   module constpower1(ys, yu);
//     output [2:0] ys, yu;
//     genvar i, j;
//     generate
//       for (i = 0; i < 2; i = i+1)
//         for (j = 0; j < 2; j = j+1) begin:W
//           assign ys = i + j;
//         end
//     endgenerate
//   endmodule
//   (constpower2..constpower5 are the same nested loop_generate_construct,
//    varying which of the two for-loops has an explicit begin-end wrapping
//    its body and whether that block is named)
//
// What to check and why (IEEE 1800-2023 27.4 "Loop generate constructs",
// checked before any test code was written):
//   "loop_generate_construct ::= for ( genvar_initialization ; genvar_
//   expression ; genvar_iteration ) generate_block" and "genvar_
//   initialization ::= [ genvar ] genvar_identifier = constant_expression".
//   Unlike a procedural for-loop (12.7.1), the genvar is NOT declared inline
//   in the loop header here -- dut.sv declares "genvar i, j;" as a separate
//   genvar_declaration at module scope, so both become ordinary Variables
//   (27.5: "genvar ... shall be ... used ... as if it were a ... integer
//   variable") parented directly to the enclosing module, not to the
//   for-loop's own scope.
//
//   "generate_block ::= generate_item | [ generate_block_identifier : ]
//   begin [ : generate_block_identifier ] { generate_item } end
//   [ : generate_block_identifier ]" -- so when the loop body is a single
//   generate_item (here, a single continuous_assign), the begin-end is
//   optional and the item attaches directly as the for-loop's body; when
//   begin-end is used, it introduces a nested scope (Begin) holding the
//   generate_item(s).
//
//   Also (IEEE 1800-2023 6.8): "output [2:0] ys, yu" have no net_type
//   keyword and no explicit data type, so per 23.2.2.3 they default to
//   implicit nets (vpiWire), not Variables -- not the focus of this file,
//   so not asserted on beyond being out of scope.
//
// What is checked (per module, structural shape mirrors 12.7.1--for.sv's
// ForStmt shape: getForInitStmts()/getCondition()/getForIncStmts()/getStmt()
// -- GenFor for the generate-for construct):
//   - all 5 modules exist and each has exactly the genvars "i" and "j" as
//     Variables at module scope (27.5), never inside the GenFor's own scope
//   - constpower1: outer GenFor (i=0; i<2; i=i+1) wraps its body in a
//     named Begin "W"; that Begin's single item is the inner GenFor
//     (j=0; j<2; j=j+1), whose body -- no begin-end -- is directly the
//     ContAssign "ys = i + j"
//   - constpower2: outer GenFor has no begin (body is directly the inner
//     GenFor); inner GenFor's body is an (unnamed) Begin holding the
//     ContAssign
//   - constpower3: outer GenFor's body is an (unnamed) Begin holding the
//     inner GenFor; inner GenFor has no begin (body directly ContAssign)
//   - constpower4: both outer and inner GenFor wrap their bodies in
//     (unnamed) Begin blocks
//   - constpower5: neither GenFor uses begin-end; the ContAssign is the
//     direct body of the inner GenFor, which is the direct body of the
//     outer GenFor
//   - for each module, the outer/inner GenFor's own scope (getVariables())
//     is empty/null -- confirming i and j live at module scope, not in an
//     implicit per-loop scope (contrast with 12.7.1--for.sv's ForStmt,
//     where an inline "int i = 0" does create such an implicit scope)
//   - the innermost ContAssign is "ys = i + j": LHS RefObj "ys",
//     RHS Operation(vpiAddOp) over RefObj "i" and RefObj "j"
//
// What is NOT checked and why:
//   - the elaborated/unrolled instances of the generate-for (2x2 = 4
//     iterations producing 4 distinct ContAssigns) are elaboration-phase
//     concepts; this file only checks the single generate_block template
//     built at parse/db time (-d db -d ast, no full elaboration pass)
//   - implicit-net modeling of "ys"/"yu" ports is out of scope here
//   - the exact default name (genblk1, ...) assigned to the unnamed Begin
//     blocks (constpower2/3/4/5) per 27.6 is not asserted -- only that an
//     (unnamed) Begin scope exists -- to avoid over-specifying an
//     implementation-dependent numbering detail not central to this test

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_region.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DoubleLoopTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DoubleLoop.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // Outer GenFor: module->getGenStmts() -> GenRegion -> getStmt<GenFor>()
  static const hldb::GenFor *getOuterGenFor(std::string_view moduleName) {
    const hldb::Module *const m = getModule(moduleName);
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(stmt);
      if (region == nullptr) continue;
      const hldb::GenFor *const outer = region->getStmt<hldb::GenFor>();
      if (outer != nullptr) return outer;
    }
    return nullptr;
  }

  // Verifies the genvar_initialization/genvar_expression/genvar_iteration
  // shape common to both the outer (var, bound) and inner (var, bound)
  // GenFor loops: "<var> = 0; <var> < <bound>; <var> = <var>+1".
  static void CheckLoopHeader(const hldb::GenFor *loop, std::string_view var) {
    ASSERT_NE(loop, nullptr);

    ASSERT_NE(loop->getForInitStmts(), nullptr);
    ASSERT_EQ(loop->getForInitStmts()->size(), 1u);
    const hldb::Assignment *const init = any_cast<hldb::Assignment>(loop->getForInitStmts()->at(0));
    ASSERT_NE(init, nullptr) << "genvar_initialization should be an Assignment";
    const hldb::RefObj *const initLhs = init->getLhs<hldb::RefObj>();
    ASSERT_NE(initLhs, nullptr) << "'" << var << "' is predeclared via 'genvar', so init LHS should be a RefObj";
    EXPECT_EQ(initLhs->getName(), var);
    EXPECT_NE(initLhs->getActual<hldb::Variable>(), nullptr);
    const hldb::Constant *const initRhs = init->getRhs<hldb::Constant>();
    ASSERT_NE(initRhs, nullptr);
    EXPECT_EQ(initRhs->getDecompile(), "0");

    const hldb::Operation *const cond = loop->getCondition<hldb::Operation>();
    ASSERT_NE(cond, nullptr) << "genvar_expression is not an Operation";
    EXPECT_EQ(cond->getOpType(), vpiLtOp);
    ASSERT_NE(cond->getOperands(), nullptr);
    ASSERT_EQ(cond->getOperands()->size(), 2u);
    const hldb::RefObj *const condLhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
    ASSERT_NE(condLhs, nullptr);
    EXPECT_EQ(condLhs->getName(), var);
    const hldb::Constant *const condRhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
    ASSERT_NE(condRhs, nullptr);
    EXPECT_EQ(condRhs->getDecompile(), "2");

    ASSERT_NE(loop->getForIncStmts(), nullptr);
    ASSERT_EQ(loop->getForIncStmts()->size(), 1u);
    const hldb::Assignment *const inc = any_cast<hldb::Assignment>(loop->getForIncStmts()->at(0));
    ASSERT_NE(inc, nullptr) << "genvar_iteration '" << var << " = " << var << "+1' should be an Assignment";
    EXPECT_TRUE(inc->getBlocking());
    const hldb::RefObj *const incLhs = inc->getLhs<hldb::RefObj>();
    ASSERT_NE(incLhs, nullptr);
    EXPECT_EQ(incLhs->getName(), var);
    const hldb::Operation *const incRhs = inc->getRhs<hldb::Operation>();
    ASSERT_NE(incRhs, nullptr);
    EXPECT_EQ(incRhs->getOpType(), vpiAddOp);
    ASSERT_NE(incRhs->getOperands(), nullptr);
    ASSERT_EQ(incRhs->getOperands()->size(), 2u);
    const hldb::RefObj *const incRhsVar = any_cast<hldb::RefObj>(incRhs->getOperands()->at(0));
    ASSERT_NE(incRhsVar, nullptr);
    EXPECT_EQ(incRhsVar->getName(), var);
    const hldb::Constant *const incRhsOne = any_cast<hldb::Constant>(incRhs->getOperands()->at(1));
    ASSERT_NE(incRhsOne, nullptr);
    EXPECT_EQ(incRhsOne->getDecompile(), "1");

    // 27.5/27.4: the genvar lives at module scope (predeclared), not in an
    // implicit scope owned by this particular for-loop.
    EXPECT_TRUE(loop->getVariables() == nullptr || loop->getVariables()->empty())
        << "no inline 'genvar' declaration in the loop header -- the GenFor should not own '" << var << "'";
  }

  // Verifies the innermost body: "assign ys = i + j;"
  static void CheckAssignYsIPlusJ(const hldb::ContAssign *ca) {
    ASSERT_NE(ca, nullptr);
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "ys");
    const hldb::Operation *const rhs = ca->getRhs<hldb::Operation>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getOpType(), vpiAddOp);
    ASSERT_NE(rhs->getOperands(), nullptr);
    ASSERT_EQ(rhs->getOperands()->size(), 2u);
    const hldb::RefObj *const opI = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
    ASSERT_NE(opI, nullptr);
    EXPECT_EQ(opI->getName(), "i");
    const hldb::RefObj *const opJ = any_cast<hldb::RefObj>(rhs->getOperands()->at(1));
    ASSERT_NE(opJ, nullptr);
    EXPECT_EQ(opJ->getName(), "j");
  }
};

// ---------------------------------------------------------------------------
// Module existence and genvar declarations (27.5: genvar -> Variable,
// declared at module scope for every one of the 5 variants)
// ---------------------------------------------------------------------------
TEST_F(DoubleLoopTest, AllFiveModulesExist) {
  EXPECT_NE(getModule("constpower1"), nullptr);
  EXPECT_NE(getModule("constpower2"), nullptr);
  EXPECT_NE(getModule("constpower3"), nullptr);
  EXPECT_NE(getModule("constpower4"), nullptr);
  EXPECT_NE(getModule("constpower5"), nullptr);
}

TEST_F(DoubleLoopTest, EachModuleHasGenvarsIAndJAsVariables) {
  for (std::string_view name : {"constpower1", "constpower2", "constpower3", "constpower4", "constpower5"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << name;
    ASSERT_NE(m->getVariables(), nullptr) << "'genvar i, j;' should produce 2 Variables in " << name;
    EXPECT_NE(hldb::findByName<hldb::Variable>("i", m->getVariables()), nullptr) << name << ": genvar 'i' not found";
    EXPECT_NE(hldb::findByName<hldb::Variable>("j", m->getVariables()), nullptr) << name << ": genvar 'j' not found";
  }
}

// ---------------------------------------------------------------------------
// constpower1: outer for has no begin around the inner for; inner for's
// body is "begin:W ... end" (named)
// ---------------------------------------------------------------------------
TEST_F(DoubleLoopTest, ConstPower1_OuterLoopHeader) {
  CheckLoopHeader(getOuterGenFor("constpower1"), "i");
}

TEST_F(DoubleLoopTest, ConstPower1_OuterBodyIsDirectlyInnerGenFor) {
  const hldb::GenFor *const outer = getOuterGenFor("constpower1");
  ASSERT_NE(outer, nullptr);
  const hldb::GenFor *const inner = outer->getStmt<hldb::GenFor>();
  ASSERT_NE(inner, nullptr) << "constpower1's outer for has no begin-end, so its body is directly the inner GenFor";
  CheckLoopHeader(inner, "j");
}

TEST_F(DoubleLoopTest, ConstPower1_InnerBodyIsNamedBeginW) {
  const hldb::GenFor *const outer = getOuterGenFor("constpower1");
  ASSERT_NE(outer, nullptr);
  const hldb::GenFor *const inner = outer->getStmt<hldb::GenFor>();
  ASSERT_NE(inner, nullptr);
  const hldb::Begin *const w = inner->getStmt<hldb::Begin>();
  ASSERT_NE(w, nullptr) << "inner for's body should be the named 'begin:W ... end' block";
  EXPECT_EQ(w->getName(), "W");
  ASSERT_NE(w->getStmts(), nullptr);
  ASSERT_EQ(w->getStmts()->size(), 1u);
  CheckAssignYsIPlusJ(any_cast<hldb::ContAssign>(w->getStmts()->at(0)));
}

// ---------------------------------------------------------------------------
// constpower2: outer for has no begin; inner for's body is an unnamed
// "begin ... end"
// ---------------------------------------------------------------------------
TEST_F(DoubleLoopTest, ConstPower2_NestingShape) {
  const hldb::GenFor *const outer = getOuterGenFor("constpower2");
  CheckLoopHeader(outer, "i");
  ASSERT_NE(outer, nullptr);
  const hldb::GenFor *const inner = outer->getStmt<hldb::GenFor>();
  ASSERT_NE(inner, nullptr) << "constpower2's outer for has no begin-end";
  CheckLoopHeader(inner, "j");
  const hldb::Begin *const body = inner->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "constpower2's inner for body is an unnamed begin-end";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  CheckAssignYsIPlusJ(any_cast<hldb::ContAssign>(body->getStmts()->at(0)));
}

// ---------------------------------------------------------------------------
// constpower3: outer for's body is an unnamed "begin ... end" wrapping the
// inner for; inner for has no begin
// ---------------------------------------------------------------------------
TEST_F(DoubleLoopTest, ConstPower3_NestingShape) {
  const hldb::GenFor *const outer = getOuterGenFor("constpower3");
  CheckLoopHeader(outer, "i");
  ASSERT_NE(outer, nullptr);
  const hldb::Begin *const body = outer->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "constpower3's outer for body is an unnamed begin-end";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  const hldb::GenFor *const inner = any_cast<hldb::GenFor>(body->getStmts()->at(0));
  ASSERT_NE(inner, nullptr) << "the begin-end's single item should be the inner GenFor";
  CheckLoopHeader(inner, "j");
  CheckAssignYsIPlusJ(inner->getStmt<hldb::ContAssign>());
}

// ---------------------------------------------------------------------------
// constpower4: both outer and inner for wrap their bodies in unnamed
// "begin ... end" blocks
// ---------------------------------------------------------------------------
TEST_F(DoubleLoopTest, ConstPower4_NestingShape) {
  const hldb::GenFor *const outer = getOuterGenFor("constpower4");
  CheckLoopHeader(outer, "i");
  ASSERT_NE(outer, nullptr);
  const hldb::Begin *const outerBody = outer->getStmt<hldb::Begin>();
  ASSERT_NE(outerBody, nullptr) << "constpower4's outer for body is an unnamed begin-end";
  ASSERT_NE(outerBody->getStmts(), nullptr);
  ASSERT_EQ(outerBody->getStmts()->size(), 1u);
  const hldb::GenFor *const inner = any_cast<hldb::GenFor>(outerBody->getStmts()->at(0));
  ASSERT_NE(inner, nullptr);
  CheckLoopHeader(inner, "j");
  const hldb::Begin *const innerBody = inner->getStmt<hldb::Begin>();
  ASSERT_NE(innerBody, nullptr) << "constpower4's inner for body is also an unnamed begin-end";
  ASSERT_NE(innerBody->getStmts(), nullptr);
  ASSERT_EQ(innerBody->getStmts()->size(), 1u);
  CheckAssignYsIPlusJ(any_cast<hldb::ContAssign>(innerBody->getStmts()->at(0)));
}

// ---------------------------------------------------------------------------
// constpower5: neither for uses begin-end; the ContAssign is directly the
// body of the inner for, itself directly the body of the outer for
// ---------------------------------------------------------------------------
TEST_F(DoubleLoopTest, ConstPower5_NestingShape) {
  const hldb::GenFor *const outer = getOuterGenFor("constpower5");
  CheckLoopHeader(outer, "i");
  ASSERT_NE(outer, nullptr);
  const hldb::GenFor *const inner = outer->getStmt<hldb::GenFor>();
  ASSERT_NE(inner, nullptr) << "constpower5's outer for has no begin-end";
  CheckLoopHeader(inner, "j");
  CheckAssignYsIPlusJ(inner->getStmt<hldb::ContAssign>());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
