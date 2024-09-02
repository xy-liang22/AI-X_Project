/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mem/ruby/network/garnet/RoutingUnit.hh"

#include "base/cast.hh"
#include "base/compiler.hh"
#include "base/random.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/OutputUnit.hh"
#include "mem/ruby/network/garnet/Router.hh"
#include "mem/ruby/slicc_interface/Message.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RoutingUnit::RoutingUnit(Router *router)
{
    m_router = router;
    m_routing_table.clear();
    m_weight_table.clear();
}

void
RoutingUnit::addRoute(std::vector<NetDest>& routing_table_entry)
{
    if (routing_table_entry.size() > m_routing_table.size()) {
        m_routing_table.resize(routing_table_entry.size());
    }
    for (int v = 0; v < routing_table_entry.size(); v++) {
        m_routing_table[v].push_back(routing_table_entry[v]);
    }
}

void
RoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

bool
RoutingUnit::supportsVnet(int vnet, std::vector<int> sVnets)
{
    // If all vnets are supported, return true
    if (sVnets.size() == 0) {
        return true;
    }

    // Find the vnet in the vector, return true
    if (std::find(sVnets.begin(), sVnets.end(), vnet) != sVnets.end()) {
        return true;
    }

    // Not supported vnet
    return false;
}

/*
 * This is the default routing algorithm in garnet.
 * The routing table is populated during topology creation.
 * Routes can be biased via weight assignments in the topology file.
 * Correct weight assignments are critical to provide deadlock avoidance.
 */
int
RoutingUnit::lookupRoutingTable(int vnet, NetDest msg_destination)
{
    // First find all possible output link candidates
    // For ordered vnet, just choose the first
    // (to make sure different packets don't choose different routes)
    // For unordered vnet, randomly choose any of the links
    // To have a strict ordering between links, they should be given
    // different weights in the topology file

    int output_link = -1;
    int min_weight = INFINITE_;
    std::vector<int> output_link_candidates;
    int num_candidates = 0;

    // Identify the minimum weight among the candidate output links
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

        if (m_weight_table[link] <= min_weight)
            min_weight = m_weight_table[link];
        }
    }

    // Collect all candidate output links with this minimum weight
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

            if (m_weight_table[link] == min_weight) {
                num_candidates++;
                output_link_candidates.push_back(link);
            }
        }
    }

    if (output_link_candidates.size() == 0) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    // Randomly select any candidate output link
    int candidate = 0;
    if (!(m_router->get_net_ptr())->isVNetOrdered(vnet))
        candidate = rand() % num_candidates;

    output_link = output_link_candidates.at(candidate);
    return output_link;
}


void
RoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx;
    m_inports_idx2dirn[inport_idx]  = inport_dirn;
}

void
RoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx;
    m_outports_idx2dirn[outport_idx]  = outport_dirn;
}

// outportCompute() is called by the InputUnit
// It calls the routing table by default.
// A template for adaptive topology-specific routing algorithm
// implementations using port directions rather than a static routing
// table is provided here.

int
RoutingUnit::outportCompute(RouteInfo route, int inport,
                            PortDirection inport_dirn, flit *t_flit)
{
    int outport = -1;

    if (route.dest_router == m_router->get_id()) {

        // Multiple NIs may be connected to this router,
        // all with output port direction = "Local"
        // Get exact outport id from table
        outport = lookupRoutingTable(route.vnet, route.net_dest);
        return outport;
    }

    // Routing Algorithm set in GarnetNetwork.py
    // Can be over-ridden from command line using --routing-algorithm = 1
    RoutingAlgorithm routing_algorithm =
        (RoutingAlgorithm) m_router->get_net_ptr()->getRoutingAlgorithm();

    switch (routing_algorithm) {
        case TABLE_:  outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
        case XY_:     outport =
            outportComputeXY(route, inport, inport_dirn, t_flit); break;
        // any custom algorithm
        case CUSTOM_: outport =
            outportComputeCustom(route, inport, inport_dirn, t_flit); break;
        case DRAGONFLY_MINIMAL_: outport =
            outportComputeDragonflyMinimal(route, inport,
                                           inport_dirn, t_flit);
            break;
        case UGAL_: outport =
            outportComputeUGAL(route, inport, inport_dirn, t_flit); break;
        case VAL_: outport =
            outportComputeDragonflyVAL(route, inport, inport_dirn, t_flit);
            break;
        default: outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
    }

    assert(outport != -1);
    return outport;
}

// XY routing implemented using port directions
// Only for reference purpose in a Mesh
// By default Garnet uses the routing table
int
RoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn,
                              flit *t_flit)
{
    PortDirection outport_dirn = "Unknown";

    [[maybe_unused]] int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops > 0) {
        if (x_dirn) {
            assert(inport_dirn == "Local" || inport_dirn == "West");
            outport_dirn = "East";
        } else {
            assert(inport_dirn == "Local" || inport_dirn == "East");
            outport_dirn = "West";
        }
    } else if (y_hops > 0) {
        if (y_dirn) {
            // "Local" or "South" or "West" or "East"
            assert(inport_dirn != "North");
            outport_dirn = "North";
        } else {
            // "Local" or "North" or "West" or "East"
            assert(inport_dirn != "South");
            outport_dirn = "South";
        }
    } else {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == 0");
    }

    return m_outports_dirn2idx[outport_dirn];
}

// Template for implementing custom routing algorithm
// using port directions. (Example adaptive)
int
RoutingUnit::outportComputeCustom(RouteInfo route,
                                  int inport,
                                  PortDirection inport_dirn,
                                  flit *t_flit)
{
    // outportComputeRing
    PortDirection outport_dirn = "Unknown";
    int num_routers = m_router->get_net_ptr()->getNumRouters();
    assert(num_routers > 0);
    int my_id = m_router->get_id();
    int dest_id = route.dest_router;

    // printf("%d %d %d\n", my_id, dest_id, num_routers);

    bool dirn = !(((dest_id - my_id + num_routers) % num_routers) > (num_routers / 2)); // Go east if dirn = 1; Go west if dirn = 0
    int hops = dirn? ((dest_id - my_id + num_routers) % num_routers) : ((my_id - dest_id + num_routers) % num_routers);
    // printf("%d %d\n", dirn, hops);

    // already checked that in outportCompute() function
    assert(hops > 0);

    if (dirn) {
        assert(inport_dirn == "Local" || inport_dirn == "West");
        outport_dirn = "East";
    } else {
        assert(inport_dirn == "Local" || inport_dirn == "East");
        outport_dirn = "West";
    }

    return m_outports_dirn2idx[outport_dirn];
}

// minimal routing algorithm for dragonfly
int
RoutingUnit::outportComputeDragonflyMinimal(RouteInfo route,
                                    int inport,
                                    PortDirection inport_dirn,
                                    flit *t_flit)
{
    PortDirection outport_dirn = "Unknown";
    int num_routers = m_router->get_net_ptr()->getNumRouters();
    assert(num_routers > 0);

    int num_groups = m_router->get_net_ptr()->getNumGroups();
    int routers_per_group = m_router->get_net_ptr()->getRoutersPerGroup();
    int global_channels_per_router =
                m_router->get_net_ptr()->getGlobalChannelsPerRouter();

    int my_id = m_router->get_id();
    int dest_id = route.dest_router;
    // already checked that in outportCompute() function
    assert(my_id != dest_id);

    int group_cur = int(my_id / routers_per_group);
    int group_dst = int(dest_id / routers_per_group);

    if (group_cur != group_dst) {
        int group_gap = (group_dst-group_cur>0 ?
                (group_dst-group_cur-1) : (group_dst-group_cur-1+num_groups));
        int router_in_group_cur = group_gap / global_channels_per_router;
        int router_out = group_cur * routers_per_group + router_in_group_cur;
        if (my_id != router_out) {
            assert(inport_dirn == "Local");
            int outport_idx = router_out-my_id>0 ?
                (router_out-my_id-1) : (router_out-my_id-1+routers_per_group);
            outport_dirn = "Local" + std::to_string(outport_idx);
        } else {
            assert(inport_dirn == "Local" || inport_dirn.substr(0, 5) == "Local");
            int outport_idx = group_gap
                            - router_in_group_cur * global_channels_per_router;
            outport_dirn = "Global" + std::to_string(outport_idx);
        }
    } else {
        assert(inport_dirn == "Local" || inport_dirn.substr(0, 6) == "Global");
        int outport_idx = dest_id-my_id>0 ?
            (dest_id-my_id-1) : (dest_id-my_id-1+routers_per_group);
        outport_dirn = "Local" + std::to_string(outport_idx);
    }

    return m_outports_dirn2idx[outport_dirn];
}

// UGAL(Universal Globally-Adaptive Load-balanced) algorithm for dragonfly
int
RoutingUnit::outportComputeUGAL(RouteInfo route,
                                int inport,
                                PortDirection inport_dirn,
                                flit *t_flit)
{
    PortDirection outport_dirn = "Unknown";
    int num_routers = m_router->get_net_ptr()->getNumRouters();
    assert(num_routers > 0);

    int num_groups = m_router->get_net_ptr()->getNumGroups();
    int routers_per_group = m_router->get_net_ptr()->getRoutersPerGroup();
    int global_channels_per_router =
        m_router->get_net_ptr()->getGlobalChannelsPerRouter();

    int my_id = m_router->get_id();
    int group_mid = route.intermediate_group;
    int dest_id = route.dest_router;
    // already checked that in outportCompute() function
    assert(my_id != dest_id);

    int group_cur = int(my_id / routers_per_group);
    int group_dst = int(dest_id / routers_per_group);

    // if current router is the beginning
    // then randomly choose an intermediate group
    // estimate latency and choose path between MIN and VAL
    if (num_groups > 2 && group_cur != group_dst && my_id == route.src_router)
    {
        do {
            group_mid = random_mt.random<unsigned>(0, num_groups - 1);
        } while (group_mid == group_cur || group_mid == group_dst);
      
        // estimate latency
        int group_gap_dst_cur = (group_dst>group_cur ?
            (group_dst-group_cur-1) : (group_dst-group_cur-1+num_groups));
        int group_gap_mid_cur = (group_mid>group_cur ?
            (group_mid-group_cur-1) : (group_mid-group_cur-1+num_groups));
        // int group_gap_dst_mid = (group_dst>group_mid ?
        //     (group_dst-group_mid-1) : (group_dst-group_mid-1+num_groups));

        int router_min = group_cur * routers_per_group
                    + int(group_gap_dst_cur / global_channels_per_router);
        int router_val1 = group_cur * routers_per_group +
                    int(group_gap_mid_cur / global_channels_per_router);
        // int router_val2 = group_mid * routers_per_group +
        //             int(group_gap_dst_mid / global_channels_per_router);
        // int router_val3 = group_mid * routers_per_group + int((num_groups 
        //             - group_gap_mid_cur - 2) / global_channels_per_router);

        // int outport_idx_min = group_gap_dst_cur
        // - int(group_gap_dst_cur / global_channels_per_router)
        // * global_channels_per_router;
        // int outport_idx_val1 = group_gap_mid_cur
        // - int(group_gap_mid_cur / global_channels_per_router)
        // * global_channels_per_router;
        // int outport_idx_val2 = group_gap_dst_mid
        // - int(group_gap_dst_mid / global_channels_per_router)
        // * global_channels_per_router;

        // int latency_min_in = 0, latency_val1_in = 0, latency_val2_in = 0;
        // if (my_id != router_min) {
        //     int inport_idx_min = (my_id>router_min) ?
        //         (my_id-router_min-1) : (my_id-router_min+routers_per_group-1);
        //     int port_min_in = m_router->get_net_ptr()
        //         ->getRouterPtr(router_min)->getRoutingUnit()
        //         ->getPortIdx("Local" + std::to_string(inport_idx_min));
        //     latency_min_in = m_router->get_net_ptr()->getRouterPtr(router_min)
        //         ->getInputUnit(port_min_in)->get_link()->getBuffer()->getSize();
        // }

        // if (my_id != router_val1) {
        //     int inport_idx_val1 = (my_id>router_val1) ?
        //         (my_id-router_val1-1) : (my_id-router_val1+routers_per_group-1);
        //     int port_val1_in = m_router->get_net_ptr()
        //         ->getRouterPtr(router_val1)->getRoutingUnit()
        //         ->getPortIdx("Local" + std::to_string(inport_idx_val1));
        //     latency_val1_in = m_router->get_net_ptr()->getRouterPtr(router_val1)
        //         ->getInputUnit(port_val1_in)->get_link()->getBuffer()->getSize();
        // }

        // if (router_val3 != router_val2) {
        //     int inport_idx_val2 = (router_val3>router_val2) ?
        //         (router_val3-router_val2-1)
        //         : (router_val3-router_val2+routers_per_group-1);
        //     int port_val2_in = m_router->get_net_ptr()
        //         ->getRouterPtr(router_val2)->getRoutingUnit()
        //         ->getPortIdx("Local" + std::to_string(inport_idx_val2));
        //     latency_val2_in=m_router->get_net_ptr()->getRouterPtr(router_val2)
        //       ->getInputUnit(port_val2_in)->get_link()->getBuffer()->getSize();
        // }

        // int port_min = m_router->get_net_ptr()
        //     ->getRouterPtr(router_min)->getRoutingUnit()
        //     ->getPortIdx("Global" + std::to_string(outport_idx_min));
        // int port_val1 = m_router->get_net_ptr()
        //     ->getRouterPtr(router_val1)->getRoutingUnit()
        //     ->getPortIdx("Global" + std::to_string(outport_idx_val1));
        // int port_val2 = m_router->get_net_ptr()
        //     ->getRouterPtr(router_val2)->getRoutingUnit()
        //     ->getPortIdx("Global" + std::to_string(outport_idx_val2));

        // int latency_min_out = m_router->get_net_ptr()->getRouterPtr(router_min)
        // ->getOutputUnit(port_min)->get_link()->getBuffer()->getSize();
        // int latency_val1_out=m_router->get_net_ptr()->getRouterPtr(router_val1)
        // ->getOutputUnit(port_val1)->get_link()->getBuffer()->getSize();
        // int latency_val2_out=m_router->get_net_ptr()->getRouterPtr(router_val2)
        // ->getOutputUnit(port_val2)->get_link()->getBuffer()->getSize();

        // int min = latency_min_in + latency_min_out + global_latency
        //         + local_latency * 2 + router_latency * 3;
        // int val = latency_val1_in + latency_val1_out + latency_val2_in
        //         + latency_val2_out + global_latency * 2
        //         + local_latency * 3 + router_latency * 5;

        int inport_idx_min = (my_id>router_min) ?
            (my_id-router_min-1) : (my_id-router_min+routers_per_group-1);
        int port_min_in = m_router->get_net_ptr()
            ->getRouterPtr(router_min)->getRoutingUnit()
            ->getPortIdx("Local" + std::to_string(inport_idx_min));
        std::vector<unsigned int> vc_load_min = m_router->get_net_ptr()->getRouterPtr(router_min)
            ->getInputUnit(port_min_in)->get_link()->getVcLoad();
        int load_min = std::accumulate(vc_load_min.begin(), vc_load_min.end(), 0u);

        int inport_idx_val1 = (my_id>router_val1) ?
            (my_id-router_val1-1) : (my_id-router_val1+routers_per_group-1);
        int port_val1_in = m_router->get_net_ptr()
            ->getRouterPtr(router_val1)->getRoutingUnit()
            ->getPortIdx("Local" + std::to_string(inport_idx_val1));
        std::vector<unsigned int> vc_load_val1 = m_router->get_net_ptr()->getRouterPtr(router_val1)
            ->getInputUnit(port_val1_in)->get_link()->getVcLoad();
        int load_val1 = std::accumulate(vc_load_val1.begin(), vc_load_val1.end(), 0u);

        // std::vector<unsigned int> outvc_load_min = m_router->get_net_ptr()->getRouterPtr(router_min)
        //     ->getOutputUnit(port_min)->get_link()->getVcLoad();
        // int load_min_out = std::accumulate(outvc_load_min.begin(), outvc_load_min.end(), 0u);

        // std::vector<unsigned int> outvc_load_val1 = m_router->get_net_ptr()->getRouterPtr(router_val1)
        //     ->getOutputUnit(port_val1)->get_link()->getVcLoad();
        // int load_val1_out = std::accumulate(outvc_load_val1.begin(), outvc_load_val1.end(), 0u);

        int q_m_vc = vc_load_min[3], q_nm_vc = vc_load_val1[0];

        // choose path between MIN and VAL
        if ((load_val1 * 5 < load_min * 3 || router_min == router_val1) && (q_nm_vc * 5 < q_m_vc * 3 || router_min != router_val1)) {
            route.intermediate_group = group_mid;
            t_flit->set_route(route);
        } else {
            group_mid = -1;
        }
        // if ((load_val1 + load_val1_out * 1.0 * global_channels_per_router / routers_per_group) * 5 < (load_min + load_min_out * 1.0 * global_channels_per_router / routers_per_group) * 3) {
        //     route.intermediate_group = group_mid;
        //     t_flit->set_route(route);
        // } else {
        //     group_mid = -1;
        // }
    }

    // if already in the intermediate group
    // then the following path is the same as a minimal path
    if (group_mid != -1 && group_cur == group_mid) {
        route.intermediate_group = -1;
        t_flit->set_route(route);
        group_mid = -1;
    }

    // then group_mid != -1 means group_cur is the source group
    if (group_mid != -1) {
        // compute minimal route to group_mid
        // it is equivalent to set group_dst = group_mid
        // and compute minimal route to group_dst since group_cur != group_mid
        // no need to reset dest_id since it has no use
        // until computing route within group_dst
        group_dst = group_mid;
    }

    // compute outport
    if (group_cur != group_dst) {
        int group_gap = (group_dst>group_cur ?
                (group_dst-group_cur-1) : (group_dst-group_cur-1+num_groups));
        int router_in_group_cur = group_gap / global_channels_per_router;
        int router_out = group_cur * routers_per_group + router_in_group_cur;
        if (my_id != router_out) {
            assert(inport_dirn=="Local"||inport_dirn.substr(0, 6)=="Global");
            int outport_idx = router_out>my_id ?
                (router_out-my_id-1) : (router_out-my_id-1+routers_per_group);
            outport_dirn = "Local" + std::to_string(outport_idx);
        } else {
            // no assertion since all inport_dirn are possible
            int outport_idx = group_gap -
                    router_in_group_cur * global_channels_per_router;
            outport_dirn = "Global" + std::to_string(outport_idx);
        }
    } else {
        assert(inport_dirn == "Local"||inport_dirn.substr(0, 6) == "Global");
        int outport_idx = dest_id>my_id ?
                (dest_id-my_id-1) : (dest_id-my_id-1+routers_per_group);
        outport_dirn = "Local" + std::to_string(outport_idx);
    }

    return m_outports_dirn2idx[outport_dirn];
}

int
RoutingUnit::outportComputeDragonflyVAL(RouteInfo route,
                                        int inport,
                                        PortDirection inport_dirn,
                                        flit *t_flit)
{
    PortDirection outport_dirn = "Unknown";
    int num_routers = m_router->get_net_ptr()->getNumRouters();
    assert(num_routers > 0);

    int num_groups = m_router->get_net_ptr()->getNumGroups();
    int routers_per_group = m_router->get_net_ptr()->getRoutersPerGroup();
    int global_channels_per_router =
        m_router->get_net_ptr()->getGlobalChannelsPerRouter();

    int my_id = m_router->get_id();
    int group_mid = route.intermediate_group;
    int dest_id = route.dest_router;
    // already checked that in outportCompute() function
    assert(my_id != dest_id);

    int group_cur = int(my_id / routers_per_group);
    int group_dst = int(dest_id / routers_per_group);

    // if current router is the beginning
    // then randomly choose an intermediate group
    // estimate latency and choose path between MIN and VAL
    if (num_groups > 2 && group_cur != group_dst && my_id == route.src_router)
    {
        do {
            group_mid = random_mt.random<unsigned>(0, num_groups - 1);
        } while (group_mid == group_cur || group_mid == group_dst);

        route.intermediate_group = group_mid;
        t_flit->set_route(route);
    }

    // if already in the intermediate group
    // then the following path is the same as a minimal path
    if (group_mid != -1 && group_cur == group_mid) {
        route.intermediate_group = -1;
        t_flit->set_route(route);
        group_mid = -1;
    }

    // then group_mid != -1 means group_cur is the source group
    if (group_mid != -1) {
        // compute minimal route to group_mid
        // it is equivalent to set group_dst = group_mid
        // and compute minimal route to group_dst since group_cur != group_mid
        // no need to reset dest_id since it has no use
        // until computing route within group_dst
        group_dst = group_mid;
    }

    // compute outport
    if (group_cur != group_dst) {
        int group_gap = (group_dst>group_cur ?
                (group_dst-group_cur-1) : (group_dst-group_cur-1+num_groups));
        int router_in_group_cur = group_gap / global_channels_per_router;
        int router_out = group_cur * routers_per_group + router_in_group_cur;
        if (my_id != router_out) {
            assert(inport_dirn=="Local"||inport_dirn.substr(0, 6)=="Global");
            int outport_idx = router_out>my_id ?
                (router_out-my_id-1) : (router_out-my_id-1+routers_per_group);
            outport_dirn = "Local" + std::to_string(outport_idx);
        } else {
            // no assertion since all inport_dirn are possible
            int outport_idx = group_gap -
                    router_in_group_cur * global_channels_per_router;
            outport_dirn = "Global" + std::to_string(outport_idx);
        }
    } else {
        assert(inport_dirn == "Local"||inport_dirn.substr(0, 6) == "Global");
        int outport_idx = dest_id>my_id ?
                (dest_id-my_id-1) : (dest_id-my_id-1+routers_per_group);
        outport_dirn = "Local" + std::to_string(outport_idx);
    }

    return m_outports_dirn2idx[outport_dirn];
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
