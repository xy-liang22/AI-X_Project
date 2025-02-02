# Copyright (c) 2010 Advanced Micro Devices, Inc.
#               2016 Georgia Institute of Technology
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from m5.params import *
from m5.objects import *

from common import FileSystemConfig

from topologies.BaseTopology import SimpleTopology

# Creates a generic Dragonfly assuming an equal number of cache
# and directory controllers.
# XY routing is enforced (using link weights)
# to guarantee deadlock freedom.


class Dragonfly(SimpleTopology):
    description = "Dragonfly"

    def __init__(self, controllers):
        self.nodes = controllers

    # Makes a generic dragonfly
    # assuming an equal number of cache and directory cntrls

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        nodes = self.nodes

        num_routers = options.num_cpus
        routers_per_group = options.routers_per_group
        global_channels_per_router = options.global_channels_per_router
        num_groups = routers_per_group * global_channels_per_router + 1
        assert routers_per_group * num_groups == num_routers

        # default values for link latency and router latency.
        # Can be over-ridden on a per link/router basis
        link_latency = options.link_latency  # used by simple and garnet
        router_latency = options.router_latency  # only used by garnet

        # There must be an evenly divisible number of cntrls to routers
        # Also, obviously the number or rows must be <= the number of routers
        cntrls_per_router, remainder = divmod(len(nodes), num_routers)

        # Create the routers in the dragonfly
        routers = [
            Router(router_id=i, latency=router_latency)
            for i in range(num_routers)
        ]
        network.routers = routers

        # link counter to set unique link ids
        link_count = 0

        # Add all but the remainder nodes to the list of nodes to be uniformly
        # distributed across the network.
        network_nodes = []
        remainder_nodes = []
        for node_index in range(len(nodes)):
            if node_index < (len(nodes) - remainder):
                network_nodes.append(nodes[node_index])
            else:
                remainder_nodes.append(nodes[node_index])

        # Connect each node to the appropriate router
        ext_links = []
        for (i, n) in enumerate(network_nodes):
            cntrl_level, router_id = divmod(i, num_routers)
            assert cntrl_level < cntrls_per_router
            ext_links.append(
                ExtLink(
                    link_id=link_count,
                    ext_node=n,
                    int_node=routers[router_id],
                    latency=link_latency,
                )
            )
            link_count += 1

        # Connect the remaining nodes to router 0.  These should only be
        # DMA nodes.
        for (i, node) in enumerate(remainder_nodes):
            # assert node.type == "DMA_Controller"
            assert i < remainder
            ext_links.append(
                ExtLink(
                    link_id=link_count,
                    ext_node=node,
                    int_node=routers[0],
                    latency=link_latency,
                )
            )
            link_count += 1

        network.ext_links = ext_links

        # Create the dragonfly links.
        int_links = []

        # Create the global links.
        for group_src in range(num_groups):
            for router_in_group_src in range(routers_per_group):
                router_out = group_src * routers_per_group + router_in_group_src
                for idx_outport in range(global_channels_per_router):
                    group_dst = (group_src + router_in_group_src * global_channels_per_router + idx_outport + 1) % num_groups
                    # notice that group_dst != group_src
                    router_in_group_dst, idx_inport = divmod((group_src - group_dst - 1) % num_groups, global_channels_per_router)
                    # notice that 0 <= (group_src - group_dst - 1) % num_groups <= num_groups - 2 
                    router_in = group_dst * routers_per_group + router_in_group_dst
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[router_out],
                            dst_node=routers[router_in],
                            src_outport="Global" + str(idx_outport),
                            dst_inport="Global" + str(idx_inport),
                            latency=link_latency,
                            weight=2,
                        )
                    )
                    link_count += 1

        # Create the local links in each group.
        for group in range(num_groups):
            for router_out_in_group in range(routers_per_group):
                router_out = group * routers_per_group + router_out_in_group
                for idx_outport in range(routers_per_group - 1):
                    router_in = group * routers_per_group + ((router_out_in_group + idx_outport + 1) % routers_per_group)
                    idx_inport = (router_out - router_in - 1) % routers_per_group
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[router_out],
                            dst_node=routers[router_in],
                            src_outport="Local" + str(idx_outport),
                            dst_inport="Local" + str(idx_inport),
                            latency=link_latency,
                            weight=1,
                        )
                    )
                    link_count += 1

        network.int_links = int_links

    # Register nodes with filesystem
    def registerTopology(self, options):
        for i in range(options.num_cpus):
            FileSystemConfig.register_node(
                [i], MemorySize(options.mem_size) // options.num_cpus, i
            )
