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

// Tests for tests/Delay2Param/dut.sv:
//   module iNToRawFN#(parameter intWidth = 1) (signedIn, sExp);
//       localparam expWidth = clog2(intWidth) + 1;
//       wire [(expWidth - 2):0] adjustedNormDist;
//   endmodule
//
//   module iNToRawFN3#(parameter intWidth = 1, parameter intWidth2 = 1) (signedIn, sExp);
//       localparam expWidth = clog2(intWidth) + 1;
//       localparam expWidth2 = clog2(intWidth2) + 1;
//       wire [(expWidth - 2):0] adjustedNormDist;
//       wire [(expWidth2 - 2):0] adjustedNormDist2;
//   endmodule
//
//   primitive BUFG ( O, I );
//      output O;
//      input I;
//      table
//         0 : 0 ;
//         1 : 1 ;
//      endtable
//   endprimitive
//
//   module iNToRecFN#( parameter intWidth = 64, parameter intWidth2 = 64 ) ( );
//    iNToRawFN#(intWidth) iNToRawFN(signedIn, sExp);
//    iNToRawFN3#(intWidth, intWidth2) iNToRawFN3(signedIn, sExp);
//    BUFG #5 bg(out, in);
//   endmodule // iNToRecFN
//
// IEEE 1800-2023 constructs under test:
//   - Sec 6.20.2/6.20.4: module parameters ("parameter intWidth = 1") and
//     localparams ("localparam expWidth = ...") are both Parameter
//     objects distinguished only by Parameter::getLocalParam().
//   - Sec 13.4.1: a plain identifier followed by a parenthesized argument
//     list ("clog2(intWidth)") is a function_subroutine_call. "clog2" is
//     NOT "$clog2" (Sec 20.8.1) and is never declared anywhere in this
//     file, so it cannot resolve to any function_decl -- the resulting
//     FuncCall's TFCall::getTaskFunc() must stay null.
//   - Sec 28.14 ("Assigning delays to primitives" / udp_instantiation):
//     "BUFG #5 bg(out, in);" instantiates the user-defined primitive
//     "BUFG" (declared via "primitive ... endprimitive") with a single
//     delay value "5", which per the udp_instance/delay2 grammar applies
//     uniformly to all output transitions -- modeled as a one-element
//     Primitive::getDelays() collection.
//
// Checked:
//   - modules "iNToRawFN", "iNToRawFN3", "iNToRecFN" all exist.
//   - "iNToRawFN"'s parameter "intWidth": not a localparam, default value
//     ParamAssign rhs Constant "1".
//   - "iNToRawFN"'s localparam "expWidth": IS a localparam, whose expr is
//     an Operation (vpiAddOp) of 2 operands: a FuncCall "clog2" (with one
//     RefObj "intWidth" argument, TaskFunc unresolved) and Constant "1".
//   - "iNToRawFN" declares net "adjustedNormDist", vpiWire, with a
//     RefTypespec -> LogicTypespec chain.
//   - "iNToRawFN3" carries the same shape twice, for "intWidth"/"expWidth"
//     and "intWidth2"/"expWidth2".
//   - "iNToRecFN"'s own parameters "intWidth"/"intWidth2" default to "64",
//     and are not localparams.
//   - "iNToRecFN" contains a Udp primitive instance "bg" with defName
//     "BUFG" and exactly one delay, Constant "5".
//
// NOT CHECKED (out of scope; no assertion locks in current tool output as
// "correct" -- see below for the one exception, called out explicitly):
//   - Elaborated parameter-override propagation from "iNToRecFN" into its
//     "iNToRawFN"/"iNToRawFN3" instances (e.g. whether the nested
//     instance's own "intWidth" Parameter reflects the overridden value
//     64): the exact API shape for reaching an elaborated sub-instance's
//     own parameter scope was not independently confirmed against a
//     passing precedent test in this suite, so asserting on it here would
//     risk guessing rather than reading. Left as a follow-up.
//   - The packed range "(expWidth - 2):0" on "adjustedNormDist" beyond
//     confirming the net exists with a Logic typespec -- the range's own
//     Operation (vpiSubOp) shape is not walked here.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/primitive.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/udp.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Delay2ParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Delay2Param.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Module *m, std::string_view name) {
    return (m == nullptr) ? nullptr : hldb::findByName<hldb::Parameter>(name, m->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(const hldb::Module *m, std::string_view name) {
    return (m == nullptr) ? nullptr : hldb::findByName(name, m->getParamAssigns());
  }

  // Checks 'localparam <lpName> = clog2(<paramName>) + 1;' for a module
  // that has both a parameter 'paramName' and a localparam 'lpName' of
  // that exact shape.
  static void ExpectClog2PlusOneLocalParam(const hldb::Module *m, std::string_view lpName,
                                            std::string_view paramName) {
    const hldb::ParamAssign *const lpa = getParamAssign(m, lpName);
    ASSERT_NE(lpa, nullptr) << "ParamAssign localparam '" << lpName << "' not found";

    const hldb::RefObj *const lro = lpa->getLhs<hldb::RefObj>();
    ASSERT_NE(lro, nullptr);
    EXPECT_EQ(lro->getName(), lpName);

    const hldb::Parameter *const lp = lro->getActual<hldb::Parameter>();
    ASSERT_NE(lp, nullptr) << "Paramter localparam '" << lpName << "' not found";
    EXPECT_TRUE(lp->getLocalParam()) << lpName << " must be a localparam (Sec 6.20.4)";

    ASSERT_NE(lpa->getRhs(), nullptr) << lpName << ": rhs must be present";
    const hldb::Operation *const add = lpa->getRhs<hldb::Operation>();
    ASSERT_NE(add, nullptr) << lpName << ": 'clog2(...) + 1' must be an Operation";
    EXPECT_EQ(add->getOpType(), vpiAddOp);
    ASSERT_NE(add->getOperands(), nullptr);
    ASSERT_EQ(add->getOperands()->size(), 2u);

    const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(add->getOperands()->at(0));
    ASSERT_NE(call, nullptr) << lpName << ": first operand must be the 'clog2(...)' FuncCall";
    EXPECT_EQ(call->getName(), "clog2");
    EXPECT_EQ(call->getTaskFunc(), nullptr)
        << "'clog2' is never declared in this file (it is not '$clog2') -- must not resolve (Sec 13.4.1)";
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr) << lpName << ": clog2's argument must be a RefObj";
    EXPECT_EQ(arg->getName(), paramName);

    const hldb::Constant *const one = any_cast<hldb::Constant>(add->getOperands()->at(1));
    ASSERT_NE(one, nullptr) << lpName << ": second operand must be the Constant '1'";
    EXPECT_EQ(one->getDecompile(), "1");
  }

  static void ExpectWireWithLogicTypespec(const hldb::Module *m, std::string_view netName) {
    ASSERT_NE(m, nullptr);
    ASSERT_NE(m->getNets(), nullptr);
    const hldb::Net *const net = hldb::findByName<hldb::Net>(netName, m->getNets());
    ASSERT_NE(net, nullptr) << "net '" << netName << "' not found";
    EXPECT_EQ(net->getNetType(), vpiWire);
    ASSERT_NE(net->getTypespec(), nullptr) << netName << ": typespec must be present";
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr)
        << netName << ": typespec must resolve to LogicTypespec";
  }
};

// --- module existence ----

TEST_F(Delay2ParamTest, ModulesExist) {
  EXPECT_NE(getModule("iNToRawFN"), nullptr) << "module 'iNToRawFN' not found";
  EXPECT_NE(getModule("iNToRawFN3"), nullptr) << "module 'iNToRawFN3' not found";
  EXPECT_NE(getModule("iNToRecFN"), nullptr) << "module 'iNToRecFN' not found";
}

// --- iNToRawFN: parameter / localparam / net ----

TEST_F(Delay2ParamTest, INToRawFN_IntWidthParamDefaultsToOne) {
  const hldb::Module *const m = getModule("iNToRawFN");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = getParam(m, "intWidth");
  ASSERT_NE(p, nullptr) << "'intWidth' not found among iNToRawFN's parameters";
  EXPECT_FALSE(p->getLocalParam()) << "Sec 6.20.2: 'parameter intWidth' is a module parameter, not a localparam";

  const hldb::ParamAssign *const pa = getParamAssign(m, "intWidth");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'intWidth' not found";
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'parameter intWidth = 1': RHS must be a Constant";
  EXPECT_EQ(c->getDecompile(), "1");
}

TEST_F(Delay2ParamTest, INToRawFN_ExpWidthLocalParamIsClog2PlusOne) {
  const hldb::Module *const m = getModule("iNToRawFN");
  ASSERT_NE(m, nullptr);
  ExpectClog2PlusOneLocalParam(m, "expWidth", "intWidth");
}

TEST_F(Delay2ParamTest, INToRawFN_AdjustedNormDistIsWire) {
  ExpectWireWithLogicTypespec(getModule("iNToRawFN"), "adjustedNormDist");
}

// --- iNToRawFN3: two independent parameter/localparam/net triples ----

TEST_F(Delay2ParamTest, INToRawFN3_TwoParamsDefaultToOne) {
  const hldb::Module *const m = getModule("iNToRawFN3");
  ASSERT_NE(m, nullptr);

  const hldb::Parameter *const w1 = getParam(m, "intWidth");
  ASSERT_NE(w1, nullptr);
  EXPECT_FALSE(w1->getLocalParam());
  const hldb::ParamAssign *const pa1 = getParamAssign(m, "intWidth");
  ASSERT_NE(pa1, nullptr);
  ASSERT_NE(pa1->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa1->getRhs<hldb::Constant>()->getDecompile(), "1");

  const hldb::Parameter *const w2 = getParam(m, "intWidth2");
  ASSERT_NE(w2, nullptr);
  EXPECT_FALSE(w2->getLocalParam());
  const hldb::ParamAssign *const pa2 = getParamAssign(m, "intWidth2");
  ASSERT_NE(pa2, nullptr);
  ASSERT_NE(pa2->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa2->getRhs<hldb::Constant>()->getDecompile(), "1");
}

TEST_F(Delay2ParamTest, INToRawFN3_BothLocalParamsAreClog2PlusOne) {
  const hldb::Module *const m = getModule("iNToRawFN3");
  ASSERT_NE(m, nullptr);
  ExpectClog2PlusOneLocalParam(m, "expWidth", "intWidth");
  ExpectClog2PlusOneLocalParam(m, "expWidth2", "intWidth2");
}

TEST_F(Delay2ParamTest, INToRawFN3_BothNetsAreWires) {
  const hldb::Module *const m = getModule("iNToRawFN3");
  ExpectWireWithLogicTypespec(m, "adjustedNormDist");
  ExpectWireWithLogicTypespec(m, "adjustedNormDist2");
}

// --- iNToRecFN: own parameters, plus the BUFG primitive delay ----

TEST_F(Delay2ParamTest, INToRecFN_ParamsDefaultToSixtyFour) {
  const hldb::Module *const m = getModule("iNToRecFN");
  ASSERT_NE(m, nullptr);

  const hldb::Parameter *const w1 = getParam(m, "intWidth");
  ASSERT_NE(w1, nullptr);
  EXPECT_FALSE(w1->getLocalParam());
  const hldb::ParamAssign *const pa1 = getParamAssign(m, "intWidth");
  ASSERT_NE(pa1, nullptr);
  ASSERT_NE(pa1->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa1->getRhs<hldb::Constant>()->getDecompile(), "64");

  const hldb::Parameter *const w2 = getParam(m, "intWidth2");
  ASSERT_NE(w2, nullptr);
  EXPECT_FALSE(w2->getLocalParam());
  const hldb::ParamAssign *const pa2 = getParamAssign(m, "intWidth2");
  ASSERT_NE(pa2, nullptr);
  ASSERT_NE(pa2->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa2->getRhs<hldb::Constant>()->getDecompile(), "64");
}

TEST_F(Delay2ParamTest, INToRecFN_BufgPrimitiveHasFiveUnitDelay) {
  const hldb::Module *const m = getModule("iNToRecFN");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPrimitives(), nullptr) << "iNToRecFN has no primitives";

  const hldb::Udp *bg = nullptr;
  for (const hldb::Primitive *const prim : *m->getPrimitives()) {
    if (const hldb::Udp *const udp = any_cast<const hldb::Udp *>(prim)) {
      if (udp->getName() == "bg") {
        bg = udp;
        break;
      }
    }
  }
  ASSERT_NE(bg, nullptr) << "'BUFG #5 bg(out, in);' must produce a Udp primitive instance named 'bg'";
  EXPECT_EQ(bg->getDefName(), "BUFG");

  ASSERT_NE(bg->getDelays(), nullptr) << "'#5' delay must be captured (Sec 28.14)";
  ASSERT_EQ(bg->getDelays()->size(), 1u) << "a single delay value applies uniformly to all transitions";
  const hldb::Constant *const delay = any_cast<hldb::Constant>(bg->getDelays()->at(0));
  ASSERT_NE(delay, nullptr);
  EXPECT_EQ(delay->getDecompile(), "5");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
