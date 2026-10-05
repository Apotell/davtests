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

// Tests for tests/LogicTypedef/dut.sv:
//
//   module test;
//       typedef logic [1:0] log_two_bits;
//       log_two_bits logn;
//
//       typedef reg [1:0] reg_two_bits;
//       reg_two_bits regn;
//
//       typedef bit [1:0] bit_two_bits;
//       bit_two_bits bitn;
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 6.18 "User-defined types": each 'typedef <type>
//   <name>;' declares a type identifier '<name>' that is another name for
//   '<type>'; the three typedefs are owned by module 'test'.
// IEEE 1800-2023 Sec 6.11.2: "logic and reg denote the same type" -- both
//   'log_two_bits' and 'reg_two_bits' alias a 4-state 2-bit packed vector
//   (modeled as LogicTypespec), while 'bit' is the 2-state type (modeled
//   as BitTypespec).
// IEEE 1800-2023 Sec 7.4.1: '[1:0]' is a single packed dimension with
//   left bound 1 and right bound 0. Sec 6.11.3: logic/reg/bit vectors are
//   unsigned unless declared signed.
// IEEE 1800-2023 Sec 6.8: 'log_two_bits logn;' etc. are variable
//   declarations (a data type with no net-type keyword) -- 'logn', 'regn',
//   'bitn' are Variables, never Nets, and their declared type is the
//   corresponding typedef name.
//
// Not checked: preprocessing output (-writepp) and file-unit handling
// (-fileunit) from the .hlc command line -- neither affects the model.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LogicTypedefTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LogicTypedef.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTest() { return hldb::findByName<hldb::Module>("test", m_design->getAllModules()); }

  static const hldb::Typedef *findTypedef(std::string_view name) {
    const hldb::Module *const m = getTest();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>(name, m->getTypedefs());
  }

  static const hldb::Variable *findVar(std::string_view name) {
    const hldb::Module *const m = getTest();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, m->getVariables());
  }

  // Checks that 'ranges' is the single packed range '[1:0]'.
  static void expectRange1To0(const hldb::RangeCollection *ranges) {
    ASSERT_NE(ranges, nullptr) << "'[1:0]' packed dimension missing";
    ASSERT_EQ(ranges->size(), 1u);
    const hldb::Range *const r = ranges->at(0);
    const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->getDecompile(), std::string_view("1"));
    EXPECT_EQ(right->getDecompile(), std::string_view("0"));
  }

  // Checks that variable 'varName' is a Variable (not a Net) whose type
  // resolves to the TypedefTypespec named 'typeName'.
  static void expectVarOfTypedef(std::string_view varName, std::string_view typeName) {
    const hldb::Module *const m = getTest();
    ASSERT_NE(m, nullptr);
    const hldb::Variable *const v = findVar(varName);
    ASSERT_NE(v, nullptr) << "'" << varName << "' should be a Variable per Sec 6.8";
    EXPECT_EQ(hldb::findByName<hldb::Net>(varName, m->getNets()), nullptr);
    const hldb::RefTypespec *const rts = v->getTypespec();
    ASSERT_NE(rts, nullptr);
    ASSERT_NE(rts->getActual(), nullptr);
    ASSERT_EQ(rts->getActual()->getAnyType(), hldb::AnyType::TypedefTypespec);
    const hldb::TypedefTypespec *const tts = rts->getActual<hldb::TypedefTypespec>();
    EXPECT_EQ(tts->getName(), typeName);
    EXPECT_EQ(v->getValue(), nullptr) << "'" << varName << "' has no initializer";
  }
};

// ---------------------------------------------------------------------------
// Existence
// ---------------------------------------------------------------------------

TEST_F(LogicTypedefTest, ModuleTestExists) { EXPECT_NE(getTest(), nullptr) << "module 'test' not found"; }

TEST_F(LogicTypedefTest, ModuleOwnsThreeTypedefs) {
  const hldb::Module *const m = getTest();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getTypedefs(), nullptr);
  EXPECT_EQ(m->getTypedefs()->size(), 3u);
  EXPECT_NE(findTypedef("log_two_bits"), nullptr);
  EXPECT_NE(findTypedef("reg_two_bits"), nullptr);
  EXPECT_NE(findTypedef("bit_two_bits"), nullptr);
}

// ---------------------------------------------------------------------------
// typedef logic [1:0] log_two_bits -- Sec 6.18, 6.11.2
// ---------------------------------------------------------------------------

TEST_F(LogicTypedefTest, LogTwoBitsAliasesUnsignedLogic1To0) {
  const hldb::Typedef *const td = findTypedef("log_two_bits");
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(td->getAlias()->getActual(), nullptr);
  ASSERT_EQ(td->getAlias()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
  const hldb::LogicTypespec *const lts = td->getAlias()->getActual<hldb::LogicTypespec>();
  EXPECT_FALSE(lts->getSigned());
  expectRange1To0(lts->getRanges());
}

// ---------------------------------------------------------------------------
// typedef reg [1:0] reg_two_bits -- Sec 6.11.2: reg is the same type as logic
// ---------------------------------------------------------------------------

TEST_F(LogicTypedefTest, RegTwoBitsAliasesUnsignedLogic1To0) {
  const hldb::Typedef *const td = findTypedef("reg_two_bits");
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(td->getAlias()->getActual(), nullptr);
  ASSERT_EQ(td->getAlias()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec)
      << "'reg' and 'logic' denote the same type (Sec 6.11.2)";
  const hldb::LogicTypespec *const lts = td->getAlias()->getActual<hldb::LogicTypespec>();
  EXPECT_FALSE(lts->getSigned());
  expectRange1To0(lts->getRanges());
}

// ---------------------------------------------------------------------------
// typedef bit [1:0] bit_two_bits -- Sec 6.11.2: bit is 2-state
// ---------------------------------------------------------------------------

TEST_F(LogicTypedefTest, BitTwoBitsAliasesUnsignedBit1To0) {
  const hldb::Typedef *const td = findTypedef("bit_two_bits");
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(td->getAlias()->getActual(), nullptr);
  ASSERT_EQ(td->getAlias()->getActual()->getAnyType(), hldb::AnyType::BitTypespec);
  const hldb::BitTypespec *const bts = td->getAlias()->getActual<hldb::BitTypespec>();
  EXPECT_FALSE(bts->getSigned());
  expectRange1To0(bts->getRanges());
}

TEST_F(LogicTypedefTest, TypedefsAreNotNettypes) {
  for (const std::string_view name : {"log_two_bits", "reg_two_bits", "bit_two_bits"}) {
    const hldb::Typedef *const td = findTypedef(name);
    ASSERT_NE(td, nullptr) << name;
    EXPECT_FALSE(td->getIsNettype()) << name << " is a 'typedef', not a 'nettype' (Sec 6.6.7)";
  }
}

TEST_F(LogicTypedefTest, ModuleOwnsTypedefTypespecForEachName) {
  const hldb::Module *const m = getTest();
  ASSERT_NE(m, nullptr);
  for (const std::string_view name : {"log_two_bits", "reg_two_bits", "bit_two_bits"}) {
    const hldb::TypedefTypespec *const tts = hldb::findByName<hldb::TypedefTypespec>(name, m->getTypespecs());
    ASSERT_NE(tts, nullptr) << "TypedefTypespec '" << name << "' not found in module 'test'";
    EXPECT_EQ(tts->getTypedef(), findTypedef(name));
  }
}

// ---------------------------------------------------------------------------
// Variables of typedef'd types -- Sec 6.8
// ---------------------------------------------------------------------------

TEST_F(LogicTypedefTest, ModuleHasExactlyThreeVariablesAndNoNets) {
  const hldb::Module *const m = getTest();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 3u);
  EXPECT_TRUE(m->getNets() == nullptr || m->getNets()->empty());
}

TEST_F(LogicTypedefTest, LognIsVariableOfLogTwoBits) { expectVarOfTypedef("logn", "log_two_bits"); }

TEST_F(LogicTypedefTest, RegnIsVariableOfRegTwoBits) { expectVarOfTypedef("regn", "reg_two_bits"); }

TEST_F(LogicTypedefTest, BitnIsVariableOfBitTwoBits) { expectVarOfTypedef("bitn", "bit_two_bits"); }

// ---------------------------------------------------------------------------
// Diagnostics -- the source is legal
// ---------------------------------------------------------------------------

TEST_F(LogicTypedefTest, CompilerReportsZeroErrors) {
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
