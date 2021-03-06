/**
 * Copyright (c) 2017-2018. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "JobManagerMessage.h"

namespace wrench {

    /**
     * @brief Constructor
     *
     * @param name: the message name
     */
    JobManagerMessage::JobManagerMessage(std::string name) :
            SimulationMessage("JobManagerMessage::" + name, 0) {
    }


    JobManagerStandardJobDoneMessage::JobManagerStandardJobDoneMessage(StandardJob *job,
                                                                       ComputeService *compute_service,
                                                                       std::map<WorkflowTask *, WorkflowTask::State> necessary_state_changes) :
            JobManagerMessage("JobManagerStandardJobDoneMessage") {
      this->job = job;
      this->compute_service = compute_service;
      this->necessary_state_changes = necessary_state_changes;
    }

    JobManagerStandardJobFailedMessage::JobManagerStandardJobFailedMessage(StandardJob *job,
                                                                           ComputeService *compute_service,
                                                                           std::map<WorkflowTask *, WorkflowTask::State> necessary_state_changes,
                                                                           std::set<WorkflowTask *> necessary_failure_count_increments,
                                                                           std::shared_ptr<FailureCause> cause) :
            JobManagerMessage("JobManagerStandardJobFailedMessage") {
      this->job = job;
      this->compute_service = compute_service;
      this->necessary_state_changes = necessary_state_changes;
      this->necessary_failure_count_increments = necessary_failure_count_increments;
      this->cause = cause;
    }


}
