/**
 * Copyright (c) 2017-2018. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <wrench-dev.h>
#include <wrench/simgrid_S4U_util/S4U_Mailbox.h>
#include <wrench/simulation/SimulationMessage.h>
#include "services/compute/standard_job_executor/StandardJobExecutorMessage.h"
#include <gtest/gtest.h>
#include <wrench/services/compute/batch/BatchService.h>
#include <wrench/services/compute/batch/BatchServiceMessage.h>
#include "wrench/workflow/job/PilotJob.h"

#include "../include/TestWithFork.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(failure_traces_test, "Log category for FailureTracesTest");


class FailureTracesTest : public ::testing::Test {

public:
    wrench::StorageService *storage_service1 = nullptr;
    wrench::StorageService *storage_service2 = nullptr;
    wrench::ComputeService *compute_service = nullptr;
    wrench::ComputeService *compute_service1 = nullptr;
    wrench::ComputeService *compute_service2 = nullptr;
    wrench::Simulation *simulation;

    void do_CheckHowSimgridRespondsToFailureTraces_test();


protected:
    FailureTracesTest() {

      // Create the simplest workflow
      workflow = std::unique_ptr<wrench::Workflow>(new wrench::Workflow());

      // Create a four-host 10-core platform file
      std::string xml = "<?xml version='1.0'?>"
              "<!DOCTYPE platform SYSTEM \"http://simgrid.gforge.inria.fr/simgrid/simgrid.dtd\">"
              "<platform version=\"4.1\"> "
              "   <zone id=\"AS0\" routing=\"Full\"> "
              "       <host id=\"Host1\" speed=\"1f\" core=\"10\"/> "
              "       <host id=\"Host2\" speed=\"1f\" core=\"10\"/> "
              "       <host id=\"Host3\" speed=\"1f\" core=\"10\"/> "
              "       <host id=\"Host4\" speed=\"1f\" core=\"10\"/> "
              "       <link id=\"1\" bandwidth=\"50000GBps\" latency=\"0us\"/>"
              "       <link id=\"2\" bandwidth=\"0.0001MBps\" latency=\"1000000us\"/>"
              "       <link id=\"3\" bandwidth=\"0.0001MBps\" latency=\"1000000us\"/>"
              "       <route src=\"Host3\" dst=\"Host1\"> <link_ctn id=\"1\"/> </route>"
              "       <route src=\"Host3\" dst=\"Host4\"> <link_ctn id=\"1\"/> </route>"
              "       <route src=\"Host4\" dst=\"Host1\"> <link_ctn id=\"1\"/> </route>"
              "       <route src=\"Host1\" dst=\"Host2\"> <link_ctn id=\"1\""
              "/> </route>"
              "   </zone> "
              "</platform>";
      FILE *platform_file = fopen(platform_file_path.c_str(), "w");
      fprintf(platform_file, "%s", xml.c_str());
      fclose(platform_file);

    }

    std::string platform_file_path = "/tmp/platform.xml";
    std::unique_ptr<wrench::Workflow> workflow;

};

/**********************************************************************/
/**             CheckHowSimgridRespondsToFailureTraces               **/
/**********************************************************************/

class FailureTracesTestTestWMS : public wrench::WMS {

public:
    FailureTracesTest(FailureTracesTest *test,
                              const std::set<wrench::ComputeService *> &compute_services,
                              std::string hostname) :
            wrench::WMS(nullptr, nullptr, compute_services, {}, {}, nullptr, hostname,
                        "test") {
      this->test = test;
    }

private:

    FailureTracesTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a sequential task that lasts one min and requires 1 cores
        wrench::WorkflowTask *task = this->getWorkflow()->addTask("task", 60, 1, 1, 1.0, 0);
        task->addInputFile(this->getWorkflow()->getFileByID("input_file"));
        task->addOutputFile(this->getWorkflow()->getFileByID("output_file"));

        // Create a StandardJob with some pre-copies
        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {},
                {std::make_tuple(this->getWorkflow()->getFileByID("input_file"), this->test->storage_service1,
                                 wrench::ComputeService::SCRATCH)},
                {},
                {});

        // Submit the job for execution
        job_manager->submitJob(job, this->test->compute_service);

        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = this->getWorkflow()->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
            //sleep to make sure that the files are deleted
            wrench::S4U_Simulation::sleep(100);
            double free_space_size = this->test->compute_service->getFreeScratchSpaceSize();
            if (free_space_size < this->test->compute_service->getTotalScratchSpaceSize()) {
              throw std::runtime_error(
                      "File was not deleted from scratch");
            }
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
      }

      return 0;
    }
};

TEST_F(FailureTracesTest, CheckHowSimgridRespondsToFailureTracesTest) {
  DO_TEST_WITH_FORK(do_CheckHowSimgridRespondsToFailureTraces_test);
}


void ScratchSpaceTest::do_CheckHowSimgridRespondsToFailureTraces_test() {


  // Create and initialize a simulation
  auto simulation = new wrench::Simulation();
  int argc = 1;
  auto argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("scratch_space_test");

  ASSERT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  ASSERT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  ASSERT_NO_THROW(storage_service1 = simulation->add(
          new wrench::SimpleStorageService(hostname, 1000000.0)));

  // Create a Storage Service
  ASSERT_NO_THROW(storage_service2 = simulation->add(
          new wrench::SimpleStorageService(hostname, 1000000.0)));


  // Create a Compute Service
  ASSERT_NO_THROW(compute_service = simulation->add(
          new wrench::MultihostMulticoreComputeService(hostname,
                                                       {std::make_tuple(hostname, wrench::ComputeService::ALL_CORES,
                                                                        wrench::ComputeService::ALL_RAM)},
                                                       1000000.0, {})));

  simulation->add(new wrench::FileRegistryService(hostname));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  ASSERT_NO_THROW(wms = simulation->add(
          new FailureTracesTestTestWMS(
                  this, {compute_service}, hostname)));

  ASSERT_NO_THROW(wms->addWorkflow(std::move(workflow.get())));


  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  ASSERT_NO_THROW(simulation->stageFile(input_file, storage_service1));

  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  ASSERT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}
