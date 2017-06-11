/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MulticoreComputeService.h"

#include <simulation/Simulation.h>
#include <logging/TerminalOutput.h>
#include <simgrid_S4U_util/S4U_Mailbox.h>
#include <simulation/SimulationMessage.h>
#include <services/storage_services/StorageService.h>
#include <services/ServiceMessage.h>
#include <services/compute_services/ComputeServiceMessage.h>
#include "exceptions/WorkflowExecutionException.h"
#include "workflow_job/PilotJob.h"
#include "MulticoreComputeServiceMessage.h"
#include "WorkUnit.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(multicore_job_executor, "Log category for Multicore Job Executor");

namespace wrench {

    /**
     * @brief Asynchronously submit a standard job to the compute service
     *
     * @param job: a pointer a StandardJob object
     *
     * @throw WorkflowExecutionException
     * @throw std::runtime_error
     *
     */
    void MulticoreComputeService::submitStandardJob(StandardJob *job) {

      if (this->state == Service::DOWN) {
        throw WorkflowExecutionException(new ServiceIsDown(this));
      }

      std::string answer_mailbox = S4U_Mailbox::getPrivateMailboxName();

      //  send a "run a task" message to the daemon's mailbox
      try {
        S4U_Mailbox::putMessage(this->mailbox_name,
                                new ComputeServiceSubmitStandardJobRequestMessage(answer_mailbox, job,
                                                                                  this->getPropertyValueAsDouble(
                                                                                          MulticoreComputeServiceProperty::SUBMIT_PILOT_JOB_REQUEST_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::submitStandardJob(): Unknown exception: " + std::string(e.what()));
        }
      }

      // Get the answer
      std::unique_ptr<SimulationMessage> message = nullptr;
      try {
        message = S4U_Mailbox::getMessage(answer_mailbox);
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::submitStandardJob(): Unknown exception: " + std::string(e.what()));
        }
      }

      if (ComputeServiceSubmitStandardJobAnswerMessage *msg = dynamic_cast<ComputeServiceSubmitStandardJobAnswerMessage *>(message.get())) {
        // If no success, throw an exception
        if (not msg->success) {
          throw WorkflowExecutionException(msg->failure_cause);
        }
      } else {
        throw std::runtime_error(
                "MulticoreComputeService::submitStandardJob(): Received an unexpected [" + message->getName() +
                "] message!");
      }

    };

    /**
     * @brief Asynchronously submit a pilot job to the compute service
     *
     * @param task: a pointer the PilotJob object
     * @param callback_mailbox: the name of a mailbox to which a "pilot job started" callback will be sent
     *
     * @throw WorkflowExecutionException
     * @throw std::runtime_error
     */
    void MulticoreComputeService::submitPilotJob(PilotJob *job) {

      if (this->state == Service::DOWN) {
        throw WorkflowExecutionException(new ServiceIsDown(this));
      }

      std::string answer_mailbox = S4U_Mailbox::getPrivateMailboxName();

      // Send a "run a pilot job" message to the daemon's mailbox
      try {
        S4U_Mailbox::putMessage(this->mailbox_name,
                                new ComputeServiceSubmitPilotJobRequestMessage(answer_mailbox, job,
                                                                               this->getPropertyValueAsDouble(
                                                                                       MulticoreComputeServiceProperty::SUBMIT_PILOT_JOB_REQUEST_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::submitPilotJob(): Unknown exception: " + std::string(e.what()));
        }
      }

      // Get the answer
      std::unique_ptr<SimulationMessage> message = nullptr;

      try {
        message = S4U_Mailbox::getMessage(answer_mailbox);
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::submitPilotJob(): Unknown exception: " + std::string(e.what()));
        }
      }

      if (ComputeServiceSubmitPilotJobAnswerMessage *msg = dynamic_cast<ComputeServiceSubmitPilotJobAnswerMessage *>(message.get())) {
        // If no success, throw an exception
        if (not msg->success) {
          throw WorkflowExecutionException(msg->failure_cause);
        }

      } else {
        throw std::runtime_error(
                "MulticoreComputeService::submitPilotJob(): Received an unexpected [" + message->getName() +
                "] message!");
      }
    };

    /**
     * @brief Synchronously ask the service how many cores it has
     *
     * @return the number of cores
     *
     * @throw WorkflowExecutionException
     * @throw std::runtime_error
     */
    unsigned long MulticoreComputeService::getNumCores() {

      if (this->state == Service::DOWN) {
        throw WorkflowExecutionException(new ServiceIsDown(this));
      }

      // send a "num cores" message to the daemon's mailbox
      std::string answer_mailbox = S4U_Mailbox::getPrivateMailboxName();

      try {
        S4U_Mailbox::putMessage(this->mailbox_name,
                                new MulticoreComputeServiceNumCoresRequestMessage(answer_mailbox,
                                                                                  this->getPropertyValueAsDouble(
                                                                                          MulticoreComputeServiceProperty::NUM_CORES_REQUEST_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::getNumCores(): Unknown exception: " + std::string(e.what()));
        }
      }

      // getMessage the reply
      std::unique_ptr<SimulationMessage> message = nullptr;

      try {
        message = S4U_Mailbox::getMessage(answer_mailbox);
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::getNumCores(): Unknown exception: " + std::string(e.what()));
        }
      }

      if (MulticoreComputeServiceNumCoresAnswerMessage *msg = dynamic_cast<MulticoreComputeServiceNumCoresAnswerMessage *>(message.get())) {
        return msg->num_cores;
      } else {
        throw std::runtime_error(
                "MulticoreComputeService::getNumCores(): unexpected [" + msg->getName() + "] message");
      }
    }

    /**
     * @brief Synchronously ask the service how many idle cores it has
     *
     * @return the number of currently idle cores
     *
     * @throw WorkflowExecutionException
     * @throw std::runtime_error
     */
    unsigned long MulticoreComputeService::getNumIdleCores() {

      if (this->state == Service::DOWN) {
        throw WorkflowExecutionException(new ServiceIsDown(this));
      }

      // send a "num idle cores" message to the daemon's mailbox
      std::string answer_mailbox = S4U_Mailbox::getPrivateMailboxName();

      try {
        S4U_Mailbox::putMessage(this->mailbox_name, new MulticoreComputeServiceNumIdleCoresRequestMessage(
                answer_mailbox,
                this->getPropertyValueAsDouble(
                        MulticoreComputeServiceProperty::NUM_IDLE_CORES_REQUEST_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::getNumIdleCores(): Unknown exception: " + std::string(e.what()));
        }
      }

      // Get the reply
      std::unique_ptr<SimulationMessage> message = nullptr;
      try {
        message = S4U_Mailbox::getMessage(answer_mailbox);
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::getNumIdleCores(): Unknown exception: " + std::string(e.what()));
        }
      }

      if (MulticoreComputeServiceNumIdleCoresAnswerMessage *msg = dynamic_cast<MulticoreComputeServiceNumIdleCoresAnswerMessage *>(message.get())) {
        return msg->num_idle_cores;
      } else {
        throw std::runtime_error(
                "MulticoreComputeService::getNumIdleCores(): unexpected [" + msg->getName() + "] message");
      }
    }

    /**
     * @brief Synchronously ask the service for its TTL
     *
     * @return the TTL in seconds
     *
     * @throw WorkflowExecutionException
     * @throw std::runtime_error
     */
    double MulticoreComputeService::getTTL() {

      if (this->state == Service::DOWN) {
        throw WorkflowExecutionException(new ServiceIsDown(this));
      }

      // send a "ttl request" message to the daemon's mailbox
      std::string answer_mailbox = S4U_Mailbox::getPrivateMailboxName();

      try {
        S4U_Mailbox::dputMessage(this->mailbox_name,
                                 new MulticoreComputeServiceTTLRequestMessage(
                                         answer_mailbox,
                                         this->getPropertyValueAsDouble(
                                                 MulticoreComputeServiceProperty::TTL_REQUEST_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::getTTL(): Unknown exception: " + std::string(e.what()));
        }
      }

      // Get the reply
      std::unique_ptr<SimulationMessage> message = nullptr;

      try {
        message = S4U_Mailbox::getMessage(answer_mailbox);
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::getTTL(): Unknown exception: " + std::string(e.what()));
        }
      }

      if (MulticoreComputeServiceTTLAnswerMessage *msg = dynamic_cast<MulticoreComputeServiceTTLAnswerMessage *>(message.get())) {
        return msg->ttl;
      } else {
        throw std::runtime_error("MulticoreComputeService::getTTL(): unexpected [" + msg->getName() + "] message");
      }
    }

    /**
     * @brief Synchronously ask the service for its per-core flop rate
     *
     * @return the rate in Flops/sec
     *
     * @throw WorkflowExecutionException
     * @throw std::runtime_error
     */
    double MulticoreComputeService::getCoreFlopRate() {

      if (this->state == Service::DOWN) {
        throw WorkflowExecutionException(new ServiceIsDown(this));
      }

      // send a "floprate request" message to the daemon's mailbox
      std::string answer_mailbox = S4U_Mailbox::getPrivateMailboxName();
      try {
        S4U_Mailbox::dputMessage(this->mailbox_name,
                                 new MulticoreComputeServiceFlopRateRequestMessage(
                                         answer_mailbox,
                                         this->getPropertyValueAsDouble(
                                                 MulticoreComputeServiceProperty::FLOP_RATE_REQUEST_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::getCoreFlopRate(): Unknown exception: " + std::string(e.what()));
        }
      }

      // Get the reply
      std::unique_ptr<SimulationMessage> message = nullptr;
      try {
        message = S4U_Mailbox::getMessage(answer_mailbox);
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          throw WorkflowExecutionException(new NetworkError());
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::getCoreFlopRate(): Unknown exception: " + std::string(e.what()));
        }
      }

      if (MulticoreComputeServiceFlopRateAnswerMessage *msg = dynamic_cast<MulticoreComputeServiceFlopRateAnswerMessage *>(message.get())) {
        return msg->flop_rate;
      } else {
        throw std::runtime_error(
                "MulticoreComputeService::getCoreFLopRate(): unexpected [" + msg->getName() + "] message");
      }
    }

    /**
     * @brief Constructor
     *
     * @param hostname: the name of the host on which the job executor should be started
     * @param supports_standard_jobs: true if the job executor should support standard jobs
     * @param supports_pilot_jobs: true if the job executor should support pilot jobs
     * @param plist: a property list
     */
    MulticoreComputeService::MulticoreComputeService(std::string hostname,
                                                     bool supports_standard_jobs,
                                                     bool supports_pilot_jobs,
                                                     std::map<std::string, std::string> plist) :
            MulticoreComputeService::MulticoreComputeService(hostname,
                                                             supports_standard_jobs,
                                                             supports_pilot_jobs, plist, 0, -1, nullptr, "", nullptr) {

    }

    /**
     * @brief Constructor
     *
     * @param hostname: the name of the host on which the job executor should be started
     * @param supports_standard_jobs: true if the job executor should support standard jobs
     * @param supports_pilot_jobs: true if the job executor should support pilot jobs
     * @param default_storage_service: a raw pointer to a StorageService object
     * @param plist: a property list
     */
    MulticoreComputeService::MulticoreComputeService(std::string hostname,
                                                     bool supports_standard_jobs,
                                                     bool supports_pilot_jobs,
                                                     StorageService *default_storage_service,
                                                     std::map<std::string, std::string> plist) :
            MulticoreComputeService::MulticoreComputeService(hostname,
                                                             supports_standard_jobs,
                                                             supports_pilot_jobs,
                                                             plist, 0, -1, nullptr, "",
                                                             default_storage_service) {

    }


    /**
     * @brief Constructor that starts the daemon for the service on a host,
     *        registering it with a WRENCH Simulation
     *
     * @param hostname: the name of the host
     * @param supports_standard_jobs: true if the job executor should support standard jobs
     * @param supports_pilot_jobs: true if the job executor should support pilot jobs
     * @param plist: a property list
     * @param num_worker_threads: the number of worker threads (0 means "use as many as there are cores on the host")
     * @param ttl: the time-ti-live, in seconds
     * @param pj: a containing PilotJob  (nullptr if none)
     * @param suffix: a string to append to the process name
     * @param default_storage_service: a raw pointer to a StorageService object
     *
     * @throw std::invalid_argument
     */
    MulticoreComputeService::MulticoreComputeService(
            std::string hostname,
            bool supports_standard_jobs,
            bool supports_pilot_jobs,
            std::map<std::string, std::string> plist,
            unsigned int num_worker_threads,
            double ttl,
            PilotJob *pj,
            std::string suffix,
            StorageService *default_storage_service) :
            ComputeService("multicore_compute_service" + suffix,
                           "multicore_compute_service" + suffix,
                           default_storage_service) {

      // Set default properties
      for (auto p : this->default_property_values) {
        this->setProperty(p.first, p.second);
      }

      // Set specified properties
      for (auto p : plist) {
        this->setProperty(p.first, p.second);
      }

      this->hostname = hostname;
      if (num_worker_threads > 0) {
        this->max_num_worker_threads = num_worker_threads;
      } else {
        this->max_num_worker_threads = S4U_Simulation::getNumCores(hostname);
      }
      this->ttl = ttl;
      this->has_ttl = (ttl >= 0);
      this->containing_pilot_job = pj;

      this->supports_standard_jobs = supports_standard_jobs;
      this->supports_pilot_jobs = supports_pilot_jobs;

      // Start the daemon on the same host
      try {
        this->start(hostname);
      } catch (std::invalid_argument &e) {
        throw &e;
      }
    }

    /**
     * @brief Main method of the daemon
     *
     * @return 0 on termination
     */
    int MulticoreComputeService::main() {

      TerminalOutput::setThisProcessLoggingColor(WRENCH_LOGGING_COLOR_RED);

      /** Initialize all state **/
      initialize();

      WRENCH_INFO("New Multicore Job Executor starting (%s) with up to %d worker threads ",
                  this->mailbox_name.c_str(), this->max_num_worker_threads);

      this->death_date = -1.0;
      if (this->has_ttl) {
        this->death_date = S4U_Simulation::getClock() + this->ttl;
        WRENCH_INFO("Will be terminating at date %lf", this->death_date);
      }

      /** Main loop **/
      while (this->processNextMessage((this->has_ttl ? this->death_date - S4U_Simulation::getClock() : -1.0))) {

        // Clear pending asynchronous puts that are done
        S4U_Mailbox::clear_dputs();

        /** Dispatch currently pending work until no longer possible **/
        while (this->dispatchNextPendingWork());

        /** Dispatch jobs **/
        while (this->dispatchNextPendingJob()) {
          // Dispatch next pending work until no longer possible
          while (this->dispatchNextPendingWork());
        }
      }

      WRENCH_INFO("Multicore Job Executor on host %s terminated!", S4U_Simulation::getHostName().c_str());
      return 0;
    }

    /**
     * @brief Dispatch one pending job, if possible
     * @return true if a job was dispatched, false otherwise
     */
    bool MulticoreComputeService::dispatchNextPendingJob() {

      /** If some idle core, then see if they can be used **/
      if ((this->working_threads.size() < this->max_num_worker_threads)) {

        /** Look at pending jobs **/
        if (this->pending_jobs.size() > 0) {

          WorkflowJob *next_job = this->pending_jobs.front();

          switch (next_job->getType()) {

            case WorkflowJob::STANDARD: {

              // Put the job in the running queue
              this->pending_jobs.pop();
              this->running_jobs.insert(next_job);

              // Create all the work for the job
              createWorkForNewlyDispatchedJob((StandardJob *) next_job);

              return true;
            }

            case WorkflowJob::PILOT: {
              PilotJob *job = (PilotJob *) next_job;
              WRENCH_INFO("Looking at dispatching pilot job %s with callbackmailbox %s",
                          job->getName().c_str(),
                          job->getCallbackMailbox().c_str());

              if (this->max_num_worker_threads - this->working_threads.size() >=
                  job->getNumCores()) {

                // Immediately decrease the number of available worker threads
                this->max_num_worker_threads -= job->getNumCores();
                WRENCH_INFO("Setting my number of available cores to %d", this->max_num_worker_threads);

                ComputeService *cs =
                        new MulticoreComputeService(S4U_Simulation::getHostName(),
                                                    true, false, this->property_list,
                                                    (unsigned int) job->getNumCores(),
                                                    job->getDuration(), job,
                                                    "_pilot",
                                                    this->default_storage_service);
                cs->setSimulation(this->simulation);

                // Create and launch a compute service for the pilot job
                job->setComputeService(cs);

                // Put the job in the running queue
                this->pending_jobs.pop();
                this->running_jobs.insert(next_job);

                // Send the "Pilot job has started" callback
                // Note the getCallbackMailbox instead of the popCallbackMailbox, because
                // there will be another callback upon termination.
                try {
                  S4U_Mailbox::dputMessage(job->getCallbackMailbox(),
                                           new ComputeServicePilotJobStartedMessage(job, this,
                                                                                    this->getPropertyValueAsDouble(
                                                                                            MulticoreComputeServiceProperty::PILOT_JOB_STARTED_MESSAGE_PAYLOAD)));
                } catch (std::runtime_error &e) {
                  if (!strcmp(e.what(), "network_error")) {
                    throw WorkflowExecutionException(new NetworkError());
                  } else {
                    throw std::runtime_error(
                            "MulticoreComputeService::dispatchNextPendingJob(): Unknown exception: " +
                            std::string(e.what()));
                  }
                }

                // Push my own mailbox onto the pilot job!
                job->pushCallbackMailbox(this->mailbox_name);
                return true;
              }
              break;
            }
          }
        }
      }
      return false;
    }

    /**
     * @brief Dispatch one unit of pending work to a worker thread, if possible
     * @return true if work was dispatched, false otherwise
     */
    bool MulticoreComputeService::dispatchNextPendingWork() {

      if ((this->ready_works.size() > 0) &&
          (this->working_threads.size() < this->max_num_worker_threads)) {

        // Get the first work out of the pending work queue
        WorkUnit *work_to_do = this->ready_works.front();
        this->ready_works.pop_front();
        this->running_works.insert(work_to_do);

        // Create a worker thread!
        WRENCH_INFO("Starting a worker thread to do some work");

        WorkerThread *working_thread =
                new WorkerThread(this->simulation,
                                 S4U_Simulation::getHostName(),
                                 this->mailbox_name,
                                 work_to_do,
                                 this->default_storage_service,
                                 this->getPropertyValueAsDouble(
                                         MulticoreComputeServiceProperty::TASK_STARTUP_OVERHEAD));

        this->working_threads.insert(working_thread);
        return true;

      } else {
        return false;
      }
    }

    /**
     * @brief Wait for and react to any incoming message
     *
     * @param timeout: timeout value in seconds
     *
     * @return false if the daemon should terminate, true otherwise
     *
     * @throw std::runtime_error
     */
    bool MulticoreComputeService::processNextMessage(double timeout) {

      // Wait for a message
      std::unique_ptr<SimulationMessage> message;

      try {
        if (this->has_ttl) {
          if (timeout <= 0) {
            return false;
          } else {
            message = S4U_Mailbox::getMessage(this->mailbox_name, timeout);
          }
        } else {
          message = S4U_Mailbox::getMessage(this->mailbox_name);
        }
      } catch (std::runtime_error &e) {
        if (!strcmp(e.what(), "network_error")) {
          return true;
        } else if (!strcmp(e.what(), "timeout")) {
          WRENCH_INFO("Time out - must die.. !!");
          this->terminate(true);
          return false;
        } else {
          throw std::runtime_error(
                  "MulticoreComputeService::processNextMessage(): Unknown exception: " + std::string(e.what()));
        }
      }

      if (message == nullptr) {
        WRENCH_INFO("Got a NULL message... Likely this means we're all done. Aborting");
        return false;
      }

      WRENCH_INFO("Got a [%s] message", message->getName().c_str());

      if (ServiceStopDaemonMessage *msg = dynamic_cast<ServiceStopDaemonMessage *>(message.get())) {
        this->terminate(false);
        // This is Synchronous
        S4U_Mailbox::putMessage(msg->ack_mailbox,
                                new ServiceDaemonStoppedMessage(this->getPropertyValueAsDouble(
                                        MulticoreComputeServiceProperty::DAEMON_STOPPED_MESSAGE_PAYLOAD)));
        return false;

      } else if (ComputeServiceSubmitStandardJobRequestMessage *msg = dynamic_cast<ComputeServiceSubmitStandardJobRequestMessage *>(message.get())) {
        WRENCH_INFO("Asked to run a standard job with %ld tasks", msg->job->getNumTasks());
        if (not this->supportsStandardJobs()) {
          try {
            S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                     new ComputeServiceSubmitStandardJobAnswerMessage(msg->job, this,
                                                                                      false,
                                                                                      new JobTypeNotSupported(msg->job,
                                                                                                              this),
                                                                                      this->getPropertyValueAsDouble(
                                                                                              MulticoreComputeServiceProperty::SUBMIT_STANDARD_JOB_ANSWER_MESSAGE_PAYLOAD)));
          } catch (std::runtime_error &e) {
            return true;
          }
          return true;
        }

        this->pending_jobs.push(msg->job);

        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                   new ComputeServiceSubmitStandardJobAnswerMessage(msg->job, this,
                                                                                    true,
                                                                                    nullptr,
                                                                                    this->getPropertyValueAsDouble(
                                                                                            MulticoreComputeServiceProperty::SUBMIT_STANDARD_JOB_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          return true;
        }
        return true;
      } else if (ComputeServiceSubmitPilotJobRequestMessage *msg = dynamic_cast<ComputeServiceSubmitPilotJobRequestMessage *>(message.get())) {
        WRENCH_INFO("Asked to run a pilot job with %d cores for %lf seconds", msg->job->getNumCores(),
                    msg->job->getDuration());

        WRENCH_INFO("THE REPLY MAILBOX IS %s", msg->answer_mailbox.c_str());

        bool success = true;
        WorkflowExecutionFailureCause *failure_cause = nullptr;


        if (not this->supportsPilotJobs()) {
          try {
            S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                     new ComputeServiceSubmitPilotJobAnswerMessage(msg->job,
                                                                                   this,
                                                                                   false,
                                                                                   new JobTypeNotSupported(msg->job,
                                                                                                           this),
                                                                                   this->getPropertyValueAsDouble(
                                                                                           MulticoreComputeServiceProperty::SUBMIT_PILOT_JOB_ANSWER_MESSAGE_PAYLOAD)));
          } catch (std::runtime_error &e) {
            return true;
          }
          return true;
        }

        if (S4U_Simulation::getNumCores(this->hostname) < msg->job->getNumCores()) {

          try {
            S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                     new ComputeServiceSubmitPilotJobAnswerMessage(msg->job,
                                                                                   this,
                                                                                   false,
                                                                                   new NotEnoughCores(msg->job, this),
                                                                                   this->getPropertyValueAsDouble(
                                                                                           MulticoreComputeServiceProperty::SUBMIT_PILOT_JOB_ANSWER_MESSAGE_PAYLOAD)));
          } catch (std::runtime_error &e) {
            return true;
          }
          return true;
        }

        // success
        WRENCH_INFO("SENDING REPLY TO %s", msg->answer_mailbox.c_str());
        this->pending_jobs.push(msg->job);
        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                   new ComputeServiceSubmitPilotJobAnswerMessage(msg->job,
                                                                                 this,
                                                                                 true,
                                                                                 nullptr,
                                                                                 this->getPropertyValueAsDouble(
                                                                                         MulticoreComputeServiceProperty::SUBMIT_PILOT_JOB_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          return true;
        }
        return true;

      } else if (WorkerThreadWorkDoneMessage *msg = dynamic_cast<WorkerThreadWorkDoneMessage *>(message.get())) {
        processWorkCompletion(msg->worker_thread, msg->work);
        return true;

      } else if (WorkerThreadWorkFailedMessage *msg = dynamic_cast<WorkerThreadWorkFailedMessage *>(message.get())) {
        processWorkFailure(msg->worker_thread, msg->work, msg->cause);
        return true;

      } else if (ComputeServicePilotJobExpiredMessage *msg = dynamic_cast<ComputeServicePilotJobExpiredMessage *>(message.get())) {
        processPilotJobCompletion(msg->job);
        return true;

      } else if (MulticoreComputeServiceNumCoresRequestMessage *msg = dynamic_cast<MulticoreComputeServiceNumCoresRequestMessage *>(message.get())) {
        MulticoreComputeServiceNumCoresAnswerMessage *answer_message = new MulticoreComputeServiceNumCoresAnswerMessage(
                S4U_Simulation::getNumCores(this->hostname),
                this->getPropertyValueAsDouble(
                        MulticoreComputeServiceProperty::NUM_CORES_ANSWER_MESSAGE_PAYLOAD));
        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox, answer_message);
        } catch (std::runtime_error &e) {
          return true;
        }
        return true;
      } else if (MulticoreComputeServiceNumIdleCoresRequestMessage *msg = dynamic_cast<MulticoreComputeServiceNumIdleCoresRequestMessage *>(message.get())) {
        MulticoreComputeServiceNumIdleCoresAnswerMessage *answer_message = new MulticoreComputeServiceNumIdleCoresAnswerMessage(
                this->max_num_worker_threads - (unsigned int) this->working_threads.size(),
                this->getPropertyValueAsDouble(
                        MulticoreComputeServiceProperty::NUM_IDLE_CORES_ANSWER_MESSAGE_PAYLOAD));
        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox, answer_message);
        } catch (std::runtime_error &e) {
          return true;
        }
        return true;
      } else if (MulticoreComputeServiceTTLRequestMessage *msg = dynamic_cast<MulticoreComputeServiceTTLRequestMessage *>(message.get())) {
        MulticoreComputeServiceTTLAnswerMessage *answer_message = new MulticoreComputeServiceTTLAnswerMessage(
                this->death_date - S4U_Simulation::getClock(),
                this->getPropertyValueAsDouble(
                        MulticoreComputeServiceProperty::TTL_ANSWER_MESSAGE_PAYLOAD));
        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox, answer_message);
        } catch (std::runtime_error &e) {
          return true;
        }
        return true;
      } else if (MulticoreComputeServiceFlopRateRequestMessage *msg = dynamic_cast<MulticoreComputeServiceFlopRateRequestMessage *>(message.get())) {
        MulticoreComputeServiceFlopRateAnswerMessage *answer_message = new MulticoreComputeServiceFlopRateAnswerMessage(
                simgrid::s4u::Host::by_name(this->hostname)->getPstateSpeed(0),
                this->getPropertyValueAsDouble(
                        MulticoreComputeServiceProperty::FLOP_RATE_ANSWER_MESSAGE_PAYLOAD));
        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox, answer_message);
        } catch (std::runtime_error &e) {
          return true;
        }
        return true;
      } else {
        throw std::runtime_error("Unexpected [" + message->getName() + "] message");
      }
    }

    /**
     * @brief Terminate all pilot job compute services
     */
    void MulticoreComputeService::terminateAllPilotJobs() {
      for (auto job : this->running_jobs) {
        if (job->getType() == WorkflowJob::PILOT) {
          PilotJob *pj = (PilotJob *) job;
          WRENCH_INFO("Terminating pilot job %s", job->getName().c_str());
          pj->getComputeService()->stop();
        }
      }
    }

    /**
     * @brief fail a pending standard job
     * @param job: the job
     * @param cause: the failure cause
     */
    void MulticoreComputeService::failPendingStandardJob(StandardJob *job, WorkflowExecutionFailureCause *cause) {
      WRENCH_INFO("Failing job %s", job->getName().c_str());
      // Set all tasks back to the READY state and wipe out output files
      for (auto failed_task: job->getTasks()) {
        failed_task->setFailed();
        failed_task->setReady();
        try {
          StorageService::deleteFiles(failed_task->getOutputFiles(), job->getFileLocations(),
                                      this->default_storage_service);
        } catch (WorkflowExecutionException &e) {
          WRENCH_WARN("Warning: %s", e.getCause()->toString().c_str());
        }
      }

      // Send back a job failed message
      WRENCH_INFO("Sending job failure notification to '%s'", job->getCallbackMailbox().c_str());
      // NOTE: This is synchronous so that the process doesn't fall off the end
      try {
        S4U_Mailbox::putMessage(job->popCallbackMailbox(),
                                new ComputeServiceStandardJobFailedMessage(job, this, cause,
                                                                           this->getPropertyValueAsDouble(
                                                                                   MulticoreComputeServiceProperty::STANDARD_JOB_FAILED_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        return;
      }
    }

    /**
  * @brief fail a running standard job
  * @param job: the job
  * @param cause: the failure cause
  */
    void MulticoreComputeService::failRunningStandardJob(StandardJob *job, WorkflowExecutionFailureCause *cause) {

      WRENCH_INFO("Failing job %s", job->getName().c_str());

      std::vector<WorkUnit *> works_to_terminate;

      // Brutally terminate all relevant running work
      for (auto w : this->running_works) {
        if (w->job == job) {
          works_to_terminate.push_back(w);
        }
      }

      for (auto w : works_to_terminate) {
        WorkerThread *worker_thread_to_terminate = nullptr;
        for (auto wt : this->working_threads) {
          if (wt->work == w) {
            worker_thread_to_terminate = wt;
            break;
          }
        }
        if (worker_thread_to_terminate == nullptr) {
          throw std::runtime_error("Can't find worker threads for work belonging to job " + w->job->getName());
        }
        WRENCH_INFO("SHOULD NOW KILL WORKER THREAD %s", worker_thread_to_terminate->getName().c_str());
        WRENCH_INFO("TODO TODO TODO TODO TODO TODO TODO");
        worker_thread_to_terminate->kill();
      }



      // Set all tasks back to the READY state and wipe out output files
      for (auto failed_task: job->getTasks()) {
        if (failed_task->getState() != WorkflowTask::COMPLETED) {
          failed_task->setFailed();
          failed_task->setReady();
        }
      }

      // Send back a job failed message (Not that it can be a partial fail)
      WRENCH_INFO("Sending job failure notification to '%s'", job->getCallbackMailbox().c_str());
      // NOTE: This is synchronous so that the process doesn't fall off the end
      try {
        S4U_Mailbox::putMessage(job->popCallbackMailbox(),
                                new ComputeServiceStandardJobFailedMessage(job, this, cause,
                                                                           this->getPropertyValueAsDouble(
                                                                                   MulticoreComputeServiceProperty::STANDARD_JOB_FAILED_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        return;
      }
    }


    /**
 * @brief Declare all current jobs as failed (likely because the daemon is being terminated
 * or has timed out (because it's in fact a pilot job))
 */
    void MulticoreComputeService::failCurrentStandardJobs(WorkflowExecutionFailureCause *cause) {

      WRENCH_INFO("Failing %ld running jobs", this->running_jobs.size());
      for (auto workflow_job : this->running_jobs) {
        if (workflow_job->getType() == WorkflowJob::STANDARD) {
          StandardJob *job = (StandardJob *) workflow_job;
          this->failRunningStandardJob(job, cause);
        }
      }

      WRENCH_INFO("Failing %ld pending jobs", this->pending_jobs.size());
      while (not this->pending_jobs.empty()) {
        WorkflowJob *workflow_job = this->pending_jobs.front();
        this->pending_jobs.pop();
        if (workflow_job->getType() == WorkflowJob::STANDARD) {
          StandardJob *job = (StandardJob *) workflow_job;
          this->failPendingStandardJob(job, cause);
        }
      }
    }

/**
 * @brief Initialize all state for the daemon
 */
    void MulticoreComputeService::initialize() {

      // Figure out the number of worker threads
      if (this->max_num_worker_threads == 0) {
        this->max_num_worker_threads = S4U_Simulation::getNumCores(S4U_Simulation::getHostName());
      }
    }

    /**
     * @brief Process a work completion
     * @param worker_thread: the worker thread that completed the work
     * @param work: the work
     *
     * @throw std::runtime_error
     */
    void MulticoreComputeService::processWorkCompletion(WorkerThread *worker_thread, WorkUnit *work) {

      // Remove the work thread from the working list
      for (auto wt : this->working_threads) {
        if (wt == worker_thread) {
          this->working_threads.erase(wt);
          break;
        }
      }

      // Remove the work from the running work queue
      if (this->running_works.find(work) == this->running_works.end()) {
        throw std::runtime_error(
                "MulticoreComputeService::processWorkCompletion(): just completed work should be in the running work queue");
      }
      this->running_works.erase(work);

      // Add the work to the completed work queue
      this->completed_works.insert(work);

      // Process task completions, if any
      for (auto task : work->tasks) {

        WRENCH_INFO("One of my worker threads completed task %s (and its state is: %s)", task->getId().c_str(),
                    WorkflowTask::stateToString(task->getState()).c_str());

        // Increase the "completed tasks" count of the job
        work->job->incrementNumCompletedTasks();


      }

      // Send the callback to the originator if the job has completed (i.e., if this
      // work unit has no children)
      if (work->children.size() == 0) {
        this->running_jobs.erase(work->job);
        try {
          S4U_Mailbox::dputMessage(work->job->popCallbackMailbox(),
                                   new ComputeServiceStandardJobDoneMessage(work->job, this,
                                                                            this->getPropertyValueAsDouble(
                                                                                    MulticoreComputeServiceProperty::STANDARD_JOB_DONE_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          return;
        }
      } else {
        // Otherwise, update children
        for (auto child : work->children) {
          child->num_pending_parents--;
          if (child->num_pending_parents == 0) {
            // Make the child ready!
            if (this->non_ready_works.find(child) == this->non_ready_works.end()) {
              throw std::runtime_error(
                      "MulticoreComputeService::processWorkCompletion(): can't find non-ready child in non-ready set!");
            }
            this->non_ready_works.erase(child);
            this->ready_works.push_back(child);
          }
        }
      }

      return;
    }


    /**
     * @brief Process a work failure
     * @param worker_thread: the worker thread that did the work
     * @param work: the work
     * @param cause: the cause of the failure
     */
    void MulticoreComputeService::processWorkFailure(WorkerThread *worker_thread,
                                                     WorkUnit *work,
                                                     WorkflowExecutionFailureCause *cause) {

      StandardJob *job = work->job;

      WRENCH_INFO("A worker thread has failed to do work on behalf of job %s", job->getName().c_str());

      // Remove the work thread from the working list
      for (auto wt : this->working_threads) {
        if (wt == worker_thread) {
          this->working_threads.erase(wt);
          break;
        }
      }

      // Remove the work from the running work queue
      if (this->running_works.find(work) == this->running_works.end()) {
        throw std::runtime_error(
                "MulticoreComputeService::processWorkFailure(): just completed work should be in the running work queue");
      }
      this->running_works.erase(work);

      // Remove all other works for the job in the "not ready" state
      std::vector<WorkUnit *> to_erase;

      for (auto w : this->non_ready_works) {
        if (w->job == job) {
          to_erase.push_back(w);
        }
      }

      for (auto w : to_erase) {
        this->non_ready_works.erase(w);
      }
      to_erase.clear();

      // Remove all other works for the job in the "ready" state
      for (std::deque<WorkUnit *>::iterator it = this->ready_works.begin(); it != this->ready_works.end(); it++) {
        if ((*it)->job == job) {
          this->ready_works.erase(it);
        }
      }

      // Deal with running works!
      for (auto w : this->running_works) {
        if (w->job == job) {
          if ((w->post_file_copies.size() != 0) || (w->pre_file_copies.size() != 0)) {
            throw std::runtime_error(
                    "MulticoreComputeService::processWorkFailure(): trying to cancel a running work that's doing some file copy operations - not supported (for now)");
          }
          // find the worker thread that's doing the work (lame iteration)
          for (auto wt :  this->working_threads) {
            if (wt->work == w) {
              wt->kill();
            }
          }
          // Remove the work from the running queue
          this->running_works.erase(w);
        }
      }

      // Deal with completed works
      for (auto w : this->completed_works) {
        if (w->job == job) {
          to_erase.push_back(w);
        }
      }
      for (auto w : to_erase) {
        this->non_ready_works.erase(w);
      }
      to_erase.clear();

      // Remove the job from the list of running jobs
      this->running_jobs.erase(job);

      // Fail the job
      this->failPendingStandardJob(job, cause);

    }

    /**
     * @brief Terminate the daemon, dealing with pending/running jobs
     */
    void MulticoreComputeService::terminate(bool notify_pilot_job_submitters) {

      this->setStateToDown();

      WRENCH_INFO("Failing current standard jobs");
      this->failCurrentStandardJobs(new ServiceIsDown(this));

      WRENCH_INFO("Terminate all pilot jobs");
      this->terminateAllPilotJobs();

      // Am I myself a pilot job?
      if (notify_pilot_job_submitters && this->containing_pilot_job) {

        WRENCH_INFO("Letting the level above know that the pilot job has ended on mailbox %s",
                    this->containing_pilot_job->getCallbackMailbox().c_str());
        // NOTE: This is synchronous so that the process doesn't fall off the end
        try {
          S4U_Mailbox::putMessage(this->containing_pilot_job->popCallbackMailbox(),
                                  new ComputeServicePilotJobExpiredMessage(this->containing_pilot_job, this,
                                                                           this->getPropertyValueAsDouble(
                                                                                   MulticoreComputeServiceProperty::PILOT_JOB_EXPIRED_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          return;
        }
      }
    }

    /**
     * @brief Process a pilot job completion
     *
     * @param job: pointer to the PilotJob object
     */
    void MulticoreComputeService::processPilotJobCompletion(PilotJob *job) {

      // Remove the job from the running list
      this->running_jobs.erase(job);

      // Update the number of available cores
      this->max_num_worker_threads += job->getNumCores();

      // Forward the notification
      try {
        S4U_Mailbox::dputMessage(job->popCallbackMailbox(),
                                 new ComputeServicePilotJobExpiredMessage(job, this,
                                                                          this->getPropertyValueAsDouble(
                                                                                  MulticoreComputeServiceProperty::PILOT_JOB_EXPIRED_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        return;
      }

      return;
    }


    /**
     * @brief Create all work for a newly dispatched job
     * @param job: the job
     */
    void MulticoreComputeService::createWorkForNewlyDispatchedJob(StandardJob *job) {

      std::vector<WorkUnit *> pre_file_copies_work_unit = {};
      std::vector<WorkUnit *> post_file_copies_work_unit = {};

      // Create all the work units

      if (job->pre_file_copies.size() > 0) {
        pre_file_copies_work_unit.push_back(new WorkUnit(job, {}, 0, job->pre_file_copies, {}, {}, {}));
      }

      if (job->post_file_copies.size() > 0) {
        post_file_copies_work_unit.push_back(new WorkUnit(job, {}, 0, {}, {}, {}, job->post_file_copies));
      }

      for (auto task : job->tasks) {
        WorkUnit *task_work_unit = new WorkUnit(job, post_file_copies_work_unit, pre_file_copies_work_unit.size(),
                                                {}, {task}, job->file_locations, {});
        if (pre_file_copies_work_unit.size() > 0) {
          pre_file_copies_work_unit[0]->children.push_back(task_work_unit);
        }
        if (post_file_copies_work_unit.size() > 0) {
          post_file_copies_work_unit[0]->num_pending_parents++;
        }

        if (task_work_unit->num_pending_parents == 0) {
          this->ready_works.push_back(task_work_unit);
        } else {
          this->non_ready_works.insert(task_work_unit);
        };
      }

      // Add the pre and post work units to the lists

      if (pre_file_copies_work_unit.size() > 0) {
        if (pre_file_copies_work_unit[0]->num_pending_parents == 0) {
          this->ready_works.push_back(pre_file_copies_work_unit[0]);
        } else {
          this->non_ready_works.insert(pre_file_copies_work_unit[0]);
        }
      }

      if (post_file_copies_work_unit.size() > 0) {
        if (post_file_copies_work_unit[0]->num_pending_parents == 0) {
          this->ready_works.push_back(post_file_copies_work_unit[0]);
        } else {
          this->non_ready_works.insert(post_file_copies_work_unit[0]);
        }
      }

      return;
    }


};