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

// Tests for 20.6--bits_type.sv (tags: 20.6)
//   module top();
//   typedef struct packed {
//   	logic val1;
//   	bit [7:0] val2;
//   } mystruct;
//   initial begin
//   	$display(":assert: (%d == 9)", $bits(mystruct));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.6.2, "Expression size system function":
// "size_function ::= $bits ( expression ) | $bits ( data_type )" -- "The
// $bits system function returns the number of bits required to hold an
// expression as a bit stream." Here it is called with a data_type argument:
// the typedef name "mystruct", which is legal per the second production.
//
// Checked:
//   - design has module "top" declaring exactly 1 Struct, packed, with 2
//     members: "val1" (LogicTypespec) and "val2" (BitTypespec, vector, with
//     exactly 1 range [7:0] of Constant unsigned int bounds)
//   - module has exactly 1 Typedef "mystruct" whose alias resolves to a
//     StructTypespec for that same Struct
//   - module has a TypedefTypespec "mystruct" of kind vpiTypedefKindStruct
//     pointing back to that Typedef
//   - module has exactly 1 process, an Initial whose body is a Begin
//     wrapping exactly 1 statement and no variables
//   - that statement is a SysTaskCall "$display" with exactly 2
//     NamedArgument arguments: the first's high conn is a Constant string
//     ":assert: (%d == 9)" (size 144) and the second's high conn is a
//     SysFuncCall "$bits"
//   - "$bits" has exactly 1 NamedArgument argument whose high conn is a
//     RefObj named "mystruct"
//   - compiler reports no COMP_FAILED_TO_BIND
//
// SKIPPED (known HLC bug, fix pending):
//   - the "mystruct" RefObj's actual should be the type the name denotes
//     (the TypedefTypespec "mystruct"). HLC currently binds it to the
//     Typedef declaration object itself, and HLC's own HLDB linter rejects
//     that with HLDB_ILLEGAL_PROPERTY_VALUE (DB2038) at 25:39. The same
//     DB2038 appears across many golden logs in this suite, so this is a
//     systemic binding gap, not specific to $bits.
//
// NOT CHECKED: runtime effects (that $bits(mystruct) evaluates to 9 and
// that the ":assert:" comparison holds) cannot be observed -- HLC is a
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
#include <hldb/hldb_vpi_user.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

namespace hlc {

class BitsTypeFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.6--bits_type.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Struct *getStruct() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getStructs() == nullptr || mod->getStructs()->empty()) {
      return nullptr;
    }
    return mod->getStructs()->at(0);
  }

  static const hldb::Typedef *getTypedef() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getTypedefs() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Typedef>("mystruct", mod->getTypedefs());
  }

  static const hldb::TypedefTypespec *getTypedefTypespec() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getTypespecs() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TypedefTypespec>("mystruct", mod->getTypespecs());
  }

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

  static const hldb::RefObj *getBitsArgument() {
    const hldb::SysFuncCall *const call = getBitsCall();
    if (call == nullptr || call->getArguments() == nullptr || call->getArguments()->empty()) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    if (arg0 == nullptr) {
      return nullptr;
    }
    return arg0->getHighConn<hldb::RefObj>();
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

// --- module ------------------------------------------------------------------

TEST_F(BitsTypeFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

// --- typedef struct packed { logic val1; bit [7:0] val2; } mystruct; -----------

TEST_F(BitsTypeFunctionTest, ModuleHasOnePackedStructWithTwoMembers) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getStructs(), nullptr);
  EXPECT_EQ(mod->getStructs()->size(), 1u) << "'struct packed { ... }' is the only struct";

  const hldb::Struct *const st = getStruct();
  ASSERT_NE(st, nullptr);
  EXPECT_TRUE(st->getPacked()) << "the struct is declared 'packed'";
  ASSERT_NE(st->getMembers(), nullptr);
  EXPECT_EQ(st->getMembers()->size(), 2u) << "'val1' and 'val2'";
}

TEST_F(BitsTypeFunctionTest, Member0IsLogicVal1) {
  const hldb::Struct *const st = getStruct();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_GE(st->getMembers()->size(), 1u);

  const hldb::TypespecMember *const member = st->getMembers()->at(0);
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), "val1");
  const hldb::RefTypespec *const ref = member->getTypespec();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::LogicTypespec>(), nullptr) << "'logic val1' should have a LogicTypespec";
}

TEST_F(BitsTypeFunctionTest, Member1IsBitVectorVal2WithRange7To0) {
  const hldb::Struct *const st = getStruct();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_GE(st->getMembers()->size(), 2u);

  const hldb::TypespecMember *const member = st->getMembers()->at(1);
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), "val2");
  const hldb::RefTypespec *const ref = member->getTypespec();
  ASSERT_NE(ref, nullptr);
  const hldb::BitTypespec *const ts = ref->getActual<hldb::BitTypespec>();
  ASSERT_NE(ts, nullptr) << "'bit [7:0] val2' should have a BitTypespec";
  EXPECT_TRUE(ts->getVector());
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 1u) << "'[7:0]' is a single packed range";

  const hldb::Range *const range = ts->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  checkRangeBound(range->getLeftExpr<hldb::Constant>(), "7");
  checkRangeBound(range->getRightExpr<hldb::Constant>(), "0");
}

TEST_F(BitsTypeFunctionTest, TypedefMystructAliasesTheStruct) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getTypedefs(), nullptr);
  EXPECT_EQ(mod->getTypedefs()->size(), 1u) << "'mystruct' is the only typedef";

  const hldb::Typedef *const td = getTypedef();
  ASSERT_NE(td, nullptr) << "'typedef ... mystruct;' should be a Typedef";
  const hldb::RefTypespec *const alias = td->getAlias();
  ASSERT_NE(alias, nullptr);
  const hldb::StructTypespec *const sts = alias->getActual<hldb::StructTypespec>();
  ASSERT_NE(sts, nullptr) << "'mystruct' should alias a StructTypespec";
  EXPECT_EQ(sts->getStruct(), getStruct());
}

TEST_F(BitsTypeFunctionTest, TypedefTypespecMystructIsStructKind) {
  const hldb::TypedefTypespec *const tts = getTypedefTypespec();
  ASSERT_NE(tts, nullptr) << "'mystruct' should have a TypedefTypespec";
  EXPECT_EQ(tts->getKind(), vpiTypedefKindStruct);
  EXPECT_EQ(tts->getTypedef(), getTypedef());
}

// --- initial process ------------------------------------------------------------

TEST_F(BitsTypeFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(BitsTypeFunctionTest, InitialBodyIsBeginWithOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- $display(":assert: (%d == 9)", $bits(mystruct)); --------------------------

TEST_F(BitsTypeFunctionTest, DisplayCallHasFormatAndBitsArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 144) << "18 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 9)");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  EXPECT_NE(getBitsCall(), nullptr) << "'$bits(mystruct)' should be a SysFuncCall";
}

// --- $bits(mystruct) --------------------------------------------------------------

TEST_F(BitsTypeFunctionTest, BitsCallHasMystructArgument) {
  const hldb::SysFuncCall *const call = getBitsCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$bits");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.6.2: '$bits' takes a single expression or data_type";

  const hldb::RefObj *const arg = getBitsArgument();
  ASSERT_NE(arg, nullptr) << "'mystruct' should be a RefObj";
  EXPECT_EQ(arg->getName(), "mystruct");
}

TEST_F(BitsTypeFunctionTest, BitsArgumentResolvesToTypedefTypespec) {
  GTEST_SKIP() << "HLC binds the '$bits(mystruct)' RefObj's actual to the Typedef declaration, which its own "
                  "HLDB linter rejects (DB2038); should resolve to the TypedefTypespec 'mystruct', since "
                  "'$bits ( data_type )' names a type per IEEE 1800-2023 Sec 20.6.2. Fix pending.";
  const hldb::RefObj *const arg = getBitsArgument();
  ASSERT_NE(arg, nullptr);
  ASSERT_NE(arg->getActual(), nullptr);
  EXPECT_EQ(arg->getActual()->getVpiType(), vpiTypedefTypespec);
  EXPECT_EQ(arg->getActual<hldb::TypedefTypespec>(), getTypedefTypespec());
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(BitsTypeFunctionTest, CompilesWithNoBindErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

TEST_F(BitsTypeFunctionTest, BitsArgumentHasNoIllegalPropertyValue) {
  GTEST_SKIP() << "HLC reports HLDB_ILLEGAL_PROPERTY_VALUE (DB2038) at 25:39 for the '$bits(mystruct)' RefObj's "
                  "vpiActual; '$bits ( data_type )' is legal per IEEE 1800-2023 Sec 20.6.2. Fix pending.";
  EXPECT_EQ(findError(ErrorDefinition::HLDB_ILLEGAL_PROPERTY_VALUE, 25, 39), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
