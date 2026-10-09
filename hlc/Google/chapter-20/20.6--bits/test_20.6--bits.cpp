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

// Tests for 20.6--bits.sv (tags: 20.6)
//   module top();
//   initial begin
//   	logic [31:0] val;
//   	$display(":assert: (%d == 32)", $bits(val));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.6.2, "Expression size system function":
// "size_function ::= $bits ( expression ) | $bits ( data_type )" -- "The
// $bits system function returns the number of bits required to hold an
// expression as a bit stream." Here it is called with a single expression
// argument: the variable "val".
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 variable and 1 statement
//   - the variable is "val": LogicTypespec, vector, with exactly 1 range
//     whose left/right bounds are Constant unsigned int "31" / "0"
//     (size 64, IntTypespec)
//   - the statement is a SysTaskCall named "$display" with exactly 2
//     NamedArgument arguments: the first's high conn is a Constant string
//     ":assert: (%d == 32)" (size 152) and the second's high conn is a
//     SysFuncCall named "$bits"
//   - the "$bits" SysFuncCall has exactly 1 NamedArgument argument whose
//     high conn is a RefObj "val" resolving to the Variable "val"
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that $bits(val) actually evaluates to 32
// and that the ":assert:" comparison holds) cannot be observed -- HLC is a
// compiler/elaborator with no simulation capability, so no execution ever
// happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class BitsFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.6--bits.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::Variable *getValVariable() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getVariables() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("val", body->getVariables());
  }

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  }

  static const hldb::SysFuncCall *getBitsCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg1 = display->getArguments()->at(1);
    if (arg1 == nullptr) {
      return nullptr;
    }
    return arg1->getHighConn<hldb::SysFuncCall>();
  }

  // Checks one range bound: a Constant unsigned int with the given decompiled
  // text, size 64, and an IntTypespec.
  static void checkRangeBound(const hldb::Constant *bound, std::string_view literal) {
    ASSERT_NE(bound, nullptr) << "range bound '" << literal << "' should be a Constant";
    EXPECT_EQ(bound->getConstType(), vpiUIntConst);
    EXPECT_EQ(bound->getSize(), 64);
    EXPECT_EQ(bound->getDecompile(), literal);
    const hldb::RefTypespec *const ref = bound->getTypespec();
    ASSERT_NE(ref, nullptr);
    EXPECT_NE(ref->getActual<hldb::IntTypespec>(), nullptr);
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(BitsFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(BitsFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(BitsFunctionTest, InitialBodyIsBeginWithOneVariableAndOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  ASSERT_NE(body->getVariables(), nullptr);
  EXPECT_EQ(body->getVariables()->size(), 1u) << "'logic [31:0] val;' is the only variable declaration";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- logic [31:0] val; -----------------------------------------------------------

TEST_F(BitsFunctionTest, ValIsLogicVectorVariable) {
  const hldb::Variable *const val = getValVariable();
  ASSERT_NE(val, nullptr) << "'val' should be a Variable";
  EXPECT_EQ(val->getName(), "val");
  const hldb::RefTypespec *const ref = val->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(ref, nullptr);
  const hldb::LogicTypespec *const ts = ref->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ts, nullptr) << "'logic [31:0] val' should have a LogicTypespec";
  EXPECT_TRUE(ts->getVector());
}

TEST_F(BitsFunctionTest, ValHasRange31To0) {
  const hldb::Variable *const val = getValVariable();
  ASSERT_NE(val, nullptr);
  const hldb::RefTypespec *const ref = val->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(ref, nullptr);
  const hldb::LogicTypespec *const ts = ref->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ts, nullptr);
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 1u) << "'[31:0]' is a single packed range";

  const hldb::Range *const range = ts->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  checkRangeBound(range->getLeftExpr<hldb::Constant>(), "31");
  checkRangeBound(range->getRightExpr<hldb::Constant>(), "0");
}

// --- $display(":assert: (%d == 32)", $bits(val)); ------------------------------

TEST_F(BitsFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(BitsFunctionTest, DisplayCallHasFormatAndBitsArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 152) << "19 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 32)");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  EXPECT_NE(getBitsCall(), nullptr) << "'$bits(val)' should be a SysFuncCall";
}

// --- $bits(val) -------------------------------------------------------------------

TEST_F(BitsFunctionTest, BitsCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getBitsCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$bits");
}

TEST_F(BitsFunctionTest, BitsCallHasValArgument) {
  const hldb::SysFuncCall *const call = getBitsCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.6.2: '$bits' takes a single expression or data_type";

  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::RefObj *const arg = arg0->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg, nullptr) << "'val' should be a RefObj";
  EXPECT_EQ(arg->getName(), "val");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getValVariable());
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(BitsFunctionTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
