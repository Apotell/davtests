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

// Validates the UHDM graph produced for tests/NetsAndVariables/DefaultNettypeIllegalUsage.sv.
// That file's only live code is a small baseline module; every illegal
// `default_nettype usage it catalogs (bad argument type, wrong keyword case,
// missing/unrecognized argument, and implicit-net-under-none at a module
// instance port connection or a primitive gate terminal) is documented only
// as a comment, since compiling any of them would break the file. Each is
// given its own GTEST_SKIP placeholder below rather than a guessed
// diagnostic assertion.
//
// Checked:
//   - default_nettype_illegal_usage_test exists, and its one explicitly
//     declared variable (explicit_var) resolves to a LogicTypespec

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class DefaultNettypeIllegalUsageTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DefaultNettypeIllegalUsage.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("work@default_nettype_illegal_usage_test", m_design->getAllModules());
  }
};

TEST_F(DefaultNettypeIllegalUsageTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(DefaultNettypeIllegalUsageTest, ExplicitVarIsLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("explicit_var", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(DefaultNettypeIllegalUsageTest, BadArgumentTypeIsIllegalNotCompiled) {
  GTEST_SKIP() << "`default_nettype logic/reg/int/bit/var is illegal (the directive requires a net type keyword "
                  "or 'none', not a data type); documented as a comment rather than compiled.";
}

TEST_F(DefaultNettypeIllegalUsageTest, WrongKeywordCaseIsIllegalNotCompiled) {
  GTEST_SKIP() << "`default_nettype Wire/WIRE/None is illegal (SystemVerilog keywords are case-sensitive, so "
                  "these are unrecognized tokens, not the keywords 'wire'/'none'); documented as a comment "
                  "rather than compiled.";
}

TEST_F(DefaultNettypeIllegalUsageTest, MissingOrUnrecognizedArgumentIsIllegalNotCompiled) {
  GTEST_SKIP() << "`default_nettype with no argument, or with an identifier that is not a net type keyword or "
                  "'none' (e.g. 'mytype'), is illegal; documented as a comment rather than compiled.";
}

TEST_F(DefaultNettypeIllegalUsageTest, ImplicitNetAtInstancePortUnderNoneIsIllegalNotCompiled) {
  GTEST_SKIP() << "Under `default_nettype none, an undeclared identifier used as an unconnected module instance "
                  "port connection is illegal (no default net type to fall back on); documented as a comment "
                  "rather than compiled.";
}

TEST_F(DefaultNettypeIllegalUsageTest, ImplicitNetAtGateTerminalUnderNoneIsIllegalNotCompiled) {
  GTEST_SKIP() << "Under `default_nettype none, an undeclared identifier used as a primitive gate terminal is "
                  "illegal for the same reason as the continuous-assignment and instance-port cases; documented "
                  "as a comment rather than compiled.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
