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

// Tests for dut.sv (tags: LocalParam)
//   module top (input wire i, output reg o1, output reg o2);
//     assigner #(.invert(0)) asgn0(.inp(i), .out(o1));
//     assigner #(.invert(1)) asgn1(.inp(i), .out(o2));
//   endmodule
//
//   module assigner #(parameter invert = 0) (input wire inp, output reg out);
//    localparam int do_invert = 0;
//    if (!invert)
//      assign out = inp;
//    if (invert)
//      assign out = ~inp;
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - both module definitions 'top' and 'assigner' exist; the definition
//     of 'assigner' is named by its module_identifier (23.2.1)
//   - 'invert' is a (non-local) value parameter with default value 0; it
//     has no type/range so it is a logic vector (6.20.2)
//   - 'do_invert' is a local parameter (6.20.4) of type int with value 0
//   - 'top' instantiates 'assigner' twice (23.3.2) with named parameter
//     value assignments (23.10.2.2): asgn0 overrides invert to 0, asgn1
//     overrides invert to 1
//   - the named port connections (23.3.2.2): .inp(i) and .out(o1)/.out(o2)
//   - 'top's ports: i is an input wire (net), o1/o2 are output reg
//     (variables, 6.8)
//   - the two conditional generate constructs in 'assigner' (27.5):
//     'if (!invert)' -> condition is vpiNotOp over a reference to the
//     parameter; body is the continuous assignment out = inp
//     'if (invert)' -> condition references the parameter; body is
//     out = ~inp (vpiBitNegOp)
//
// What is NOT checked and why:
//   - the size reported for unsized literals such as '0' (5.7.1 only
//     requires "at least 32 bits"; implementation-defined).
//   - which generate branch is selected per instance: that requires an
//     elaborated design, and this .hlc does not request elaboration.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LocalParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LocalParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Module *getAssigner() {
    return hldb::findByDefName<hldb::Module>("assigner", m_design->getAllModules());
  }

  static const hldb::Parameter *findParam(const hldb::Module *m, std::string_view name) {
    if (m == nullptr || m->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *m->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  static const hldb::ParamAssign *findParamAssign(const hldb::Module *m, std::string_view name) {
    if (m == nullptr || m->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *m->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if (lhs != nullptr && lhs->getName() == name) return pa;
      const hldb::Parameter *const lhsp = pa->getLhs<hldb::Parameter>();
      if (lhsp != nullptr && lhsp->getName() == name) return pa;
    }
    return nullptr;
  }

  static const hldb::RefInstance *findInst(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getRefInstances() == nullptr) return nullptr;
    for (const hldb::RefInstance *const ri : *top->getRefInstances()) {
      if (ri->getName() == name) return ri;
    }
    return nullptr;
  }

  static std::vector<const hldb::GenIf *> getGenIfs() {
    std::vector<const hldb::GenIf *> result;
    const hldb::Module *const m = getAssigner();
    if (m == nullptr || m->getGenStmts() == nullptr) return result;
    for (const hldb::Any *const s : *m->getGenStmts()) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(s)) result.emplace_back(gi);
    }
    return result;
  }

  // Returns the single ContAssign inside a generate branch body, whether the
  // body is represented directly or wrapped in an (implicit) Begin.
  static const hldb::ContAssign *getBranchAssign(const hldb::GenIf *gi) {
    if (gi == nullptr || gi->getStmt() == nullptr) return nullptr;
    if (const hldb::ContAssign *const ca = gi->getStmt<hldb::ContAssign>()) return ca;
    const hldb::Begin *const b = gi->getStmt<hldb::Begin>();
    if (b == nullptr || b->getStmts() == nullptr || b->getStmts()->size() != 1) return nullptr;
    return any_cast<hldb::ContAssign>(b->getStmts()->at(0));
  }

  static void checkInstance(std::string_view instName, std::string_view invertValue, std::string_view outActual) {
    const hldb::RefInstance *const ri = findInst(instName);
    ASSERT_NE(ri, nullptr) << "instance '" << instName << "' not found";
    ASSERT_NE(ri->getTypespec(), nullptr);
    const hldb::ModuleTypespec *const mt = ri->getTypespec()->getActual<hldb::ModuleTypespec>();
    ASSERT_NE(mt, nullptr) << "instance type must be a module typespec";
    EXPECT_EQ(mt->getDefName(), "assigner");

    // 23.10.2.2: #(.invert(N)) -- named parameter value assignment
    ASSERT_NE(mt->getParamAssigns(), nullptr);
    ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
    const hldb::ParamAssign *const pa = mt->getParamAssigns()->at(0);
    EXPECT_TRUE(pa->getConnByName());
    EXPECT_TRUE(pa->getOverridden());
    const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "invert");
    ASSERT_NE(lhs->getActual(), nullptr);
    EXPECT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Parameter);
    const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getValue(), invertValue);

    // 23.3.2.2: .inp(i), .out(oN)
    ASSERT_NE(ri->getPorts(), nullptr);
    ASSERT_EQ(ri->getPorts()->size(), 2u);
    const hldb::Port *const p0 = any_cast<hldb::Port>(ri->getPorts()->at(0));
    const hldb::Port *const p1 = any_cast<hldb::Port>(ri->getPorts()->at(1));
    ASSERT_NE(p0, nullptr);
    ASSERT_NE(p1, nullptr);
    const hldb::RefObj *const low0 = p0->getLowConn<hldb::RefObj>();
    const hldb::RefObj *const high0 = p0->getHighConn<hldb::RefObj>();
    const hldb::RefObj *const low1 = p1->getLowConn<hldb::RefObj>();
    const hldb::RefObj *const high1 = p1->getHighConn<hldb::RefObj>();
    ASSERT_NE(low0, nullptr);
    ASSERT_NE(high0, nullptr);
    ASSERT_NE(low1, nullptr);
    ASSERT_NE(high1, nullptr);
    EXPECT_EQ(low0->getName(), "inp");
    EXPECT_EQ(high0->getName(), "i");
    EXPECT_EQ(low1->getName(), "out");
    EXPECT_EQ(high1->getName(), outActual);
    ASSERT_NE(high0->getActual(), nullptr);
    EXPECT_EQ(high0->getActual()->getAnyType(), hldb::AnyType::Net);
    ASSERT_NE(high1->getActual(), nullptr);
    EXPECT_EQ(high1->getActual()->getAnyType(), hldb::AnyType::Variable);
  }
};

// ---------------------------------------------------------------------------
// Module definitions -- 23.2.1
// ---------------------------------------------------------------------------

TEST_F(LocalParamTest, ModulesExist) {
  EXPECT_NE(getTop(), nullptr) << "module 'top' not found";
  EXPECT_NE(getAssigner(), nullptr) << "module 'assigner' not found";
}

TEST_F(LocalParamTest, AssignerDefinitionIsNamedByModuleIdentifier) {
  const hldb::Module *const m = getAssigner();
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), "assigner")
      << "23.2.1: the module definition's name is its module_identifier 'assigner'; parameter overrides of "
         "individual instances are not part of the definition's name";
}

// ---------------------------------------------------------------------------
// parameter invert = 0 -- 6.20.2
// ---------------------------------------------------------------------------

TEST_F(LocalParamTest, InvertIsNonLocalParameterDefaultZero) {
  const hldb::Module *const m = getAssigner();
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = findParam(m, "invert");
  ASSERT_NE(p, nullptr) << "'parameter invert' not found";
  EXPECT_FALSE(p->getLocalParam()) << "'invert' is declared with 'parameter', not 'localparam'";
  const hldb::ParamAssign *const pa = findParamAssign(m, "invert");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "0");
}

TEST_F(LocalParamTest, UntypedInvertIsLogicVector) {
  const hldb::Module *const m = getAssigner();
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = findParam(m, "invert");
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  ASSERT_NE(p->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(p->getTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec)
      << "6.20.2: a parameter with no type or range whose final value is integral is a logic vector";
}

// ---------------------------------------------------------------------------
// localparam int do_invert = 0 -- 6.20.4
// ---------------------------------------------------------------------------

TEST_F(LocalParamTest, DoInvertIsLocalParamOfTypeInt) {
  const hldb::Module *const m = getAssigner();
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = findParam(m, "do_invert");
  ASSERT_NE(p, nullptr) << "'localparam int do_invert' not found";
  EXPECT_TRUE(p->getLocalParam());
  ASSERT_NE(p->getTypespec(), nullptr);
  ASSERT_NE(p->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(p->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
  const hldb::ParamAssign *const pa = findParamAssign(m, "do_invert");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "0");
}

TEST_F(LocalParamTest, AssignerHasExactlyTwoParameters) {
  const hldb::Module *const m = getAssigner();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 2u);
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getParameters() == nullptr || top->getParameters()->empty()) << "'top' declares no parameters";
}

// ---------------------------------------------------------------------------
// top ports -- 23.2.2.3, 6.8
// ---------------------------------------------------------------------------

TEST_F(LocalParamTest, TopPortsDirectionsAndKinds) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 3u);
  const char *const names[] = {"i", "o1", "o2"};
  const int32_t dirs[] = {vpiInput, vpiOutput, vpiOutput};
  const hldb::AnyType kinds[] = {hldb::AnyType::Net, hldb::AnyType::Variable, hldb::AnyType::Variable};
  for (size_t idx = 0; idx < 3; ++idx) {
    const hldb::Port *const p = top->getPorts()->at(idx);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getName(), names[idx]);
    EXPECT_EQ(p->getDirection(), dirs[idx]);
    const hldb::RefObj *const low = p->getLowConn<hldb::RefObj>();
    ASSERT_NE(low, nullptr);
    ASSERT_NE(low->getActual(), nullptr);
    EXPECT_EQ(low->getActual()->getAnyType(), kinds[idx])
        << "port '" << names[idx] << "': 'wire' declares a net, 'reg' declares a variable";
  }
}

// ---------------------------------------------------------------------------
// Instances -- 23.3.2, 23.10.2.2
// ---------------------------------------------------------------------------

TEST_F(LocalParamTest, TopHasTwoAssignerInstances) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 2u);
  EXPECT_NE(findInst("asgn0"), nullptr);
  EXPECT_NE(findInst("asgn1"), nullptr);
}

TEST_F(LocalParamTest, Asgn0OverridesInvertToZero) { checkInstance("asgn0", "0", "o1"); }

TEST_F(LocalParamTest, Asgn1OverridesInvertToOne) { checkInstance("asgn1", "1", "o2"); }

// ---------------------------------------------------------------------------
// Conditional generate constructs -- 27.5
// ---------------------------------------------------------------------------

TEST_F(LocalParamTest, AssignerHasTwoGenerateIfs) {
  EXPECT_EQ(getGenIfs().size(), 2u) << "two 'if (...)' generate constructs without 'else'";
}

TEST_F(LocalParamTest, FirstGenIfNotInvertAssignsInp) {
  const std::vector<const hldb::GenIf *> gis = getGenIfs();
  ASSERT_EQ(gis.size(), 2u);
  const hldb::Operation *const cond = gis[0]->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'!invert' must be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiNotOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "invert");
  ASSERT_NE(ref->getActual(), nullptr);
  EXPECT_EQ(ref->getActual()->getAnyType(), hldb::AnyType::Parameter);

  const hldb::ContAssign *const ca = getBranchAssign(gis[0]);
  ASSERT_NE(ca, nullptr) << "branch body must be 'assign out = inp;'";
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getName(), "out");
  EXPECT_EQ(rhs->getName(), "inp");
}

TEST_F(LocalParamTest, SecondGenIfInvertAssignsNegatedInp) {
  const std::vector<const hldb::GenIf *> gis = getGenIfs();
  ASSERT_EQ(gis.size(), 2u);
  const hldb::RefObj *const cond = gis[1]->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "'invert' must be a reference";
  EXPECT_EQ(cond->getName(), "invert");
  ASSERT_NE(cond->getActual(), nullptr);
  EXPECT_EQ(cond->getActual()->getAnyType(), hldb::AnyType::Parameter);

  const hldb::ContAssign *const ca = getBranchAssign(gis[1]);
  ASSERT_NE(ca, nullptr) << "branch body must be 'assign out = ~inp;'";
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "out");
  const hldb::Operation *const rhs = ca->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiBitNegOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 1u);
  const hldb::RefObj *const opnd = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(opnd, nullptr);
  EXPECT_EQ(opnd->getName(), "inp");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
