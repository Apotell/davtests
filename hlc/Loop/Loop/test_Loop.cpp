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

// Tests for tests/Loop/dut.sv (tags: Loop)
//   module top();
//      test u1();
//      loop u2();
//   endmodule
//   module loop();
//      loop u1();
//   endmodule
//   module test();
//      parameter bht_row_width_p = 10;
//      if (bht_row_width_p) begin
//        test #(.bht_row_width_p(bht_row_width_p-2)) u();
//      end
//   endmodule
//
// The construct under test is recursive module instantiation: "loop"
// instantiates itself unconditionally (a recursion that can never
// terminate), while "test" instantiates itself inside a conditional
// generate guarded by a parameter that decreases on every level
// (10 -> 8 -> 6 -> 4 -> 2 -> 0), so its recursion terminates.
//
// What is checked (IEEE 1800-2023):
//   - 23.3.1 "Top-level modules are modules that are included in the
//     SystemVerilog source text, but do not appear in any module
//     instantiation statement ... This applies even if the module
//     instantiation appears in a generate block that is not itself
//     instantiated". So "top" is the only top-level module; "loop" and
//     "test" both appear in instantiation statements (27.5: "a module
//     containing an instantiation of itself will not be a top-level
//     module").
//   - 23.3.2 module instantiation: top.u1 -> test, top.u2 -> loop,
//     loop.u1 -> loop (self), test's generate block u -> test (self).
//   - 27.5 conditional generate: "if (bht_row_width_p) begin ... end" in
//     test is a GenIf whose condition references the parameter and whose
//     generate block holds the self-instantiation; the instance overrides
//     bht_row_width_p by name (23.10.2.2) with "bht_row_width_p-2"
//     (Operation vpiSubOp).
//   - 6.20.4: test has no parameter_port_list, so its body "parameter" is a
//     nonlocal (overridable) parameter.
//   - 27.5: "With proper use of parameters, the resulting recursion can be
//     made to terminate, resulting in a legitimate model hierarchy." The
//     unconditional "loop u1();" inside module loop can never terminate, so
//     it cannot yield a legitimate model hierarchy and must be diagnosed
//     (ELAB_INSTANTIATION_LOOP for "loop"); test's terminating recursion
//     must not be.
//
// What is NOT checked and why:
//   - the "genblk1" external name of test's unnamed generate block (27.6):
//     that name belongs to the instantiated generate scope in the
//     elaborated hierarchy; this .hlc requests no elaboration and HLDB holds
//     only the folded definitions here.
//   - per-level parameter values of the test recursion (elaboration only).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
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

namespace hlc {

class LoopTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Loop.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // Returns the module that 'inst' instantiates, or nullptr.
  static const hldb::Module *instantiatedModule(const hldb::RefInstance *inst) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr) return nullptr;
    return mt->getModule();
  }

  static const hldb::GenIf *getTestGenIf() {
    const hldb::Module *const test = getModule("test");
    if (test == nullptr || test->getGenStmts() == nullptr || test->getGenStmts()->size() != 1) return nullptr;
    return any_cast<hldb::GenIf>(test->getGenStmts()->at(0));
  }

  static const hldb::RefInstance *getTestSelfInstance() {
    const hldb::GenIf *const gi = getTestGenIf();
    if (gi == nullptr) return nullptr;
    const hldb::Begin *const blk = gi->getStmt<hldb::Begin>();
    if (blk == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u", blk->getStmts());
  }
};

TEST_F(LoopTest, AllThreeModulesExist) {
  EXPECT_NE(getModule("top"), nullptr);
  EXPECT_NE(getModule("loop"), nullptr);
  EXPECT_NE(getModule("test"), nullptr);
}

// 23.3.1 / 27.5
TEST_F(LoopTest, OnlyTopIsTopLevelModule) {
  const hldb::Module *const top = getModule("top");
  const hldb::Module *const loop = getModule("loop");
  const hldb::Module *const test = getModule("test");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(test, nullptr);
  EXPECT_TRUE(top->getTopModule()) << "23.3.1: 'top' appears in no instantiation statement";
  EXPECT_FALSE(loop->getTopModule()) << "27.5: a module containing an instantiation of itself is not top-level";
  EXPECT_FALSE(test->getTopModule()) << "23.3.1: 'test' is instantiated (even inside a generate block)";
}

// 23.3.2
TEST_F(LoopTest, TopInstantiatesTestAndLoop) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 2u);

  const hldb::RefInstance *const u1 = hldb::findByName<hldb::RefInstance>("u1", top->getRefInstances());
  ASSERT_NE(u1, nullptr);
  EXPECT_EQ(instantiatedModule(u1), getModule("test"));

  const hldb::RefInstance *const u2 = hldb::findByName<hldb::RefInstance>("u2", top->getRefInstances());
  ASSERT_NE(u2, nullptr);
  EXPECT_EQ(instantiatedModule(u2), getModule("loop"));
}

TEST_F(LoopTest, LoopInstantiatesItselfUnconditionally) {
  const hldb::Module *const loop = getModule("loop");
  ASSERT_NE(loop, nullptr);
  EXPECT_EQ(loop->getGenStmts(), nullptr) << "no generate construct guards the self-instantiation";
  ASSERT_NE(loop->getRefInstances(), nullptr);
  ASSERT_EQ(loop->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const u1 = loop->getRefInstances()->at(0);
  ASSERT_NE(u1, nullptr);
  EXPECT_EQ(u1->getName(), "u1");
  EXPECT_EQ(instantiatedModule(u1), loop);
}

// 6.20.4: no parameter_port_list -> body 'parameter' is nonlocal.
TEST_F(LoopTest, TestParameterIsNonlocalWithDefault10) {
  const hldb::Module *const test = getModule("test");
  ASSERT_NE(test, nullptr);
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("bht_row_width_p", test->getParameters());
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam());

  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("bht_row_width_p", test->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "10");
}

// 27.5: if-generate on the parameter.
TEST_F(LoopTest, TestHasGenIfOnParameter) {
  const hldb::GenIf *const gi = getTestGenIf();
  ASSERT_NE(gi, nullptr) << "'if (bht_row_width_p) begin ... end' should be the sole GenIf of 'test'";
  const hldb::RefObj *const cond = gi->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "bht_row_width_p");
  ASSERT_NE(cond->getActual(), nullptr);
  const hldb::Module *const test = getModule("test");
  ASSERT_NE(test, nullptr);
  EXPECT_EQ(cond->getActual(), hldb::findByName<hldb::Parameter>("bht_row_width_p", test->getParameters()));
  EXPECT_NE(gi->getStmt<hldb::Begin>(), nullptr) << "generate block written with begin...end";
}

TEST_F(LoopTest, TestInstantiatesItselfInsideGenerateBlock) {
  const hldb::RefInstance *const u = getTestSelfInstance();
  ASSERT_NE(u, nullptr) << "instance 'u' not found in test's generate block";
  EXPECT_EQ(instantiatedModule(u), getModule("test"));
}

// 23.10.2.2: .bht_row_width_p(bht_row_width_p-2)
TEST_F(LoopTest, SelfInstanceOverridesParameterWithDecrement) {
  const hldb::RefInstance *const u = getTestSelfInstance();
  ASSERT_NE(u, nullptr);
  ASSERT_NE(u->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = u->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = mt->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());

  const hldb::Module *const test = getModule("test");
  ASSERT_NE(test, nullptr);
  const hldb::Parameter *const param = hldb::findByName<hldb::Parameter>("bht_row_width_p", test->getParameters());
  ASSERT_NE(param, nullptr);

  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "bht_row_width_p");
  EXPECT_EQ(lhs->getActual(), param);

  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "override value 'bht_row_width_p-2' should be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiSubOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);
  const hldb::RefObj *const a = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "bht_row_width_p");
  EXPECT_EQ(a->getActual(), param) << "the override expression is evaluated in the instantiating scope";
  const hldb::Constant *const b = any_cast<hldb::Constant>(rhs->getOperands()->at(1));
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getDecompile(), "2");
}

// 27.5: the unconditional self-instantiation of 'loop' can never terminate.
TEST_F(LoopTest, NonTerminatingRecursionOfLoopIsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::ELAB_INSTANTIATION_LOOP, "loop"), nullptr)
      << "27.5: only a recursion that terminates yields a legitimate model hierarchy; 'loop u1();' inside "
         "module loop recurses forever";
}

// 27.5: the parameter-guarded recursion of 'test' terminates and is legal.
TEST_F(LoopTest, TerminatingRecursionOfTestIsNotDiagnosed) {
  EXPECT_EQ(findError(ErrorDefinition::ELAB_INSTANTIATION_LOOP, "test"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
