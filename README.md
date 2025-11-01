# Detection-and-Optimized-Self-Healing-of-Atriculation-Points-
Distributed Cut Vertex Detection and Self-Healing Security for RPL-based WSNs

This project is a C implementation of a proactive network hardening algorithm for the Contiki-NG operating system. It is designed to identify and eliminate topological vulnerabilities in Wireless Sensor Networks (WSNs) that use the RPL routing protocol.

1. Problem Statement

The RPL routing protocol, standard in many IoT networks, organizes nodes into a tree-like topology (a DODAG). This structure is efficient but inherently fragile, as it creates numerous cut vertices (or articulation points).

A cut vertex is a node that, if it fails or is compromised, will split the network into two or more disconnected components. An attacker can exploit this by launching a "cut-vertex-aware" black-hole or grey-hole attack, disconnecting an entire subtree from the network root and causing catastrophic data loss.

2. Proposed Solution

This algorithm provides a proactive "self-healing" mechanism by transforming the network from a vulnerable tree-like structure into a robust, biconnected graph (a graph with no cut vertices).

It operates in two main phases:

Analysis Phase: The algorithm ingests the network topology and uses Tarjan's algorithm for biconnected components (a linear-time $O(V+E)$ algorithm) to identify all cut vertices and partition the graph into its constituent "blocks."

Healing (Meshification) Phase: It analyzes the block-cut tree (BCT) structure to find all "leaf blocks" (blocks connected to the network by only one cut vertex). It then adds a mathematically minimal set of $\lceil k/2 \rceil$ redundant edges to connect pairs of these leaf blocks, "stitching" the graph together.

This process eliminates all single points of failure, ensuring that the failure or compromise of any single node cannot disconnect the network.

3. Features

Cut Vertex Detection: Implements Tarjan's efficient $O(V+E)$ algorithm.

Optimal Meshification: Calculates and adds the minimal number of redundant edges to achieve biconnectivity.

Performance Statistics: Generates a detailed report in the console, including:

Execution time breakdown (ms) for each phase.

Network overhead analysis (edge overhead %, degree increase).

Algorithm efficiency (ms/node, ms/edge).

Final validation (Initial vs. Final cut vertex count).

Visualization: Automatically generates .dot graph files and uses Graphviz to render .png images of the network before and after hardening.

Blue Node: Root

Red Nodes: Cut Vertices (in the "before" image)

Green Edges: Newly added redundant edges (in the "after" image)

4. Getting Started

Prerequisites

Contiki-NG: You must have the Contiki-NG source code cloned on your system, as this project is built using its libraries and build system.

C Compiler: A C compiler like gcc (comes with build-essential on Linux).

Graphviz: The sfdp tool is required for generating the .png visualizations.

# On Debian/Ubuntu
sudo apt-get update
sudo apt-get install graphviz


Installation & Setup

Navigate to your Contiki-NG directory (e.g., ~/contiki-ng).

Create a new project directory within examples/, for example:

mkdir examples/rpl-mesh-security


Place the rpl_cutvertex_detection.c file inside this new directory.

Create a Makefile in the same directory (examples/rpl-mesh-security/) with the following content:

CONTIKI_PROJECT = rpl_cutvertex_detection
all: $(CONTIKI_PROJECT)

CONTIKI = ../..
include $(CONTIKI)/Makefile.include


5. How to Run

Navigate to your project directory:

cd examples/rpl-mesh-security


Compile the project for the native target:

make TARGET=native


Run the simulation (this will default to 50 nodes):

./rpl_cutvertex_detection.native


Run with a specific node count (e.g., 100 nodes):

./rpl_cutvertex_detection.native 100


6. Expected Output

First, the program will log its progress.

[INFO: CUT-MESH] Starting meshification...
[INFO: CUT-MESH] Using node count: 100
[INFO: CUT-MESH] Generating random topology with 100 nodes...
[INFO: CUT-MESH] Generated: 100 nodes, 139 edges (avg degree: 2.78)
[INFO: CUT-MESH] Initial: 25 cut vertices, 38 blocks
[INFO: CUT-MESH] Exported dodag_old.dot
[INFO: CUT-MESH] Found 37 leaf blocks (need 19 edges)
[INFO: CUT-MESH] Added 19 optimal redundant edges
[INFO: CUT-MESH] Exported dodag_final.dot
[INFO: CUT-MESH] Generating PNG images...
[INFO: CUT-MESH] SUCCESS: Generated PNG files (112.45 ms)


Then, it will print the final statistics report:

╔════════════════════════════════════════════════════════════╗
║           MESHIFICATION RESULTS & STATISTICS             ║
╠════════════════════════════════════════════════════════════╣
║ Timestamp: 2025-09-20 14:30:01                           ║
╠════════════════════════════════════════════════════════════╣
║ NETWORK CONFIGURATION                                        ║
╠════════════════════════════════════════════════════════════╣
║ Network Size:                 100 nodes                   ║
║ Max Supported:               1000 nodes                   ║
║ Connection Probability:      0.15                       ║
╠════════════════════════════════════════════════════════════╣
║ TOPOLOGY METRICS                                             ║
╠════════════════════════════════════════════════════════════╣
║ Original Edges:               139                       ║
║ Redundant Edges Added:        19                       ║
║ Total Edges (Final):         158                       ║
║ Edge Overhead:              13.67%                      ║
╠════════════════════════════════════════════════════════════╣
║ DEGREE DISTRIBUTION                                          ║
╠════════════════════════════════════════════════════════════╣
║ Avg Degree (Initial):       2.78                       ║
║ Avg Degree (Final):         3.16                       ║
║ Max Degree (Final):            7                       ║
║ Degree Increase:            13.67%                      ║
╠════════════════════════════════════════════════════════════╣
║ BICONNECTIVITY ANALYSIS                                      ║
╠════════════════════════════════════════════════════════════╣
║ Biconnected Components:        1                       ║
║ Leaf Blocks:                 37                       ║
║ Cut Vertices (Initial):       25                       ║
║ Cut Vertices (Final):          0                       ║
║ Cut Vertices Eliminated:     25 (100.0%)                ║
╠════════════════════════════════════════════════════════════╣
║ EXECUTION TIME BREAKDOWN                                     ║
╠════════════════════════════════════════════════════════════╣
║ Topology Generation:          0.15 ms                   ║
║ Initial Analysis (Tarjan):    0.11 ms                   ║
║ Redundancy Addition:          0.04 ms                   ║
║ Final Analysis (Tarjan):      0.10 ms                   ║
║ DOT Export:                   0.52 ms                   ║
║ ─────────────────────────────────────────────────────────  ║
║ TOTAL EXECUTION TIME:          1.52 ms                   ║
╠════════════════════════════════════════════════════════════╣
║ ALGORITHM EFFICIENCY                                         ║
╠════════════════════════════════════════════════════════════╣
║ Time per Node:               0.015 ms/node                ║
║ Time per Edge:               0.010 ms/edge                ║
║ Theoretical Complexity:     O(V + E)                     ║
╠════════════════════════════════════════════════════════════╣
║ OUTPUT FILES                                                 ║
╠════════════════════════════════════════════════════════════╣
║ • dodag_old.dot   (Original topology)                      ║
║ • dodag_final.dot (Meshified topology)                     ║
║ • dodag_old.png   (Original visualization)                 ║
║ • dodag_final.png (Meshified visualization)                ║
╚════════════════════════════════════════════════════════════╝


7. Visualization

After the program finishes, check your project directory. You will find four new files. The most important are the PNG images:

dodag_old.png: Shows the initial, vulnerable graph. Cut vertices are colored red.

dodag_final.png: Shows the final, hardened graph. New redundant edges are colored green, and there are no more red nodes.

8. Authors

SRI SHESHAADHRI R (22BCE1172)

MIRDULAA BALAJI (22BAI1284)

SIDHARTH P J (22BRS1086)

Under the guidance of Dr. Radha R.

9. License

This project is licensed under the MIT License.
