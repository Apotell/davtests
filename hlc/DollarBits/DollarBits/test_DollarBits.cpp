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

// Tests for tests/DollarBits/dut.sv:
//
//   module other #(parameter int Width = 1)
//     (input logic[Width-1:0] in, output logic[Width-1:0] out);
//      assign out = in;
//   endmodule;
//
//   module dut (input logic[7:0] in, output logic[7:0] out);
//      other #(.Width($bits({in}))) oth(.in(in), .out(out));
//   endmodule
//
// IEEE 1800-2023 Sec 20.6.2 defines '$bits(expression)' as a constant
// system function returning the number of bits required to hold the
// given expression/type. Here it is applied to a concatenation
// '{in}' (Sec 11.4.12 concatenation, applied here to a single operand --
// legal per the concatenation grammar, which allows one or more
// expressions), used as an instance parameter override on "oth".
//
// Checked:
//   - modules "other" and "dut" both exist
//   - "other" declares parameter "Width" (not a localparam), whose default
//     ParamAssign RHS decompiles to "1"
//   - "dut" instantiates "other" via RefInstance "oth", whose typespec
//     resolves (ModuleTypespec) to "other"
//   - "oth" carries an explicit, overriding, by-name ParamAssign for
//     "Width" (Sec 23.3 named parameter value assignment)
//   - that ParamAssign's RHS is a SysFuncCall "$bits" with exactly one
//     argument
//   - the $bits argument is an Operation (vpiConcatOp, Sec 11.4.12) with
//     exactly one operand, a RefObj naming "in" that resolves to an actual
//     (dut's "in" port)
//   - compiler reports zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DollarBitsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DollarBits.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getOther() {
    return hldb::findByDefName<hldb::Module>("other", m_design->getAllModules());
  }

  static const hldb::Module *getDut() { return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules()); }

  template <typename ScopeT>
  static const hldb::ParamAssign *findParamAssign(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(scope));
  }
};

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

TEST_F(DollarBitsTest, ModuleOtherExists) { EXPECT_NE(getOther(), nullptr) << "module 'other' not found"; }

TEST_F(DollarBitsTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr) << "module 'dut' not found"; }

// ---------------------------------------------------------------------------
// 'other' declares parameter Width
// ---------------------------------------------------------------------------

TEST_F(DollarBitsTest, OtherHasWidthParameterNotLocalParam) {
  const hldb::Module *const other = getOther();
  ASSERT_NE(other, nullptr);
  ASSERT_NE(other->getParameters(), nullptr);
  const hldb::Parameter *width = nullptr;
  for (const hldb::Any *const p : *other->getParameters()) {
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
    if (param != nullptr && param->getName() == "Width") {
      width = param;
      break;
    }
  }
  ASSERT_NE(width, nullptr) << "'parameter int Width' not found on module 'other'";
  EXPECT_FALSE(width->getLocalParam()) << "'parameter int Width' is a module parameter, not a localparam";
}

TEST_F(DollarBitsTest, OtherWidthDefaultIsOne) {
  const hldb::Module *const other = getOther();
  ASSERT_NE(other, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(other, "Width");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'Width' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'parameter int Width = 1': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "1");
}

// ---------------------------------------------------------------------------
// 'dut' instantiates 'other' as 'oth', overriding Width via $bits({in})
// ---------------------------------------------------------------------------

TEST_F(DollarBitsTest, DutInstantiatesOtherAsOth) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getRefInstances(), nullptr);
  const hldb::RefInstance *const oth = hldb::findByName<hldb::RefInstance>("oth", dut->getRefInstances());
  ASSERT_NE(oth, nullptr) << "'other #(...) oth (...)' RefInstance not found in 'dut'";
  ASSERT_NE(oth->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = oth->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "'oth's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("other"));
}

TEST_F(DollarBitsTest, OthWidthOverrideIsByNameAndOverriding) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::RefInstance *const oth = hldb::findByName<hldb::RefInstance>("oth", dut->getRefInstances());
  ASSERT_NE(oth, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(oth, "Width");
  ASSERT_NE(pa, nullptr) << "'.Width($bits({in}))' override not found on 'oth'";
  EXPECT_TRUE(pa->getConnByName()) << "'.Width(...)' is a by-name parameter connection (Sec 23.3)";
  EXPECT_TRUE(pa->getOverridden()) << "an explicit instance-level override must be marked as overriding the default";
}

TEST_F(DollarBitsTest, OthWidthOverrideRhsIsBitsSysFuncCallWithOneArgument) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::RefInstance *const oth = hldb::findByName<hldb::RefInstance>("oth", dut->getRefInstances());
  ASSERT_NE(oth, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(oth, "Width");
  ASSERT_NE(pa, nullptr);
  const hldb::SysFuncCall *const bits = pa->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr) << "Sec 20.6.2: '.Width($bits({in}))' RHS must be a SysFuncCall";
  EXPECT_EQ(bits->getName(), "$bits");
  ASSERT_NE(bits->getArguments(), nullptr);
  EXPECT_EQ(bits->getArguments()->size(), 1u);
}

TEST_F(DollarBitsTest, BitsArgumentIsSingleOperandConcatOfIn) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::RefInstance *const oth = hldb::findByName<hldb::RefInstance>("oth", dut->getRefInstances());
  ASSERT_NE(oth, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(oth, "Width");
  ASSERT_NE(pa, nullptr);
  const hldb::SysFuncCall *const bits = pa->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr);
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);

  const hldb::Operation *const concat = any_cast<hldb::Operation>(bits->getArguments()->at(0));
  ASSERT_NE(concat, nullptr) << "Sec 11.4.12: '{in}' is a concatenation Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 1u) << "'{in}' concatenates exactly one operand";

  const hldb::RefObj *const in = any_cast<hldb::RefObj>(concat->getOperands()->at(0));
  ASSERT_NE(in, nullptr);
  EXPECT_EQ(in->getName(), "in");
  EXPECT_NE(in->getActual(), nullptr) << "'in' inside '{in}' must resolve to dut's 'in' port";
}

// ---------------------------------------------------------------------------
// Compiler diagnostics
// ---------------------------------------------------------------------------

TEST_F(DollarBitsTest, CompilerReportsZeroErrors) {
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
