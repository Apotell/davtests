#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/module.h>

namespace SURELOG {
class Timescale : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-parse", "-nobuiltin", "dut.sv"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(Timescale, default) {
  const uhdm::SourceFile *const s = uhdm::findByName<uhdm::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(s, nullptr) << "SourceFile s is null";

  const uhdm::Module *const m1 = uhdm::findByName<uhdm::Module>("work@m1", m_design->getAllModules());
  ASSERT_NE(m1, nullptr) << "Module m1 is null";

  const uhdm::Module *const m11 = uhdm::findByName<uhdm::Module>("work@m11", m1->getModules());
  ASSERT_NE(m11, nullptr) << "Module m11 is null";

  const uhdm::Module *const m12 = uhdm::findByName<uhdm::Module>("work@m12", m1->getModules());
  ASSERT_NE(m12, nullptr) << "Module m12 is null";

  const uhdm::Module *const m2 = uhdm::findByName<uhdm::Module>("work@m2", m_design->getAllModules());
  ASSERT_NE(m2, nullptr) << "Module m2 is null";

  ASSERT_EQ(s->getTimeUnit(), -9);
  ASSERT_EQ(s->getTimePrecision(), -12);

  ASSERT_EQ(m1->getTimeUnit(), -8);
  ASSERT_EQ(m1->getTimePrecision(), -12);

  ASSERT_EQ(m11->getTimeUnit(), -8);
  ASSERT_EQ(m11->getTimePrecision(), -11);

  ASSERT_EQ(m12->getTimeUnit(), 0);
  ASSERT_EQ(m12->getTimePrecision(), -12);

  ASSERT_EQ(m2->getTimeUnit(), -9);
  ASSERT_EQ(m2->getTimePrecision(), -12);
}
}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
