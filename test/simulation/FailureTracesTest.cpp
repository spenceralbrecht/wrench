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

      std::string trace_file_content = "PERIODICITY 10\n"
              " 0 1\n"
              " 5 0";

      FILE *trace_file = fopen(trace_file_path.c_str(), "w");
      fprintf(trace_file, "%s", trace_file_content.c_str());
      fclose(trace_file);

      // Create a four-host 10-core platform file
      std::string xml = "<?xml version='1.0'?>"
              "<!DOCTYPE platform SYSTEM \"http://simgrid.gforge.inria.fr/simgrid/simgrid.dtd\">"
              "<platform version=\"4.1\"> "
              "   <zone id=\"AS0\" routing=\"Full\"> "
              "       <host id=\"Host1\" speed=\"1f\" state_file=\""+trace_file_name+"\"  core=\"1\"/> "
              "       <host id=\"Host2\" speed=\"1f\" core=\"1\"/> "
              "       <link id=\"1\" bandwidth=\"50000GBps\" latency=\"0us\"/>"
              "       <route src=\"Host1\" dst=\"Host2\"> <link_ctn id=\"1\"/> </route>"
              "   </zone> "
              "</platform>";
      FILE *platform_file = fopen(platform_file_path.c_str(), "w");
      fprintf(platform_file, "%s", xml.c_str());
      fclose(platform_file);

    }

    std::string platform_file_path = "/tmp/platform.xml";
    std::string trace_file_name = "host.trace";
    std::string trace_file_path = "/tmp/"+trace_file_name;
    std::unique_ptr<wrench::Workflow> workflow;

};

/**********************************************************************/
/**             CheckHowSimgridRespondsToFailureTraces               **/
/**********************************************************************/

class FailureTracesTestTestWMS : public wrench::WMS {

public:
    FailureTracesTestTestWMS(FailureTracesTest *test,
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
        // Create a sequential task that lasts one second and requires 1 cores
        wrench::WorkflowTask *task = this->getWorkflow()->addTask("task", 1, 1, 1, 1.0, 0);
        // Create a sequential task that lasts one second and requires 1 cores
        wrench::WorkflowTask *task1 = this->getWorkflow()->addTask("task1", 1, 1, 1, 1.0, 0);
        // Create a sequential task that lasts one second and requires 1 cores
        wrench::WorkflowTask *task2 = this->getWorkflow()->addTask("task2", 1, 1, 1, 1.0, 0);

        // Create a StandardJob with no file operations
        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {},
                {},
                {},
                {});

        // Create a StandardJob with no file operations
        wrench::StandardJob *job1 = job_manager->createStandardJob(
                {task1},
                {},
                {},
                {},
                {});

        // Create a StandardJob with no file operations
        wrench::StandardJob *job2 = job_manager->createStandardJob(
                {task2},
                {},
                {},
                {},
                {});

        // Submit the job for execution, I should get a job completion event
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

            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }

        //Until now, we just submitted 1 job that took 1 second. But in our platform file, our one of the host
        //,where MHMCCS is running, will turn off in 5th second.

        // So now, let's sleep for 8 seconds and submit a new job (that lasts for 1 second)
        // In this case, after 5th second up until 10th second, the host works at a speed of 0 flops/seconds, so we
        // should get an exception about host failed
        wrench::S4U_Simulation::sleep(8);
        // Submit the job for execution, I should get some kind of error
        try {
          WRENCH_INFO("Trying to submit a job when the host is down");
          job_manager->submitJob(job1, this->test->compute_service);
        } catch (wrench::WorkflowExecutionException &e) {
          WRENCH_INFO("Expectedly we need to get this exception because the host was down: %s ", e.getCause()->toString().c_str());
        }

        //Now sleep for 8 more seconds, so total seconds until now is 17, so probably at this time, when we wake up
        //everything will restart and our job will be submitted to the re-started compute service and so we can wait for job completion
        wrench::S4U_Simulation::sleep(8);
        // Wait for a workflow execution event
        try {
          WRENCH_INFO("Waiting for the job done message");
          event = this->getWorkflow()->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
            WRENCH_INFO("Received a job done message");
            break;
          }
          default: {
            std::cerr << "This is the type of event that I found " << std::to_string((int) (event->type)) << "\n";
          }
        }

      }

      return 0;
    }
};

TEST_F(FailureTracesTest, CheckHowSimgridRespondsToFailureTracesTest) {
  DO_TEST_WITH_FORK(do_CheckHowSimgridRespondsToFailureTraces_test);
}


void FailureTracesTest::do_CheckHowSimgridRespondsToFailureTraces_test() {


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

  std::string wms_hostname = simulation->getHostnameList()[1];

  // Create a Storage Service
  ASSERT_NO_THROW(storage_service1 = simulation->add(
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
                  this, {compute_service}, wms_hostname)));

  ASSERT_NO_THROW(wms->addWorkflow(std::move(workflow.get())));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  ASSERT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}
