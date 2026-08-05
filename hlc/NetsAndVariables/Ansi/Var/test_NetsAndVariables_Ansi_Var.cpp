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

// Validates the UHDM graph produced for tests/NetsAndVariables/Var.sv, which
// demonstrates the SystemVerilog 'var' keyword (an explicit variable
// declaration, never a net).
//
// Checked:
//   - every 'var'-declared identifier materializes as a hldb::Variable in
//     getVariables(), never as a hldb::Net in getNets() -- 'var_data_type'
//     is never subject to the net/variable ambiguity that ports face, so
//     there must be no Net with the same name (no duplicate)
//   - var_logic / var_bit / var_int / var_byte resolve to their expected
//     typespecs (LogicTypespec / BitTypespec / IntTypespec / ByteTypespec)
//   - var_implicit_logic (bare 'var', no data type) defaults to LogicTypespec
//   - var_vector ('var logic [3:0]') is a packed vector, range 3:0
//   - var_initialized has an initial value, and is also the target of a
//     continuous assignment (SystemVerilog allows 'assign' to drive a
//     variable, not just a net) -- the ContAssign's vpiActual must resolve
//     to the Variable, and var_initialized must still not appear in
//     getNets(): the old Variable-to-Net morph pass (which used to promote
//     continuous-assignment targets to nets) has been removed
//   - two processes exist (one always_comb, one always_ff)
//
// The illegal 'var' + net-type-keyword combinations documented as comments
// in Var.sv (var wire, var tri, var supply0, wire var, and redeclaring a
// var-declared identifier with a net type) are not compiled and so are not
// exercised here; see VarNetTypeCombinationsAreIllegalNotCompiled below.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/bit_typespec.h>
#include <hldb/byte_typespec.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class AnsiVarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Var.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("var_keyword_test", m_design->getAllModules());
  }

  // Asserts 'name' is a hldb::Variable in top->getVariables() and that no
  // hldb::Net with the same name exists in top->getNets() (no duplicate).
  static const hldb::Variable *getVarNoNetDuplicate(const hldb::Module *top, std::string_view name) {
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, top->getVariables());
    EXPECT_NE(v, nullptr) << "'" << name << "' should appear in vpiVariables";
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, top->getNets()), nullptr)
        << "'" << name << "' is 'var'-declared -- it must not also appear in vpiNet";
    return v;
  }
};

TEST_F(AnsiVarTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(AnsiVarTest, VarLogicIsLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = getVarNoNetDuplicate(top, "var_logic");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(AnsiVarTest, VarBitIsBitTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = getVarNoNetDuplicate(top, "var_bit");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::BitTypespec>(), nullptr);
}

TEST_F(AnsiVarTest, VarIntIsIntTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = getVarNoNetDuplicate(top, "var_int");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(AnsiVarTest, VarByteIsByteTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = getVarNoNetDuplicate(top, "var_byte");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::ByteTypespec>(), nullptr);
}

TEST_F(AnsiVarTest, VarImplicitLogicDefaultsToLogicTypespec) {
  // 'var var_implicit_logic;' has no explicit data type; per the LRM this
  // defaults to 'logic'.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = getVarNoNetDuplicate(top, "var_implicit_logic");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(AnsiVarTest, VarVectorIsThreeToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = getVarNoNetDuplicate(top, "var_vector");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(AnsiVarTest, VarInitializedHasInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = getVarNoNetDuplicate(top, "var_initialized");
  ASSERT_NE(v, nullptr);
  const hldb::Constant *const init = v->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'var logic var_initialized = 1'b0;' should have an initial value";
}

TEST_F(AnsiVarTest, TwoContAssignsExist) {
  // 'assign var_initialized = a & b;' and 'assign y = var_logic;'.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 2u);
}

TEST_F(AnsiVarTest, ContAssignTargetsVarInitializedNotPromotedToNet) {
  // SystemVerilog allows a continuous assignment to target a variable, not
  // just a net -- 'assign var_initialized = a & b;' is legal. This must not
  // promote var_initialized to a Net (the deprecated Variable-to-Net morph
  // pass used to do exactly that for continuous-assignment targets).
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_FALSE(top->getContAssigns()->empty());
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "var_initialized") << "expected the first ContAssign, in source order";
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr)
      << "vpiActual of the ContAssign lhs should resolve to the Variable";
  EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr) << "var_initialized must not resolve to a Net";
  EXPECT_EQ(hldb::findByName<hldb::Net>("var_initialized", top->getNets()), nullptr);
}

TEST_F(AnsiVarTest, ContAssignTargetsYFromVarLogic) {
  // 'assign y = var_logic;' -- y is the explicit 'output logic y' port
  // (a Variable), driven here by a continuous assignment.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 2u);
  const hldb::RefObj *const lhs = top->getContAssigns()->back()->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "y");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "vpiActual of the ContAssign lhs should resolve to y";
  EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr) << "'y' must not resolve to a Net";
}

TEST_F(AnsiVarTest, TwoProcessesExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 2u) << "one always_comb, one always_ff";
}

TEST_F(AnsiVarTest, VarNetTypeCombinationsAreIllegalNotCompiled) {
  GTEST_SKIP() << "Combining 'var' with a net type keyword (var wire, var tri, var supply0, wire var), and "
                  "redeclaring a var-declared identifier with a net type, are all illegal per the LRM ('var' "
                  "requires a data type, not a net type). These are documented as comments in Var.sv rather than "
                  "compiled, so there is no graph state to assert here.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}