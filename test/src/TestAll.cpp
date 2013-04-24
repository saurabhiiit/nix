#include "TestCharon.hpp"
#include "TestFile.hpp"
#include "TestSection.hpp"
#include "TestProperty.hpp"

int main(int argc, char* argv[])
{
  CPPUNIT_TEST_SUITE_REGISTRATION(TestCharon);
  CPPUNIT_TEST_SUITE_REGISTRATION(TestFile);
  CPPUNIT_TEST_SUITE_REGISTRATION(TestSection);
  CPPUNIT_TEST_SUITE_REGISTRATION(TestProperty);

  CPPUNIT_NS::TestResult testresult;
  CPPUNIT_NS::TestResultCollector collectedresults;
  testresult.addListener(&collectedresults);

  CPPUNIT_NS::BriefTestProgressListener progress;
  testresult.addListener(&progress);

  CPPUNIT_NS::TestRunner testrunner;
  testrunner.addTest(CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest());
  testrunner.run(testresult);

  CPPUNIT_NS::CompilerOutputter compileroutputter(&collectedresults, std::cerr);
  compileroutputter.write();

  return !collectedresults.wasSuccessful();
}
