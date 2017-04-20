/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_MAXMINSCHEDULER_H
#define WRENCH_MAXMINSCHEDULER_H

#include "wms/scheduler/SchedulerFactory.h"

namespace wrench {

		class JobManager;

		extern const char maxmin_name[] = "MaxMinScheduler";

		/***********************/
		/** \cond DEVELOPER    */
		/***********************/

		/**
		 * @brief A max-min Scheduler class
		 */
		class MaxMinScheduler : public SchedulerTmpl<maxmin_name, MaxMinScheduler> {

		public:
				MaxMinScheduler();

				void scheduleTasks(JobManager *job_manager, std::vector<WorkflowTask *> ready_tasks,
													 const std::set<ComputeService *> &compute_services);
				void schedulePilotJobs(JobManager *job_manager,
															 Workflow *workflow,
															 double flops,
															 const std::set<ComputeService *> &compute_services);

				/**
				 * @brief Helper struct for the MaxMinScheduler
				 */
				struct MaxMinComparator {
						bool operator()(WorkflowTask *&lhs, WorkflowTask *&rhs);
				};

		};

		/***********************/
		/** \endcond DEVELOPER */
		/***********************/

}

#endif //WRENCH_MAXMINSCHEDULER_H