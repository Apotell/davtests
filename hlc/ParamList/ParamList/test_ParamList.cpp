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

// Validates the HLDB model built for tests/ParamList/dut.sv:
//
//   module r5p_soc_top #(
//
//     int unsigned GW = 32,
//
//     int unsigned XLEN = 32
//   )(
//
//   );
//   endmodule
//
// The point of the fixture is a parameter port list whose entries omit the
// 'parameter' keyword and start with a data type. 6.20.1: "The parameter
// keyword can be omitted in a parameter port list", and the grammar
// (parameter_port_declaration ::= ... | data_type list_of_param_assignments,
// A.1.3) makes 'int unsigned GW = 32' such an entry. The regression this file
// exists to catch is HLC rejecting the keyword-less form, dropping one of the
// two entries, or treating them as local parameters.
//
// What is checked, and why:
//   - the module's '()' declares one null port (37.14 detail 10,
//     "module M();")
//   - exactly 2 parameters, GW and XLEN, both overridable: they are in the
//     parameter port list and no localparam keyword precedes them (6.20.4)
//   - both are of type int unsigned: an int (6.11) whose signing is
//     unsigned (6.11.3)
//   - both default to the literal 32
//   - when elaborated, r5p_soc_top is the only top-level instance (23.3.1)
//     and both parameters hold 32
//   - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the elaborated values are checked only when the
// design is elaborated, on Design::getTopModules(); the declarations are
// asserted on the definition in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list '()' with no port at all, although 37.14 detail
// 10 names "module M();" as declaring a null port. ModuleHasOneNullPort is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {

class ParamListTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamList.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByDefName<hldb::Module>("r5p_soc_top", m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal. The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted.
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if ((ch != '_') && (ch != ' ')) text.push_back(ch);
    }
    int base = 10;
    const std::string::size_type tick = text.find('\'');
    if (tick != std::string::npos) {
      std::string::size_type pos = tick + 1;
      if ((pos < text.size()) && ((text[pos] == 's') || (text[pos] == 'S'))) ++pos;
      if (pos >= text.size()) return false;
      switch (text[pos]) {
        case 'h':
        case 'H': base = 16; break;
        case 'd':
        case 'D': base = 10; break;
        case 'o':
        case 'O': base = 8; break;
        case 'b':
        case 'B': base = 2; break;
        default: return false;
      }
      text = text.substr(pos + 1);
    }
    if (text.empty()) return false;
    const char *const last = text.data() + text.size();
    const std::from_chars_result res = std::from_chars(text.data(), last, *value, base);
    return (res.ec == std::errc()) && (res.ptr == last);
  }
};

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamListTest, ModuleHasOneNullPort) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr) << "module 'r5p_soc_top' not found";
  ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
}

TEST_F(ParamListTest, DeclaresTwoOverridableParameters) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 2u) << "6.20.1: both keyword-less entries are parameters";
  for (std::string_view name : {"GW", "XLEN"}) {
    const hldb::Parameter *const p = getParam(m, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.4: '" << name << "' is a parameter port, not a localparam";
  }
}

TEST_F(ParamListTest, ParametersAreIntUnsigned) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  for (std::string_view name : {"GW", "XLEN"}) {
    const hldb::Parameter *const p = getParam(m, name);
    ASSERT_NE(p, nullptr);
    ASSERT_NE(p->getTypespec(), nullptr) << "'" << name << "' is declared 'int unsigned'";
    const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << "6.11: '" << name << "' is an int";
    EXPECT_FALSE(it->getSigned()) << "6.11.3: 'unsigned' makes '" << name << "' unsigned";
  }
}

TEST_F(ParamListTest, ParametersDefaultToThirtyTwo) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  for (std::string_view name : {"GW", "XLEN"}) {
    const hldb::ParamAssign *const pa = getParamAssign(m, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "'";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->getDecompile(), "32") << name;
  }
}

TEST_F(ParamListTest, ElaboratedParametersHoldThirtyTwo) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: r5p_soc_top is the only module";
    const hldb::Module *const top = m_design->getTopModules()->at(0);
    EXPECT_EQ(top->getDefName(), "r5p_soc_top");
    for (std::string_view name : {"GW", "XLEN"}) {
      const hldb::ParamAssign *const pa = getParamAssign(top, name);
      ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "' in the elaborated module";
      const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
      ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
      uint64_t v = 0;
      ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
      EXPECT_EQ(v, 32u) << name;
    }
  }
}

TEST_F(ParamListTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "6.20.1: the parameter keyword can be omitted in a parameter port list";
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
