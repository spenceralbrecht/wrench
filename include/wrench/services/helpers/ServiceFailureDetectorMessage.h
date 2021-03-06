/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */


#ifndef WRENCH_FAILUREDETECTORMESSAGE_H
#define WRENCH_FAILUREDETECTORMESSAGE_H


#include <wrench/simulation/SimulationMessage.h>
#include <wrench-dev.h>

namespace wrench {


    /***********************/
    /** \cond INTERNAL     */
    /***********************/

    /**
     * @brief Top-level class for messages received/sent by a FailureDetector
     */
    class ServiceFailureDetectorMessage : public SimulationMessage {
    protected:
        ServiceFailureDetectorMessage(std::string name);
    };

    class ServiceHaCrashedeMessage : public ServiceFailureDetectorMessage {
    public:
        ServiceHaCrashedeMessage(Service *service);

        Service *service;
    };

    /***********************/
    /** \endcond           */
    /***********************/

};

#endif //WRENCH_FAILUREDETECTORMESSAGE_H
