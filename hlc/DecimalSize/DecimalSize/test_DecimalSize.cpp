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

// Tests for tests/DecimalSize/dut.sv:
//
//   module gem_gxl();
//     parameter [31:0] p_edma_queues              = 32'd1;
//     ...
//     parameter p_edma_axi_access_pipeline_bits   = 4'd4;
//     ...
//     parameter p_edma_jumbo_max_length           = 14'd10240;
//     ...
//     parameter p_num_type1_screeners = 8'd0;
//     ...
//   endmodule
//   module gem_ss (); ... endmodule
//   module gem_top (); ... endmodule
//
// This file exercises IEEE 1800-2023 Sec 5.7.1 sized decimal-radix integer
// literals ("<size>'d<digits>") used as module parameter default values,
// across a wide range of explicit sizes (4, 8, 14, 32 bits). Per Sec 5.7.1
// a sized decimal literal's Constant must carry:
//   - vpiConstType == vpiDecConst (the 'd base was written explicitly)
//   - vpiSize == the literal's explicit size
//   - getValue() == the digit string as written
//   - getDecompile() == the full "<size>'d<digits>" form
//
// Checked:
//   - modules gem_gxl, gem_ss, gem_top all exist
//   - gem_gxl declares (non-local) parameters p_edma_queues,
//     p_edma_axi_access_pipeline_bits, p_edma_jumbo_max_length,
//     p_num_type1_screeners
//   - each parameter's default ParamAssign RHS is a sized decimal Constant
//     with the expected vpiConstType/size/value/decompile

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

#include <string>
#include <string_view>

namespace hlc {

class DecimalSizeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DecimalSize.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Module *getGemGxl() { return getModule("gem_gxl"); }

  static const hldb::ParamAssign *findParamAssign(const hldb::Module *m, std::string_view name) {
    return (m == nullptr) ? nullptr : hldb::findByName(name, m->getParamAssigns());
  }
};

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

TEST_F(DecimalSizeTest, ModuleGemGxlExists) { EXPECT_NE(getModule("gem_gxl"), nullptr); }
TEST_F(DecimalSizeTest, ModuleGemSsExists) { EXPECT_NE(getModule("gem_ss"), nullptr); }
TEST_F(DecimalSizeTest, ModuleGemTopExists) { EXPECT_NE(getModule("gem_top"), nullptr); }

// ---------------------------------------------------------------------------
// parameter [31:0] p_edma_queues = 32'd1;
// ---------------------------------------------------------------------------

TEST_F(DecimalSizeTest, PEdmaQueuesIsModuleParameterNotLocalParam) {
  const hldb::Module *const gxl = getGemGxl();
  ASSERT_NE(gxl, nullptr);
  ASSERT_NE(gxl->getParameters(), nullptr);
  const hldb::Parameter *queues = nullptr;
  for (const hldb::Any *const p : *gxl->getParameters()) {
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
    if (param != nullptr && param->getName() == "p_edma_queues") {
      queues = param;
      break;
    }
  }
  ASSERT_NE(queues, nullptr) << "'parameter [31:0] p_edma_queues' not found on module 'gem_gxl'";
  EXPECT_FALSE(queues->getLocalParam());
}

TEST_F(DecimalSizeTest, PEdmaQueuesDefaultIs32BitDecimalOne) {
  const hldb::Module *const gxl = getGemGxl();
  ASSERT_NE(gxl, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(gxl, "p_edma_queues");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'p_edma_queues' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'p_edma_queues = 32'd1': default RHS must be a Constant";
  EXPECT_EQ(rhs->getConstType(), vpiDecConst) << "32'd1: constType should be decimal (vpiDecConst)";
  EXPECT_EQ(rhs->getSize(), 32);
  EXPECT_EQ(std::string(rhs->getValue()), "1");
  EXPECT_EQ(std::string(rhs->getDecompile()), "32'd1");
}

// ---------------------------------------------------------------------------
// parameter p_edma_axi_access_pipeline_bits = 4'd4;
// ---------------------------------------------------------------------------

TEST_F(DecimalSizeTest, PEdmaAxiAccessPipelineBitsDefaultIs4BitDecimalFour) {
  const hldb::Module *const gxl = getGemGxl();
  ASSERT_NE(gxl, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(gxl, "p_edma_axi_access_pipeline_bits");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'p_edma_axi_access_pipeline_bits' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiDecConst) << "4'd4: constType should be decimal (vpiDecConst)";
  EXPECT_EQ(rhs->getSize(), 4);
  EXPECT_EQ(std::string(rhs->getValue()), "4");
  EXPECT_EQ(std::string(rhs->getDecompile()), "4'd4");
}

// ---------------------------------------------------------------------------
// parameter p_edma_jumbo_max_length = 14'd10240;
// ---------------------------------------------------------------------------

TEST_F(DecimalSizeTest, PEdmaJumboMaxLengthDefaultIs14BitDecimal10240) {
  const hldb::Module *const gxl = getGemGxl();
  ASSERT_NE(gxl, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(gxl, "p_edma_jumbo_max_length");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'p_edma_jumbo_max_length' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiDecConst) << "14'd10240: constType should be decimal (vpiDecConst)";
  EXPECT_EQ(rhs->getSize(), 14);
  EXPECT_EQ(std::string(rhs->getValue()), "10240");
  EXPECT_EQ(std::string(rhs->getDecompile()), "14'd10240");
}

// ---------------------------------------------------------------------------
// parameter p_num_type1_screeners = 8'd0;
// ---------------------------------------------------------------------------

TEST_F(DecimalSizeTest, PNumType1ScreenersDefaultIs8BitDecimalZero) {
  const hldb::Module *const gxl = getGemGxl();
  ASSERT_NE(gxl, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(gxl, "p_num_type1_screeners");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'p_num_type1_screeners' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiDecConst) << "8'd0: constType should be decimal (vpiDecConst)";
  EXPECT_EQ(rhs->getSize(), 8);
  EXPECT_EQ(std::string(rhs->getValue()), "0");
  EXPECT_EQ(std::string(rhs->getDecompile()), "8'd0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
