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

// Tests for dut.sv (tags: LargeHexString)
//   module bottom ();
//      parameter INIT_i = 36864'h0000...0000;          // 8641 hex digits
//   endmodule
//
//   module top();
//     bottom  #(.INIT_i(36864'h0000...5AEC54B0...21EDDC7C63)) u1 ();
//                                                    // 9216 hex digits
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - both modules exist
//   - bottom: INIT_i is a value parameter (6.20.2) declared in the module
//     body of a module WITHOUT a parameter_port_list, so it is a regular
//     (overridable) parameter, not a localparam (6.20.1)
//   - its default is a sized hexadecimal literal (5.7.1): vpiHexConst with
//     size exactly 36864 bits (the size constant), and the full digit
//     string is preserved (8641 digits -> no truncation; the missing upper
//     digits are zero-padded per 5.7.1)
//   - top: u1 is an instance of bottom with a by-name parameter value
//     assignment (23.10.2.2) .INIT_i(...) whose value is a 36864-bit hex
//     literal with all 9216 digits preserved (it ends in "...21EDDC7C63")
//
// What is NOT checked and why:
//   - INIT_i's resolved data type: 6.20.2 says an untyped parameter takes
//     the type of its final value "after any value overrides have been
//     applied", which needs elaboration; this .hlc does not elaborate.
//   - the HLDB encoding of Constant::getValue(): not standard-defined.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class LargeHexStringTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LargeHexString.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::ParamAssign *getOverride() {
    const hldb::Module *const top = getModule("top");
    if ((top == nullptr) || (top->getRefInstances() == nullptr) || (top->getRefInstances()->size() != 1)) {
      return nullptr;
    }
    const hldb::RefInstance *const u1 = top->getRefInstances()->at(0);
    if (u1->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = u1->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr) return nullptr;
    return hldb::findByName<hldb::ParamAssign>("INIT_i", mt->getParamAssigns());
  }

  static bool startsWith(std::string_view s, std::string_view p) { return s.substr(0, p.size()) == p; }
  static bool endsWith(std::string_view s, std::string_view p) {
    return (s.size() >= p.size()) && (s.substr(s.size() - p.size()) == p);
  }
};

TEST_F(LargeHexStringTest, BothModulesExist) {
  EXPECT_NE(getModule("bottom"), nullptr);
  EXPECT_NE(getModule("top"), nullptr);
}

TEST_F(LargeHexStringTest, BottomInitIsNonLocalParameter) {
  const hldb::Module *const bottom = getModule("bottom");
  ASSERT_NE(bottom, nullptr);
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("INIT_i", bottom->getParameters());
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: no parameter_port_list, so body 'parameter' is overridable";
}

TEST_F(LargeHexStringTest, BottomInitDefaultIs36864BitHexLiteral) {
  const hldb::Module *const bottom = getModule("bottom");
  ASSERT_NE(bottom, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("INIT_i", bottom->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiHexConst);
  EXPECT_EQ(c->getSize(), 36864) << "5.7.1: the size constant gives the exact width";
  const std::string_view text = c->getDecompile();
  EXPECT_TRUE(startsWith(text, "36864'h0000"));
  EXPECT_EQ(text.size(), 7u + 8641u) << "all 8641 hex digits are preserved";
}

TEST_F(LargeHexStringTest, TopInstanceU1OfBottom) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const u1 = top->getRefInstances()->at(0);
  ASSERT_NE(u1, nullptr);
  EXPECT_EQ(u1->getName(), "u1");
  ASSERT_NE(u1->getTypespec(), nullptr);
  EXPECT_EQ(u1->getTypespec()->getName(), "bottom");
}

TEST_F(LargeHexStringTest, TopOverridesInitByName) {
  const hldb::ParamAssign *const pa = getOverride();
  ASSERT_NE(pa, nullptr) << "23.10.2.2: #(.INIT_i(...)) parameter value assignment";
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());
  ASSERT_NE(pa->getLhs(), nullptr);
  EXPECT_EQ(pa->getLhs()->getName(), "INIT_i");
}

TEST_F(LargeHexStringTest, TopOverrideValueIs36864BitHexLiteral) {
  const hldb::ParamAssign *const pa = getOverride();
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiHexConst);
  EXPECT_EQ(c->getSize(), 36864);
  const std::string_view text = c->getDecompile();
  EXPECT_TRUE(startsWith(text, "36864'h0000"));
  EXPECT_TRUE(endsWith(text, "30315BC6BF21EDDC7C63")) << "the low-order digits must be preserved";
  EXPECT_EQ(text.size(), 7u + 9216u) << "all 9216 hex digits are preserved";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
