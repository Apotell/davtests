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

// Tests for tests/FileLine/fileLine.sv:
//
//   module aaa;
//   initial begin
//       #1;
//       $display(`__FILE__);
//   end
//   endmodule
//
//   module bbb;
//   initial begin
//       #2;
//       if(`__LINE__ !== 21 || `__LINE__ !== 22) begin
//          $display("FAIL"); $finish;
//      end
//      $display("PASSED");
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 22.13 "Compiler directives": "`__FILE__ expands to the
// name of the current input file, in the form of a string literal" and
// "`__LINE__ expands to the current input line number, in the form of a
// simple decimal number." Both directives are macros substituted at
// preprocess time, before parsing -- so their effect is observable in the
// AST as the substituted literal, not as a surviving macro reference.
//
// This also exercises basic source-location ("file/line") tracking on
// every parsed Any node (Sec "getFile()"/"getStartLine()" accessors),
// which is the general mechanism the test name refers to.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/tf_call.h>

#include <string>
#include <string_view>

namespace hlc {

class FileLineTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FileLine.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getAaa() { return hldb::findByName<hldb::Module>("aaa", m_design->getAllModules()); }
  static const hldb::Module *getBbb() { return hldb::findByName<hldb::Module>("bbb", m_design->getAllModules()); }
};

TEST_F(FileLineTest, ModuleAaaExists) { ASSERT_NE(getAaa(), nullptr) << "module 'aaa' not found"; }

TEST_F(FileLineTest, ModuleBbbExists) { ASSERT_NE(getBbb(), nullptr) << "module 'bbb' not found"; }

// 'module aaa;' is declared on line 7 of fileLine.sv.
TEST_F(FileLineTest, AaaStartsAtDeclarationLine) {
  const hldb::Module *const aaa = getAaa();
  ASSERT_NE(aaa, nullptr);
  EXPECT_EQ(aaa->getStartLine(), 7u) << "'module aaa;' is declared on line 7";
}

// 'module bbb;' is declared on line 19 of fileLine.sv.
TEST_F(FileLineTest, BbbStartsAtDeclarationLine) {
  const hldb::Module *const bbb = getBbb();
  ASSERT_NE(bbb, nullptr);
  EXPECT_EQ(bbb->getStartLine(), 19u) << "'module bbb;' is declared on line 19";
}

// Both modules must record the same originating source file name.
TEST_F(FileLineTest, AaaAndBbbShareSourceFile) {
  const hldb::Module *const aaa = getAaa();
  const hldb::Module *const bbb = getBbb();
  ASSERT_NE(aaa, nullptr);
  ASSERT_NE(bbb, nullptr);
  const std::string aaaFile(aaa->getFile());
  const std::string bbbFile(bbb->getFile());
  EXPECT_NE(aaaFile.find("fileLine.sv"), std::string::npos) << "aaa's file must be fileLine.sv, got: " << aaaFile;
  EXPECT_NE(bbbFile.find("fileLine.sv"), std::string::npos) << "bbb's file must be fileLine.sv, got: " << bbbFile;
}

// Sec 22.13: '$display(`__FILE__)' -- after preprocessing, the argument is
// the string literal naming the current input file (fileLine.sv itself).
TEST_F(FileLineTest, DisplayCallArgumentIsFileNameLiteral) {
  const hldb::Module *const aaa = getAaa();
  ASSERT_NE(aaa, nullptr);
  ASSERT_NE(aaa->getSysTaskCalls(), nullptr) << "'$display(...)' must produce a system task call";
  const hldb::TFCall *display = nullptr;
  for (const hldb::TFCall *const call : *aaa->getSysTaskCalls()) {
    ASSERT_NE(call, nullptr);
    if (call->getName() == std::string_view{"$display"}) {
      display = call;
      break;
    }
  }
  ASSERT_NE(display, nullptr) << "'$display' call not found in module 'aaa'";
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 1u) << "'$display(`__FILE__)' has exactly one argument";
  ASSERT_NE(display->getArguments()->at(0), nullptr);
  const hldb::Constant *const literal = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(literal, nullptr) << "22.13: '`__FILE__' must expand to a string literal Constant";
  const std::string decompile(literal->getDecompile());
  EXPECT_NE(decompile.find("fileLine.sv"), std::string::npos)
      << "22.13: '`__FILE__' must expand to the current input file name, got: " << decompile;
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
