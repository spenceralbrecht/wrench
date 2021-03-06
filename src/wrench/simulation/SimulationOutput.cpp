/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */


#include "wrench/simulation/Simulation.h"
#include "wrench/simulation/SimulationOutput.h"
#include "wrench/workflow/Workflow.h"
#include "simgrid/s4u.hpp"
#include "simgrid/plugins/energy.h"

#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>

#include <iomanip>
#include <fstream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <string>
#include <unordered_set>

namespace wrench {

    /**
     * \cond
     */

    /**
     * @brief Object representing an instance when a WorkflowTask was run.
     */
    typedef struct WorkflowTaskExecutionInstance {
        std::string task_id;
        unsigned long long num_cores_allocated;
        unsigned long long vertical_position;

        std::pair<double, double> whole_task;
        std::pair<double, double> read;
        std::pair<double, double> compute;
        std::pair<double, double> write;

        double failed;
        double terminated;

        std::string hostname;
        double host_flop_rate;
        double host_memory;
        unsigned long long host_num_cores;

        double getTaskEndTime() {
            return std::max({
                this->whole_task.second,
                this->failed,
                this->terminated
            });
        }
    } WorkflowTaskExecutionInstance;

    /**
     * @brief Function called by the nlohmann::json constructor when a WorkflowTaskExecutionInstance is passed in as
     *      a parameter. This returns the JSON representation of a WorkflowTaskExecutionInstance. The name of this function
     *      is important and should not be changed as it is what nlohmann expects (hardcoded in there).
     * @param j: reference to a JSON object
     * @param w: reference to a WorkflowTaskExecutionInstance
     */
    void to_json(nlohmann::json &j, const WorkflowTaskExecutionInstance &w) {
        j = nlohmann::json{
                {"task_id", w.task_id},
                {"execution_host", {
                                    {"hostname", w.hostname},
                                    {"flop_rate", w.host_flop_rate},
                                    {"memory", w.host_memory},
                                    {"cores", w.host_num_cores}

                            }},
                {"num_cores_allocated", w.num_cores_allocated},
                {"vertical_position", w.vertical_position},
                {"whole_task", {
                                    {"start", w.whole_task.first},
                                    {"end", w.whole_task.second}
                            }},
                {"read", {
                                    {"start", w.read.first},
                                    {"end", w.read.second}
                            }},
                {"compute", {
                                    {"start", w.compute.first},
                                    {"end", w.compute.second}
                            }},
                {"write", {
                                    {"start", w.write.first},
                                    {"end", w.write.second}
                            }},
                {"failed", w.failed},
                {"terminated", w.terminated}
        };
    }

    /**
     * @brief Determines if a point lies on a line segment.
     * @param segment: pair of start and end points that make up the line segment
     * @param point: point on a 1D plane
     * @return bool
     */
    bool isPointOnSegment(std::pair<unsigned long long, unsigned long long> segment, unsigned long long point) {
        return (point <= std::max(segment.first, segment.second) and point >= std::min(segment.first, segment.second));
    }

    /**
     * @brief Determines if two line segments overlap along the x-axis and allows for a slight overlap.
     * @param segment1: first segment
     * @param segment2: second segment
     * @return bool
     */
    bool isSegmentOverlappingXAxis(std::pair<unsigned long long, unsigned long long> segment1, std::pair<unsigned long long, unsigned long long> segment2) {
        const unsigned long long EPSILON = 1000 * 1000 * 10;
        if (std::fabs(segment1.second - segment2.first) <= EPSILON or std::fabs(segment2.second -  segment1.first) <= EPSILON) {
            return false;

        // if any point of either segment lies within the other, we have overlap
        } else if (isPointOnSegment(segment1, segment2.first) or isPointOnSegment(segment1, segment2.second) or
                   isPointOnSegment(segment2, segment1.first) or isPointOnSegment(segment2, segment1.second)) {
            return true;

        // the two segments do not overlap
        } else {
            return false;
        }
    }

    /**
     * @brief Determines if two line segments overlap along the y-axis using exact values.
     * @param segment1: first segment
     * @param segment2: second segment
     * @return bool
     */
    bool isSegmentOverlappingYAxis(std::pair<unsigned long long, unsigned long long> segment1, std::pair<unsigned long long, unsigned long long> segment2) {
        if (segment1.second == segment2.first or segment2.second == segment1.first) {
            return false;
            // if any point of either segment lies within the other, we have overlap
        } else if (isPointOnSegment(segment1, segment2.first) or isPointOnSegment(segment1, segment2.second) or
                   isPointOnSegment(segment2, segment1.first) or isPointOnSegment(segment2, segment1.second)) {
            return true;

            // the two segments do not overlap
        } else {
            return false;
        }
    }

    /**
     * @brief Searches for a possible host utilization gantt chart layout and updates the data to include what vertical
     *        position to plot each rectangle.
     * @description Recursive backtracking search for a valid gantt chart layout. This algorithm looks for a
     *              vertical position to place each task execution event such that it doesn't overlap with
     *              any other task.
     *
     * @param data: JSON workflow execution data
     * @param index: the index of the workflow execution data up to where we would like to check for a valid layout
     * @return bool
     */
    bool searchForLayout(std::vector<WorkflowTaskExecutionInstance> &data, std::size_t index) {
        const unsigned long long PRECISION = 1000 * 1000 * 1000;

        WorkflowTaskExecutionInstance &current_execution_instance = data.at(index);

        auto current_rect_x_range = std::pair<unsigned long long, unsigned long long>(
                    current_execution_instance.whole_task.first * PRECISION,
                    current_execution_instance.getTaskEndTime() * PRECISION
                );

        unsigned long long num_cores_allocated = current_execution_instance.num_cores_allocated;
        unsigned long long execution_host_num_cores = current_execution_instance.host_num_cores;
        auto num_vertical_positions = execution_host_num_cores - num_cores_allocated + 1;

//      std::string spaces = "";
//      for (int i=0; i < index; i++) {
//        spaces += "  ";
//      }
//      std::cout << spaces + "task = " << index <<"\n";


      /*
       * For each possible vertical position that an event can be in, we perform a check to see that its vertical
       * position doesn't make the event (a rectangle on the graph) overlap with any of the other events. If it does not
       * overlap, then we can evaluate all events up to this one with the next event in the list (recursive call). If
       * it does overlap, then we try another vertical position. If it doesn't overlap and this is the last item in the list,
       * we have found a valid layout.
       */
        for (std::size_t vertical_position = 0; vertical_position < num_vertical_positions; ++vertical_position) {
            // Set the vertical positions as we go so the entire graph layout is set when the function returns
            current_execution_instance.vertical_position = vertical_position;

            auto current_rect_y_range = std::pair<unsigned long long, unsigned long long>(
                    vertical_position,
                    vertical_position + num_cores_allocated
            );

//              std::cout << spaces + "  pos = " <<  vertical_position << "\n";
            /*
             * We check this current event's position against all other events that were added
             * before it to make sure it doesn't overlap with any of those events.
             */
            bool has_overlap = false;
            for (std::size_t i = 0; i < index; ++i) {

                WorkflowTaskExecutionInstance other_execution_instance = data.at(i);

                // Evaluate the current event's position only against others that occurred on the same host.
                if (current_execution_instance.hostname == other_execution_instance.hostname) {
                    auto other_rect_x_range = std::pair<unsigned long long, unsigned long long>(
                            other_execution_instance.whole_task.first * PRECISION,
                            other_execution_instance.getTaskEndTime() * PRECISION
                    );

                    auto other_rect_y_range = std::pair<unsigned long long, unsigned long long>(
                            other_execution_instance.vertical_position,
                            other_execution_instance.vertical_position + other_execution_instance.num_cores_allocated
                    );

                    /*
                     * Check overlap for the x_ranges first. If there is no overlap, we can guarantee that the rectangles
                     * will not overlap. If the x_ranges do overlap, then we need to evaluate the y_ranges for overlap.
                     */
                    if (isSegmentOverlappingXAxis(current_rect_x_range, other_rect_x_range)) {
                        if (isSegmentOverlappingYAxis(current_rect_y_range, other_rect_y_range)) {
                            has_overlap = true;
                            break;
                        }
                    }
                }
            }

            if (not has_overlap and index >= data.size() - 1) {
                return true;
            } else if (not has_overlap) {
                bool found_layout = searchForLayout(data, index + 1);

                if (found_layout) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Generates graph layout for host utilization and adds that information to the JSON object.
     * @description Searches for a possible gantt chart layout to represent host utilization. If a layout is found
     *              (no tasks overlap), then information about where to plot what is added to the JSON object. Note
     *              that this is a possible layout and does not reflect what task ran on what core specifically. For
     *              example, we may hav a task that was allocated 2-cores on a idle 4-core host. The task, when plotted
     *              on the gantt chart may end up in 1 of 3 positions (using cores 0 and 1, 1 and 2, or 2 and 3).
     * @param data: JSON workflow execution data
     *
     * @throws std::runtime_error
     */
    void generateHostUtilizationGraphLayout(std::vector<WorkflowTaskExecutionInstance> &data) {
        if (searchForLayout(data, 0) == false) {
            throw std::runtime_error("SimulationOutput::generateHostUtilizationGraphLayout() could not find a valid layout.");
        }
    }

    /**
     * \endcond
     */

    /**
      * @brief Writes WorkflowTask execution history for each task to a file, formatted as a JSON array.
      * @description The JSON array has the following format:
      *
      * <pre>
      *    [
      *      {
      *          task_id: <string>,
      *          execution_host: {
      *              hostname: <string>,
      *              flop_rate: <double>,
      *              memory: <double>,
      *              cores: <unsigned_long>
      *          },
      *          num_cores_allocated: <unsigned_long>,
      *          vertical_position: <unsigned_long>,
      *          whole_task: { start: <double>, end: <double> },
      *          read:       { start: <double>, end: <double> },
      *          compute:    { start: <double>, end: <double> },
      *          write:      { start: <double>, end: <double> },
      *          failed: <double>,
      *          terminated: <double>
      *      }, . . .
      *    ]
      * </pre>
      *
      *   If generate_host_utilization_layout is set to true, a recursive function searches for a possible host
      *   utilization layout where tasks are assumed to use contiguous numbers of cores on their execution hosts.
      *   Note that each ComputeService does not enforce this, and such a layout may not exist for some workflow executions.
      *   In this situation, the function will go through the entire search space until all possible layouts are evaluated.
      *   For a large Workflow, this may take a very long time.
      *
      *   If a host utilization layout is able to be generated, the 'vertical_position' values will be set for each task run,
      *   and the task can be plotted as a rectangle on a graph where the y-axis denotes the number of cores - 1, and the x-axis denotes the
      *   workflow execution timeline. The vertical_position specifies the bottom of the rectangle. num_cores_allocated specifies the height
      *   of the rectangle.
      *
      * @param workflow: a pointer to the Workflow
      * @param file_path: the path to write the file
      * @param generate_host_utilization_layout: boolean specifying whether or not you would like a possible host utilization
      *     layout to be generated
      *
      * @throws std::invalid_argument
      */
    void SimulationOutput::dumpWorkflowExecutionJSON(Workflow *workflow, std::string file_path, bool generate_host_utilization_layout) {
        if (workflow == nullptr || file_path.empty()) {
            throw std::invalid_argument("SimulationOutput::dumpTaskDataJSON() requires a valid workflow and file_path");
        }

        auto tasks = workflow->getTasks();

        std::vector<WorkflowTaskExecutionInstance> data;

        // For each attempted execution of a task, add a WorkflowTaskExecutionInstance to the list.
        for (auto const &task : tasks) {
            auto execution_history = task->getExecutionHistory();

            while (not execution_history.empty()) {
                auto current_task_execution = execution_history.top();

                WorkflowTaskExecutionInstance current_execution_instance;

                current_execution_instance.task_id = task->getID();

                current_execution_instance.hostname       = current_task_execution.execution_host;
                current_execution_instance.host_flop_rate =  Simulation::getHostFlopRate(current_task_execution.execution_host);
                current_execution_instance.host_memory    =  Simulation::getHostMemoryCapacity(current_task_execution.execution_host);
                current_execution_instance.host_num_cores = Simulation::getHostNumCores(current_task_execution.execution_host);

                current_execution_instance.num_cores_allocated = current_task_execution.num_cores_allocated;
                current_execution_instance.vertical_position   = 0;

                current_execution_instance.whole_task = std::make_pair(current_task_execution.task_start,         current_task_execution.task_end);
                current_execution_instance.read       = std::make_pair(current_task_execution.read_input_start,   current_task_execution.read_input_end);
                current_execution_instance.compute    = std::make_pair(current_task_execution.computation_start,  current_task_execution.computation_end);
                current_execution_instance.write      = std::make_pair(current_task_execution.write_output_start, current_task_execution.write_output_end);

                current_execution_instance.failed     = current_task_execution.task_failed;
                current_execution_instance.terminated = current_task_execution.task_terminated;

                data.push_back(current_execution_instance);
                execution_history.pop();
            }
        }

        // Set the "vertical position" of each WorkflowExecutionInstance so we know where to plot each rectangle
        if (generate_host_utilization_layout) {
            generateHostUtilizationGraphLayout(data);
        }

        std::ofstream output(file_path);
        output << std::setw(4) << nlohmann::json(data) << std::endl;
        output.close();
    }

    /**
     * @brief Writes a JSON graph representation of the Workflow to a file.
     * @description A node is added for each WorkflowTask and WorkflowFile. A WorkflowTask will have the type "task" and
     *  a WorkflowFile will have the type "file". A directed link is added for each dependency in the Workflow.
     *
     * <pre>
     * {
     *      vertices: [
     *          {
     *              type: <"task">,
     *              id: <string>,
     *              flops: <double>,
     *              min_cores: <unsigned_long>,
     *              max_cores: <unsigned_long>,
     *              parallel_efficiency: <double>,
     *              memory: <double>,
     *          },
     *          {
     *              type: <"file">,
     *              id: <string>,
     *              size: <double>
     *          }, . . .
     *      ],
     *      edges: [
     *          {
     *              source: <string>,
     *              target: <string>
     *          }, . . .
     *      ]
     *  }
     *  </pre>
     *
     * @param workflow: a pointer to the workflow
     * @param file_path: the path to write the file
     *
     * @throws std::invalid_argument
     */
    void SimulationOutput::dumpWorkflowGraphJSON(wrench::Workflow *workflow, std::string file_path) {
        if (workflow == nullptr || file_path.empty()) {
            throw std::invalid_argument("SimulationOutput::dumpTaskDataJSON() requires a valid workflow and file_path");
        }

        /* schema
         *
         */
        nlohmann::json vertices;
        nlohmann::json edges;

        // add the task vertices
        for (const auto &task : workflow->getTasks()) {
            vertices.push_back({
                                    {"type", "task"},
                                    {"id", task->getID()},
                                    {"flops", task->getFlops()},
                                    {"min_cores", task->getMinNumCores()},
                                    {"max_cores", task->getMaxNumCores()},
                                    {"parallel_efficiency", task->getParallelEfficiency()},
                                    {"memory", task->getMemoryRequirement()}
                            });
        }

        // add the file vertices
        for (const auto &file : workflow->getFiles()) {
            vertices.push_back({
                                    {"type", "file"},
                                    {"id", file->getID()},
                                    {"size", file->getSize()}
                            });
        }

        // add the edges
        for (const auto &task : workflow->getTasks()) {
            // create edges between input files (if any) and the current task
            for (const auto &input_file : task->getInputFiles()) {
                edges.push_back({
                                        {"source", input_file->getID()},
                                        {"target", task->getID()}
                                });
            }


            bool has_output_files = (task->getOutputFiles().size() > 0) ? true : false;
            bool has_children = (task->getNumberOfChildren() > 0) ? true : false;

            if (has_output_files) {
                // create the edges between current task and its output files (if any)
                for (const auto &output_file : task->getOutputFiles()) {
                    edges.push_back({{"source", task->getID()},
                                     {"target", output_file->getID()}});
                }
            } else if (has_children) {
                // then create the edges from the current task to its children tasks (if it has not output files)
                for (const auto & child : workflow->getTaskChildren(task)) {
                    edges.push_back({
                                            {"source", task->getID()},
                                            {"target", child->getID()}});
                }
            }
        }

        nlohmann::json workflow_task_graph;
        workflow_task_graph["vertices"] = vertices;
        workflow_task_graph["edges"] = edges;


        std::ofstream output(file_path);
        output << std::setw(4) << workflow_task_graph << std::endl;
        output.close();
    }


    /**
     * @brief Writes a JSON file containing host energy consumption information as a JSON array.
     * @description The JSON array has the following format:
     *
     * <pre>
     * [
     *      {
     *          hostname: <string>,
     *          pstates: [                 <-- if this host is a single core host, items in this list will be formatted as
     *              {                          the first item, else if this is a multi core host, items will be formatted as
     *                  pstate: <int>,         the second item
     *                  idle: <double>,
     *                  running: <double>,   <-- if single core host
     *                  speed: <double>
     *              },
     *              {
     *                  pstate: <int>,
     *                  idle: <double>,
     *                  one_core: <double>,  <-- if multi core host
     *                  all_cores: <double>, <-- if multi core host
     *                  speed: <double>
     *              } ...
     *          ],
     *          watts_off: <double>,
     *          pstate_trace: [
     *              {
     *                  time: <double>,
     *                  pstate: <int>
     *              }, ...
     *          ],
     *          consumed_energy_trace: [
     *              {
     *                  time: <double>,
     *                  joules: <double>
     *              }, ...
     *          ]
     *      }, ...
     * ]
     * </pre>
     *
     * @param file_path: the path to write the file
     *
     * @throws std::invalid_argument
     * @throws std::runtime_error
     */
    void SimulationOutput::dumpHostEnergyConsumptionJSON(std::string file_path) {

        if (file_path.empty()) {
            throw std::invalid_argument("SimulationOutput::dumpHostEnergyConsumptionJSON() requires a valid file_path");
        }

        try {

            simgrid::s4u::Engine *simgrid_engine = simgrid::s4u::Engine::get_instance();
            std::vector<simgrid::s4u::Host *> hosts = simgrid_engine->get_all_hosts();

            nlohmann::json hosts_energy_consumption_information;
            for (const auto &host : hosts) {
                nlohmann::json datum;

                datum["hostname"] = host->get_name();

                // for each pstate, we need to record the following:
                //     in the case of a single core hosts, then "Idle:Running"
                //     in the case of multi-core hosts, then "Idle:OneCore:AllCores"
                std::string watts_per_state_property_string = host->get_property("watt_per_state");
                std::vector<std::string> watts_per_state;
                boost::split(watts_per_state, watts_per_state_property_string, boost::is_any_of(","));

                for (size_t pstate = 0; pstate < watts_per_state.size(); ++pstate) {
                    std::vector<std::string> current_state_watts;
                    boost::split(current_state_watts, watts_per_state.at(pstate), boost::is_any_of(":"));

                    if (host->get_core_count() == 1) {
                        datum["pstates"].push_back({
                                                           {"pstate",  pstate},
                                                           {"speed",   host->get_pstate_speed(pstate)},
                                                           {"idle",    current_state_watts.at(0)},
                                                           {"running", current_state_watts.at(1)}
                                                   });
                    } else {
                        datum["pstates"].push_back({
                                                           {"pstate",    pstate},
                                                           {"speed",     host->get_pstate_speed(pstate)},
                                                           {"idle",      current_state_watts.at(0)},
                                                           {"one_core",  current_state_watts.at(1)},
                                                           {"all_cores", current_state_watts.at(2)}
                                                   });
                    }
                }

                const char *watt_off_value = host->get_property("watt_off");

                if (watt_off_value != nullptr) {
                    datum["watt_off"] = std::string(watt_off_value);
                }

                for (const auto &pstate_timestamp : this->getTrace<SimulationTimestampPstateSet>()) {
                    if (host->get_name() == pstate_timestamp->getContent()->getHostname()) {
                        datum["pstate_trace"].push_back({
                                                                {"time", pstate_timestamp->getDate()},
                                                                {"pstate", pstate_timestamp->getContent()->getPstate()}
                                                        });
                    }

                }

                for (const auto &energy_consumption_timestamp : this->getTrace<SimulationTimestampEnergyConsumption>()) {
                    if (host->get_name() == energy_consumption_timestamp->getContent()->getHostname()) {
                        datum["consumed_energy_trace"].push_back({
                                                                         {"time", energy_consumption_timestamp->getDate()},
                                                                         {"joules", energy_consumption_timestamp->getContent()->getConsumption()}
                                                                 });
                    }
                }

                hosts_energy_consumption_information.push_back(datum);
            }

            // std::cerr << hosts_energy_consumption_information.dump(4);

            std::ofstream output(file_path);
            output << std::setw(4) << hosts_energy_consumption_information << std::endl;
            output.close();

        } catch (std::runtime_error &e) {
            // the functions that get energy information catch any exceptions then throw runtime_errors
            std::cerr << e.what() << std::endl;
        }
    }

    /**
     * @brief Writes a JSON file containing all hosts, network links, and the routes between each host.
     * @description The JSON array has the following format:
     *
     * <pre>
     * {
     *    vertices: [
     *        {
     *            type: <"host">,
     *            id: <string>,
     *            flop_rate: <double (flops per second)>,
     *            memory: <double (bytes)>,
     *            cores: <unsigned_long>
     *        },
     *        {
     *            type: <"link">,
     *            id: <string>,
     *            bandwidth: <double (bytes per second)>,
     *            latency: <double (in seconds)>
     *        }, . . .
     *    ],
     *    edges: [
     *        {
     *            source: {
     *                type: <string>,
     *                id: <string>
     *            }
     *            target: {
     *                type: <string>,
     *                id: <string>
     *           }
     *        }, . . .
     *    ],
     *   routes: [
     *       {
     *           source: <string>,
     *           target: <string>,
     *           latency: <double (in seconds)>
     *           route: [
     *               link_id, ...
     *           ]
     *       }
     *   ],
     * }
     * </pre>
     *
     * @param file_path: the path to write the file
     *
     * @throws std::invalid_argument
     */
    void SimulationOutput::dumpPlatformGraphJSON(std::string file_path) {
        if (file_path.empty()) {
            throw std::invalid_argument("SimulationOutput::dumpPlatformGraphJSON() requires a valid file_path");
        }

        nlohmann::json platform_graph_json;

        simgrid::s4u::Engine *simgrid_engine = simgrid::s4u::Engine::get_instance();

        // Get all the hosts
        auto hosts = simgrid_engine->get_all_hosts();

        // get the by-cluster host information
        std::map<std::string, std::vector<std::string>> cluster_to_hosts = S4U_Simulation::getAllHostnamesByCluster();

        // Build a host-to-cluster map initialized with hostnames as cluster_ids
        std::map<std::string, std::string> host_to_cluster;
        for (auto const &h : hosts) {
            host_to_cluster[h->get_name()] = h->get_name();
        }
        // Update cluster_id value for those hosts that are in an actual cluster
        for (auto const &c : cluster_to_hosts) {
            std::string cluster_id = c.first;
            for (auto const &h : c.second) {
                host_to_cluster[h] = cluster_id;
            }
        }

        // add all hosts to the list of vertices
        for (const auto &host : hosts) {
            platform_graph_json["vertices"].push_back({
                                                           {"type", "host"},
                                                           {"id", host->get_name()},
                                                           {"cluster_id", host_to_cluster[host->get_name()]},
                                                           {"flop_rate", host->get_speed()},
                                                           {"memory", Simulation::getHostMemoryCapacity(host->get_name())},
                                                           {"cores", host->get_core_count()}
            });
        }

        // add all network links to the list of vertices
        std::vector<simgrid::s4u::Link *> links = simgrid_engine->get_all_links();
        for (const auto &link : links) {
            if (not (link->get_name() == "__loopback__")) { // Ignore loopback link
                platform_graph_json["vertices"].push_back({
                                                               {"type",      "link"},
                                                               {"id",        link->get_name()},
                                                               {"bandwidth", link->get_bandwidth()},
                                                               {"latency",   link->get_latency()}
                                                       });
            }
        }


        // add each route to the list of routes
        std::vector<simgrid::s4u::Link *> route_forward;
        std::vector<simgrid::s4u::Link *> route_backward;
        double route_forward_latency = 0;
        double route_backward_latency = 0;

        // for every combination of host pairs
        for (auto target = hosts.begin(); target != hosts.end(); ++target) {
            for (auto source = hosts.begin(); source != target; ++source) {
                nlohmann::json route_forward_json;

                // populate "route_forward" with an ordered list of network links along
                // the route between source and target
                (*source)->route_to(*target, route_forward, &route_forward_latency);

                // add the route from source to target to the json
                route_forward_json["source"] = (*source)->get_name();
                route_forward_json["target"] = (*target)->get_name();
                route_forward_json["latency"] = route_forward_latency;

                for (const auto &link : route_forward) {
                    route_forward_json["route"].push_back(link->get_name());
                }

                platform_graph_json["routes"].push_back(route_forward_json);

                // populate "route_backward" with an ordered list of network links along
                // the route between target and source; the "route_backward" could be different
                // so we need to add it if it is in fact different
                nlohmann::json route_backward_json;
                (*target)->route_to(*source, route_backward, &route_backward_latency);

                // check to see if the route from source to target is the same as from target to source
                bool is_route_equal = true;
                if (route_forward.size() == route_backward.size()) {
                    for (size_t i = 0; i < route_forward.size(); ++i) {
                        if (route_forward.at(i)->get_name() != route_backward.at(route_backward.size() - 1 - i)->get_name()) {
                            is_route_equal = false;
                            break;
                        }
                    }
                } else {
                    is_route_equal = false;
                }

                if (not is_route_equal) {
                    // add the route from target to source to the json
                    route_backward_json["source"] = (*target)->get_name();
                    route_backward_json["target"] = (*source)->get_name();
                    route_backward_json["latency"] = route_backward_latency;

                    for (const auto &link : route_backward) {
                        route_backward_json["route"].push_back(link->get_name());
                    }

                    platform_graph_json["routes"].push_back(route_backward_json);
                }

                // reset these values
                route_forward.clear();
                route_backward.clear();

                // reset these values
                route_forward_latency = 0;
                route_backward_latency = 0;
            }
        }

        // maintain a unique list of edges where edges are represented using the following string format:
        // <source_type>:<source_id>-<target_type>:<target_id> where type could be 'host' or 'link'
        std::unordered_set<std::string> edges;
        const std::string HOST("host");
        const std::string LINK("link");

        std::string source_string;
        std::string target_string;

        std::string source_id;
        std::string target_id;

        // for each route, add "host<-->link" and "link<-->link" connections
        for (nlohmann::json::iterator route_itr = platform_graph_json["routes"].begin(); route_itr != platform_graph_json["routes"].end(); ++route_itr) {

            source_id = (*route_itr)["source"].get<std::string>();
            source_string = HOST + ":" + source_id;

            target_id = (*route_itr)["route"].at(0).get<std::string>();
            target_string = LINK + ":" + target_id;

            // check that the undirected edge doesn't already exist in set of edges
            if (edges.find(source_string + "-" + target_string) == edges.end() and
                edges.find(target_string + "-" + source_string) == edges.end())
            {

                edges.insert(source_string + "-" + target_string);

                // add a graph link from the source host to the first network link
                platform_graph_json["edges"].push_back({
                                                               {"source", {
                                                                                  {"type", HOST},
                                                                                  {"id", source_id}
                                                                          }

                                                               },
                                                               {"target", {
                                                                                  {"type", LINK},
                                                                                  {"id", target_id}
                                                                          }
                                                               }
                                                       });
            }



            // add graph edges comprising only network links
            for (nlohmann::json::iterator link_itr = (*route_itr)["route"].begin(); link_itr != (*route_itr)["route"].end(); ++link_itr) {
                auto next_link_itr = link_itr + 1;

                if (next_link_itr != (*route_itr)["route"].end()) {

                    source_id = (*link_itr).get<std::string>();
                    source_string = LINK + ":" + source_id;

                    target_id = (*next_link_itr).get<std::string>();
                    target_string = LINK + ":" + target_id;

                    // check that the undirected edge doesn't already exist in set of edges
                    if (edges.find(source_string + "-" + target_string) == edges.end() and
                        edges.find(target_string + "-" + source_string) == edges.end()) {

                        edges.insert(source_string + "-" + target_string);

                        platform_graph_json["edges"].push_back({
                                                                       {"source", {
                                                                                          {"type", LINK},
                                                                                          {"id", source_id}
                                                                                  }
                                                                       },
                                                                       {"target", {
                                                                                          {"type", LINK},
                                                                                          {"id", target_id}
                                                                                  }
                                                                       }
                                                               });
                    }
                }
            }

            source_id = (*route_itr)["route"].at(((*route_itr)["route"].size()) - 1).get<std::string>();
            source_string = LINK + ":" + source_id;

            target_id = (*route_itr)["target"].get<std::string>();
            target_string = HOST + ":" + target_id;

            // check that the undirected edge doesn't already exist in set of edges
            if (edges.find(source_string + "-" + target_string) == edges.end() and
                edges.find(target_string + "-" + source_string) == edges.end()) {

                edges.insert(source_string + "-" + target_string);

                // add a graph link from the last link to the target host
                platform_graph_json["edges"].push_back({
                                                               {"source", {
                                                                                  {"type", "link"},
                                                                                  {"id", source_id}
                                                                          }
                                                               },
                                                               {"target", {
                                                                                  {"type", "host"},
                                                                                  {"id", target_id}
                                                                          }
                                                               }
                                                       });

            }
        }

        //std::cerr << platform_graph_json.dump(4) << std::endl;


        std::ofstream output(file_path);
        output << std::setw(4) << platform_graph_json << std::endl;
        output.close();
    }
};
