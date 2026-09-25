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

// Tests for tests/ElabIf/dut.sv -- two independent (non-else) generate-if
// constructs whose conditions depend on a parameter propagated through an
// instance chain:
//
//   module top ();
//      middleman #(.invert(1)) mdl1();
//      middleman #(.invert(0)) mdl0();
//   endmodule
//
//   module assigner #(parameter invert = 0) ();
//     if (!invert)
//       assign out = inp;
//     if (invert)
//       assign out = ~inp;
//   endmodule
//
//   module middleman #(parameter invert = 0) ();
//      assigner #(.invert(invert)) asgn();
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 27.5 "Generate-if constructs": each 'if (cond) stmt;' with
// no 'begin/end' and no 'else' is its own GenIf; because 'assigner' has two
// separate if-statements (not an if/else), both survive as GenIf entries
// wrapping the single continuous-assignment statement directly (no Begin,
// since no explicit block was opened) -- IEEE 1800-2023 27.5 permits a
// single statement as the generate-if body without begin/end, mirroring
// 12.4 "Conditional if-else statement").
// IEEE 1800-2023 23.3/23.10: 'middleman's 'invert' parameter is propagated
// by name into 'assigner's 'invert', and top's two 'middleman' instances
// each override 'invert' with a distinct Constant (1 and 0).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <vector>

namespace hlc {

class ElabIfTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ElabIf.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  template <typename ScopeT>
  static const hldb::Parameter *findParam(const ScopeT *scope, std::string_view name) {
    if (scope == nullptr || scope->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *scope->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  template <typename ScopeT>
  static const hldb::ParamAssign *findParamAssign(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(scope));
  }

  static const hldb::RefInstance *findRefInst(std::string_view instName, const hldb::Module *parent) {
    if (parent == nullptr || parent->getRefInstances() == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(instName, parent->getRefInstances());
  }

  // Returns all GenIf statements directly in m's getGenStmts().
  static std::vector<const hldb::GenIf *> findAllGenIf(const hldb::Module *m) {
    std::vector<const hldb::GenIf *> result;
    if (m == nullptr || m->getGenStmts() == nullptr) return result;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(stmt)) result.emplace_back(gi);
    }
    return result;
  }
};

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

TEST_F(ElabIfTest, ModulesExist) {
  EXPECT_NE(getModule("top"), nullptr) << "module 'top' not found";
  EXPECT_NE(getModule("assigner"), nullptr) << "module 'assigner' not found";
  EXPECT_NE(getModule("middleman"), nullptr) << "module 'middleman' not found";
}

// ---------------------------------------------------------------------------
// assigner: 'parameter invert = 0;' plus two independent generate-ifs.
// ---------------------------------------------------------------------------

TEST_F(ElabIfTest, Assigner_InvertParamNotLocalWithDefaultZero) {
  const hldb::Module *const m = getModule("assigner");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const invert = findParam(m, "invert");
  ASSERT_NE(invert, nullptr) << "'parameter invert' not found on 'assigner'";
  EXPECT_FALSE(invert->getLocalParam()) << "'parameter invert' must not be a localparam";

  const hldb::ParamAssign *const pa = findParamAssign(m, "invert");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'invert' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'invert = 0': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "0");
}

TEST_F(ElabIfTest, Assigner_HasExactlyTwoGenIfStatements) {
  const hldb::Module *const m = getModule("assigner");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr) << "'assigner' has no generate statements";
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(m);
  ASSERT_EQ(genIfs.size(), 2u) << "'assigner' has two separate 'if' generate constructs (not if/else), "
                                  "so both must be present as distinct GenIf nodes";
}

// 'if (!invert) assign out = inp;' -- IEEE 1800-2023 11.4.1 "!" is
// vpiNotOp; the single statement body (no begin/end) is the ContAssign
// itself.
TEST_F(ElabIfTest, Assigner_FirstGenIf_ConditionIsLogicalNotOfInvert) {
  const hldb::Module *const m = getModule("assigner");
  ASSERT_NE(m, nullptr);
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(m);
  ASSERT_EQ(genIfs.size(), 2u);

  const hldb::Operation *const cond = genIfs[0]->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'!invert' condition must be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiNotOp) << "'!invert' must produce vpiNotOp";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u) << "unary '!' has exactly one operand";

  const hldb::RefObj *const operand = any_cast<hldb::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "'!invert': operand must be a RefObj (reference to invert)";
  EXPECT_EQ(operand->getName(), "invert");
}

TEST_F(ElabIfTest, Assigner_FirstGenIf_BodyIsContAssignOutFromInp) {
  const hldb::Module *const m = getModule("assigner");
  ASSERT_NE(m, nullptr);
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(m);
  ASSERT_EQ(genIfs.size(), 2u);

  const hldb::ContAssign *const assign = genIfs[0]->getStmt<hldb::ContAssign>();
  ASSERT_NE(assign, nullptr) << "'assign out = inp;' body must be a ContAssign";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'assign out = ...': LHS must be a RefObj (reference to out)";
  EXPECT_EQ(lhs->getName(), "out");

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'assign out = inp;': RHS must be a RefObj (reference to inp)";
  EXPECT_EQ(rhs->getName(), "inp");
}

// 'if (invert) assign out = ~inp;' -- the bare parameter reference is used
// directly as the condition (no operator wraps it).
TEST_F(ElabIfTest, Assigner_SecondGenIf_ConditionIsBareInvertReference) {
  const hldb::Module *const m = getModule("assigner");
  ASSERT_NE(m, nullptr);
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(m);
  ASSERT_EQ(genIfs.size(), 2u);

  const hldb::RefObj *const cond = genIfs[1]->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "'if (invert)': bare condition must be a RefObj (reference to invert)";
  EXPECT_EQ(cond->getName(), "invert");
}

// 'assign out = ~inp;' -- IEEE 1800-2023 11.4.1 "~" is vpiBitNegOp.
TEST_F(ElabIfTest, Assigner_SecondGenIf_BodyIsContAssignOutFromBitwiseNotInp) {
  const hldb::Module *const m = getModule("assigner");
  ASSERT_NE(m, nullptr);
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(m);
  ASSERT_EQ(genIfs.size(), 2u);

  const hldb::ContAssign *const assign = genIfs[1]->getStmt<hldb::ContAssign>();
  ASSERT_NE(assign, nullptr) << "'assign out = ~inp;' body must be a ContAssign";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "out");

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'~inp': RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitNegOp) << "'~inp' must produce vpiBitNegOp";
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), "inp");
}

// ---------------------------------------------------------------------------
// middleman: 'parameter invert = 0;' propagated by name into 'assigner'.
// ---------------------------------------------------------------------------

TEST_F(ElabIfTest, Middleman_InvertParamNotLocalWithDefaultZero) {
  const hldb::Module *const m = getModule("middleman");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const invert = findParam(m, "invert");
  ASSERT_NE(invert, nullptr) << "'parameter invert' not found on 'middleman'";
  EXPECT_FALSE(invert->getLocalParam());
}

TEST_F(ElabIfTest, Middleman_InstantiatesAssignerWithInvertPropagated) {
  const hldb::Module *const m = getModule("middleman");
  ASSERT_NE(m, nullptr);
  const hldb::RefInstance *const asgn = findRefInst("asgn", m);
  ASSERT_NE(asgn, nullptr) << "'assigner #(...) asgn ()' RefInstance not found in 'middleman'";
  ASSERT_NE(asgn->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = asgn->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "asgn's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("assigner"));

  const hldb::ParamAssign *const pa = findParamAssign(asgn, "invert");
  ASSERT_NE(pa, nullptr) << "'.invert(invert)' override not found on 'asgn'";
  EXPECT_TRUE(pa->getConnByName()) << "'.invert(...)' is a by-name parameter connection (Sec 23.3)";
  EXPECT_TRUE(pa->getOverridden());

  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'.invert(invert)': RHS must be a RefObj (reference to middleman's own invert)";
  EXPECT_EQ(rhs->getName(), "invert");
}

// ---------------------------------------------------------------------------
// top: two middleman instances overriding 'invert' with distinct constants
// (IEEE 1800-2023 23.10 "Overriding module parameters").
// ---------------------------------------------------------------------------

TEST_F(ElabIfTest, Top_InstantiatesMiddlemanTwiceWithDistinctInvertOverrides) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);

  const hldb::RefInstance *const mdl1 = findRefInst("mdl1", top);
  ASSERT_NE(mdl1, nullptr) << "'middleman #(.invert(1)) mdl1()' RefInstance not found";
  ASSERT_NE(mdl1->getTypespec(), nullptr);
  EXPECT_EQ(mdl1->getTypespec()->getActual<hldb::ModuleTypespec>()->getName(), std::string_view("middleman"));
  const hldb::ParamAssign *const pa1 = findParamAssign(mdl1, "invert");
  ASSERT_NE(pa1, nullptr) << "'.invert(1)' override not found on 'mdl1'";
  EXPECT_TRUE(pa1->getConnByName());
  EXPECT_TRUE(pa1->getOverridden());
  const hldb::Constant *const c1 = pa1->getRhs<hldb::Constant>();
  ASSERT_NE(c1, nullptr) << "'.invert(1)': RHS must be a Constant";
  EXPECT_EQ(std::string(c1->getDecompile()), "1");

  const hldb::RefInstance *const mdl0 = findRefInst("mdl0", top);
  ASSERT_NE(mdl0, nullptr) << "'middleman #(.invert(0)) mdl0()' RefInstance not found";
  const hldb::ParamAssign *const pa0 = findParamAssign(mdl0, "invert");
  ASSERT_NE(pa0, nullptr) << "'.invert(0)' override not found on 'mdl0'";
  EXPECT_TRUE(pa0->getConnByName());
  EXPECT_TRUE(pa0->getOverridden());
  const hldb::Constant *const c0 = pa0->getRhs<hldb::Constant>();
  ASSERT_NE(c0, nullptr) << "'.invert(0)': RHS must be a Constant";
  EXPECT_EQ(std::string(c0->getDecompile()), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
