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

// Tests for 20.9--countbits.sv (tags: 20.9)
//   module top();
//   initial begin
//   	logic [31:0] val = 32'h70008421;
//   	$display(":assert: (%d == 7)", $countbits(val, '1));
//   	$display(":assert: (%d == 7)", $countones(val));
//   	$display(":assert: (%d == 25)", $countbits(val, '0));
//   	$display(":assert: (%d == 32)", $countbits(val, '0, '1));
//   	$display(":assert: (%d == 0)", $countbits(val, 'x, 'z));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.9, "Bit vector system functions":
// "$countbits(expression, control_bit {, control_bit})" counts the bits of
// expression that match any of the control bits; "$countones(expression)"
// is equivalent to "$countbits(expression, '1)".
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     declaring exactly 1 variable and holding exactly 5 statements
//   - the variable is "val", typed by a LogicTypespec that is a vector with
//     exactly 1 range [31:0] (both bounds Constant unsigned int, size 64),
//     and initialized by a Constant hex "32'h70008421" (size 32) whose
//     typespec resolves to an IntTypespec
//   - each of the 5 statements is a SysTaskCall named "$display" with
//     exactly 2 arguments: a Constant string format (sizes 144, 144, 152,
//     152, 144) and a SysFuncCall
//   - the SysFuncCalls are, in order:
//       $countbits(val, '1)      -- 2 arguments
//       $countones(val)          -- 1 argument
//       $countbits(val, '0)      -- 2 arguments
//       $countbits(val, '0, '1)  -- 3 arguments
//       $countbits(val, 'x, 'z)  -- 3 arguments
//   - in every call the first argument is a RefObj "val" bound to the
//     Begin's Variable "val"
//   - every control bit is a Constant binary of size 1; '0 and '1 resolve to
//     a BitTypespec, while 'x and 'z resolve to a LogicTypespec
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the bit counts 7, 7, 25, 32 and 0 and that
// the ":assert:" comparisons hold) cannot be observed -- HLC is a
// compiler/elaborator with no simulation capability, so no execution ever
// happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
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

class CountbitsFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.9--countbits.hlc"}); }
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

  // "logic [31:0] val = 32'h70008421;" -- declared inside the begin block.
  static const hldb::Variable *getVal() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("val", body->getVariables());
  }

  // stmtIndex is the position of the $display statement in the begin block.
  static const hldb::SysTaskCall *getDisplayCall(size_t stmtIndex) {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= stmtIndex) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(stmtIndex));
  }

  // The SysFuncCall passed as the second argument of the stmtIndex-th $display.
  static const hldb::SysFuncCall *getBitCall(size_t stmtIndex) {
    const hldb::SysTaskCall *const display = getDisplayCall(stmtIndex);
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
  }

  static void expectDisplayFormat(size_t stmtIndex, std::string_view text, int32_t size) {
    const hldb::SysTaskCall *const call = getDisplayCall(stmtIndex);
    ASSERT_NE(call, nullptr) << "statement " << stmtIndex << " should be a $display SysTaskCall";
    EXPECT_EQ(call->getName(), "$display");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 2u) << "statement " << stmtIndex << ": a format and one call";

    const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
    EXPECT_EQ(fmt->getConstType(), vpiStringConst);
    EXPECT_EQ(fmt->getSize(), size) << "8 bits per character";
    EXPECT_EQ(fmt->getValue(), text);
    const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
    ASSERT_NE(fmtRef, nullptr);
    EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);
  }

  // Every call's first argument is a reference to the declared "val".
  static void expectValReference(const hldb::Any *arg) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(arg);
    ASSERT_NE(ref, nullptr) << "'val' should be a RefObj";
    EXPECT_EQ(ref->getName(), "val");
    ASSERT_NE(getVal(), nullptr);
    EXPECT_EQ(ref->getActual<hldb::Variable>(), getVal()) << "'val' should bind to the Begin's Variable 'val'";
  }

  // A control bit ('0, '1, 'x or 'z) is a 1-bit binary Constant. TypespecT is
  // BitTypespec for '0 / '1 and LogicTypespec for 'x / 'z.
  template <typename TypespecT>
  static void expectControlBit(const hldb::Any *arg, std::string_view decompile) {
    const hldb::Constant *const bit = any_cast<hldb::Constant>(arg);
    ASSERT_NE(bit, nullptr) << decompile << " should be a Constant";
    EXPECT_EQ(bit->getConstType(), vpiBinaryConst);
    EXPECT_EQ(bit->getSize(), 1);
    EXPECT_EQ(bit->getDecompile(), decompile);

    const hldb::RefTypespec *const ref = bit->getTypespec();
    ASSERT_NE(ref, nullptr);
    EXPECT_NE(ref->getActual<TypespecT>(), nullptr) << decompile << " has an unexpected typespec";
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(CountbitsFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(CountbitsFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(CountbitsFunctionTest, InitialBodyIsBeginWithOneVariableAndFiveStmts) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  ASSERT_NE(body->getVariables(), nullptr);
  EXPECT_EQ(body->getVariables()->size(), 1u) << "'val' is the only declaration in the begin block";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 5u) << "the begin block holds five '$display(...);' statements";
}

// --- logic [31:0] val = 32'h70008421; ----------------------------------------

TEST_F(CountbitsFunctionTest, ValIsLogicVector31To0) {
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr) << "'val' should be a Variable of the begin block";
  const hldb::RefTypespec *const ref = val->getTypespec();
  ASSERT_NE(ref, nullptr);
  const hldb::LogicTypespec *const logic = ref->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logic, nullptr) << "'logic [31:0]' should have a LogicTypespec";
  EXPECT_TRUE(logic->getVector());
  ASSERT_NE(logic->getRanges(), nullptr);
  ASSERT_EQ(logic->getRanges()->size(), 1u) << "'[31:0]' is a single packed range";

  const hldb::Range *const range = logic->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getConstType(), vpiUIntConst);
  EXPECT_EQ(left->getSize(), 64);
  EXPECT_EQ(left->getDecompile(), "31");
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getConstType(), vpiUIntConst);
  EXPECT_EQ(right->getSize(), 64);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(CountbitsFunctionTest, ValInitializerIsHexConstant) {
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr);
  const hldb::Constant *const init = val->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'= 32'h70008421' should be a Constant initializer";
  EXPECT_EQ(init->getConstType(), vpiHexConst);
  EXPECT_EQ(init->getSize(), 32);
  EXPECT_EQ(init->getDecompile(), "32'h70008421");

  const hldb::RefTypespec *const ref = init->getTypespec();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::IntTypespec>(), nullptr);
}

// --- $display(":assert: (%d == N)", ...); x 5 --------------------------------

TEST_F(CountbitsFunctionTest, DisplayCallsHaveExpectedFormats) {
  expectDisplayFormat(0, ":assert: (%d == 7)", 144);
  expectDisplayFormat(1, ":assert: (%d == 7)", 144);
  expectDisplayFormat(2, ":assert: (%d == 25)", 152);
  expectDisplayFormat(3, ":assert: (%d == 32)", 152);
  expectDisplayFormat(4, ":assert: (%d == 0)", 144);
}

// --- $countbits(val, '1) ------------------------------------------------------

TEST_F(CountbitsFunctionTest, CountbitsOfOnes) {
  const hldb::SysFuncCall *const call = getBitCall(0);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$countbits");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  expectValReference(call->getArguments()->at(0));
  expectControlBit<hldb::BitTypespec>(call->getArguments()->at(1), "'1");
}

// --- $countones(val) ----------------------------------------------------------

TEST_F(CountbitsFunctionTest, CountonesOfVal) {
  const hldb::SysFuncCall *const call = getBitCall(1);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$countones");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.9: '$countones' takes only the expression";
  expectValReference(call->getArguments()->at(0));
}

// --- $countbits(val, '0) ------------------------------------------------------

TEST_F(CountbitsFunctionTest, CountbitsOfZeros) {
  const hldb::SysFuncCall *const call = getBitCall(2);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$countbits");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  expectValReference(call->getArguments()->at(0));
  expectControlBit<hldb::BitTypespec>(call->getArguments()->at(1), "'0");
}

// --- $countbits(val, '0, '1) --------------------------------------------------

TEST_F(CountbitsFunctionTest, CountbitsOfZerosAndOnes) {
  const hldb::SysFuncCall *const call = getBitCall(3);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$countbits");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 3u) << "20.9: each control bit is a separate argument";
  expectValReference(call->getArguments()->at(0));
  expectControlBit<hldb::BitTypespec>(call->getArguments()->at(1), "'0");
  expectControlBit<hldb::BitTypespec>(call->getArguments()->at(2), "'1");
}

// --- $countbits(val, 'x, 'z) --------------------------------------------------

TEST_F(CountbitsFunctionTest, CountbitsOfXAndZ) {
  const hldb::SysFuncCall *const call = getBitCall(4);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$countbits");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 3u) << "20.9: each control bit is a separate argument";
  expectValReference(call->getArguments()->at(0));
  expectControlBit<hldb::LogicTypespec>(call->getArguments()->at(1), "'x");
  expectControlBit<hldb::LogicTypespec>(call->getArguments()->at(2), "'z");
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(CountbitsFunctionTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
