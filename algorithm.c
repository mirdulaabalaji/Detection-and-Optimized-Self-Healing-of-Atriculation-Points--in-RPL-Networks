/* rpl_cutvertex_detection.c
 *
 * Enhanced version with timing and detailed performance metrics
 * Suitable for Contiki-NG embedded environment
 */

#include "contiki.h"
#include "sys/log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define LOG_MODULE "CUT-MESH"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Adjust MAX_NODES based on your needs (50-200 recommended for stability) */
#define MAX_NODES 1000
#define MAX_NEIGHBORS 80
#define MAX_BLOCKS 1250

/* External variables for command-line args */
extern int contiki_argc;
extern char **contiki_argv;

/* Configuration */
static int n_nodes = 50;
static double connection_prob = 0.15;

/* Graph structures - static allocation */
static int neighbors[MAX_NODES][MAX_NEIGHBORS];
static int degree[MAX_NODES];
static char exists_edge[MAX_NODES][MAX_NODES];

/* Tarjan arrays */
static int disc[MAX_NODES];
static int low[MAX_NODES];
static int parent_tarjan[MAX_NODES];
static char visited[MAX_NODES];
static int time_dfs;
static char is_cut[MAX_NODES];

/* Edge stack for biconnected components */
typedef struct {
  int u, v;
} Edge;

static Edge edge_stack[MAX_NODES * 10];
static int stack_top = 0;

/* Biconnected components - compact representation */
static int block_nodes[MAX_BLOCKS][MAX_NODES];
static int block_size[MAX_BLOCKS];
static int num_blocks = 0;

/* Block-cut tree */
static char is_leaf_block[MAX_BLOCKS];
static int leaf_blocks[MAX_BLOCKS];
static int num_leaf_blocks = 0;

/* Redundant edge tracking */
static char redundant_edge[MAX_NODES][MAX_NODES];

/* Statistics */
static int original_edges = 0;
static int redundant_edges_added = 0;

/* Timing statistics */
static double time_topology_gen = 0.0;
static double time_initial_analysis = 0.0;
static double time_redundancy_addition = 0.0;
static double time_final_analysis = 0.0;
static double time_dot_export = 0.0;
static double time_total = 0.0;

/* Additional metrics */
static int initial_cut_vertices = 0;
static int final_cut_vertices = 0;
static double avg_degree_initial = 0.0;
static double avg_degree_final = 0.0;
static int max_degree_initial = 0;
static int max_degree_final = 0;

/* ----------------- Timing utilities ------------------ */

double get_time_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

/* ----------------- Initialization ------------------ */

void init_arrays(void) {
  memset(degree, 0, sizeof(degree));
  memset(exists_edge, 0, sizeof(exists_edge));
  memset(neighbors, 0, sizeof(neighbors));
  memset(redundant_edge, 0, sizeof(redundant_edge));
  memset(block_size, 0, sizeof(block_size));
  memset(is_leaf_block, 0, sizeof(is_leaf_block));
  original_edges = 0;
  redundant_edges_added = 0;
  num_blocks = 0;
  stack_top = 0;
}

/* ----------------- Graph generation ------------------ */

void generate_random_topology(void) {
  unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)clock();
  srand(seed);
  
  LOG_INFO("Generating random topology with %d nodes...\n", n_nodes);
  
  /* Step 1: Create tree backbone */
  for(int i=1; i<n_nodes; i++) {
    int parent = rand() % i;
    
    if(degree[i] < MAX_NEIGHBORS && degree[parent] < MAX_NEIGHBORS) {
      neighbors[i][degree[i]++] = parent;
      neighbors[parent][degree[parent]++] = i;
      exists_edge[i][parent] = exists_edge[parent][i] = 1;
      original_edges++;
    }
  }
  
  /* Step 2: Add random cross-edges */
  int target_edges = (int)(n_nodes * connection_prob * 10);
  int attempts = 0;
  int max_attempts = target_edges * 3;
  
  while(original_edges < target_edges && attempts < max_attempts) {
    int u = rand() % n_nodes;
    int v = rand() % n_nodes;
    
    if(u != v && !exists_edge[u][v] && degree[u] < MAX_NEIGHBORS && degree[v] < MAX_NEIGHBORS) {
      int dist = abs(u - v);
      double prob = 1.0 / (1.0 + dist / 10.0);
      
      if((double)rand() / RAND_MAX < prob) {
        neighbors[u][degree[u]++] = v;
        neighbors[v][degree[v]++] = u;
        exists_edge[u][v] = exists_edge[v][u] = 1;
        original_edges++;
      }
    }
    attempts++;
  }
  
  LOG_INFO("Generated: %d nodes, %d edges (avg degree: %.2f)\n", 
           n_nodes, original_edges, 2.0 * original_edges / n_nodes);
}

/* ----------------- Tarjan DFS ------------------ */

void tarjan_dfs_bicomp(int u) {
  visited[u] = 1;
  disc[u] = low[u] = ++time_dfs;
  int children = 0;

  for(int i=0; i<degree[u]; i++){
    int v = neighbors[u][i];
    if(!visited[v]) {
      children++;
      parent_tarjan[v] = u;
      
      if(stack_top < MAX_NODES * 10 - 1) {
        edge_stack[stack_top].u = u;
        edge_stack[stack_top].v = v;
        stack_top++;
      }
      
      tarjan_dfs_bicomp(v);
      
      if(low[v] < low[u]) low[u] = low[v];

      if( (parent_tarjan[u] == -1 && children > 1) ||
          (parent_tarjan[u] != -1 && low[v] >= disc[u]) ) {
        is_cut[u] = 1;
        
        /* Pop edges and form component */
        if(num_blocks < MAX_BLOCKS) {
          char in_block[MAX_NODES] = {0};
          Edge e;
          do {
            if(stack_top <= 0) break;
            stack_top--;
            e = edge_stack[stack_top];
            
            if(!in_block[e.u]) {
              in_block[e.u] = 1;
              block_nodes[num_blocks][block_size[num_blocks]++] = e.u;
            }
            if(!in_block[e.v]) {
              in_block[e.v] = 1;
              block_nodes[num_blocks][block_size[num_blocks]++] = e.v;
            }
          } while(!(e.u == u && e.v == v) && stack_top > 0);
          
          num_blocks++;
        }
      }
    } else if(v != parent_tarjan[u] && disc[v] < disc[u]) {
      if(stack_top < MAX_NODES * 10 - 1) {
        edge_stack[stack_top].u = u;
        edge_stack[stack_top].v = v;
        stack_top++;
      }
      
      if(disc[v] < low[u]) low[u] = disc[v];
    }
  }
  
  /* Handle remaining edges for root */
  if(parent_tarjan[u] == -1 && children <= 1 && stack_top > 0 && num_blocks < MAX_BLOCKS) {
    char in_block[MAX_NODES] = {0};
    while(stack_top > 0) {
      stack_top--;
      Edge e = edge_stack[stack_top];
      
      if(!in_block[e.u]) {
        in_block[e.u] = 1;
        block_nodes[num_blocks][block_size[num_blocks]++] = e.u;
      }
      if(!in_block[e.v]) {
        in_block[e.v] = 1;
        block_nodes[num_blocks][block_size[num_blocks]++] = e.v;
      }
    }
    num_blocks++;
  }
}

void find_biconnected_components(void) {
  memset(visited, 0, sizeof(visited));
  memset(parent_tarjan, -1, sizeof(parent_tarjan));
  memset(disc, 0, sizeof(disc));
  memset(low, 0, sizeof(low));
  memset(is_cut, 0, sizeof(is_cut));
  memset(block_size, 0, sizeof(block_size));
  
  num_blocks = 0;
  stack_top = 0;
  time_dfs = 0;
  
  for(int i=0; i<n_nodes; i++){
    if(!visited[i]) {
      parent_tarjan[i] = -1;
      tarjan_dfs_bicomp(i);
    }
  }
}

/* ----------------- Optimal edge addition ------------------ */

void identify_leaf_blocks(void) {
  num_leaf_blocks = 0;
  memset(is_leaf_block, 0, sizeof(is_leaf_block));
  
  for(int b=0; b<num_blocks; b++) {
    int cut_count = 0;
    for(int i=0; i<block_size[b]; i++) {
      int node = block_nodes[b][i];
      if(is_cut[node]) cut_count++;
    }
    
    if(cut_count == 1) {
      is_leaf_block[b] = 1;
      leaf_blocks[num_leaf_blocks++] = b;
    }
  }
}

int find_non_cut_in_block(int block) {
  for(int i=0; i<block_size[block]; i++) {
    int node = block_nodes[block][i];
    if(!is_cut[node]) return node;
  }
  return (block_size[block] > 0) ? block_nodes[block][0] : -1;
}

void add_optimal_redundant_edges(void) {
  identify_leaf_blocks();
  
  LOG_INFO("Found %d leaf blocks (need %d edges)\n", 
           num_leaf_blocks, (num_leaf_blocks + 1) / 2);
  
  redundant_edges_added = 0;
  
  for(int i=0; i<num_leaf_blocks; i+=2) {
    int block1 = leaf_blocks[i];
    int block2 = (i+1 < num_leaf_blocks) ? leaf_blocks[i+1] : leaf_blocks[0];
    
    int node1 = find_non_cut_in_block(block1);
    int node2 = find_non_cut_in_block(block2);
    
    if(node1 != -1 && node2 != -1 && node1 != node2 && !exists_edge[node1][node2]) {
      if(degree[node1] < MAX_NEIGHBORS && degree[node2] < MAX_NEIGHBORS) {
        neighbors[node1][degree[node1]++] = node2;
        neighbors[node2][degree[node2]++] = node1;
        exists_edge[node1][node2] = exists_edge[node2][node1] = 1;
        redundant_edge[node1][node2] = redundant_edge[node2][node1] = 1;
        redundant_edges_added++;
      }
    }
  }
  
  LOG_INFO("Added %d optimal redundant edges\n", redundant_edges_added);
}

/* ----------------- Compute metrics ------------------ */

void compute_network_metrics(void) {
  /* Count cut vertices */
  initial_cut_vertices = 0;
  final_cut_vertices = 0;
  for(int i=0; i<n_nodes; i++) {
    if(is_cut[i]) final_cut_vertices++;
  }
  
  /* Compute average and max degree */
  int sum_degree = 0;
  max_degree_initial = 0;
  max_degree_final = 0;
  
  for(int i=0; i<n_nodes; i++) {
    sum_degree += degree[i];
    if(degree[i] > max_degree_final) max_degree_final = degree[i];
  }
  
  avg_degree_final = (double)sum_degree / n_nodes;
  
  /* Initial avg degree is calculated from original_edges */
  avg_degree_initial = (2.0 * original_edges) / n_nodes;
}

/* ----------------- Export ------------------ */

void export_dot_graph(const char *fname, int show_redundant) {
  FILE *f = fopen(fname, "w");
  if(!f) {
    LOG_ERR("Failed to open %s\n", fname);
    return;
  }
  
  fprintf(f, "graph DODAG {\n");
  fprintf(f, "  layout=sfdp; K=0.5; overlap=prism; splines=true;\n");
  fprintf(f, "  node [shape=circle,width=0.3,fixedsize=true,fontsize=8];\n");
  
  for(int u=0; u<n_nodes; u++) {
    if(u == 0) {
      fprintf(f, "  %d [color=blue,style=filled,fillcolor=lightblue];\n", u);
    } else if(is_cut[u]) {
      fprintf(f, "  %d [color=red,style=filled,fillcolor=pink];\n", u);
    }
  }
  
  char shown[MAX_NODES][MAX_NEIGHBORS] = {0};
  
  for(int u=0; u<n_nodes; u++) {
    for(int i=0; i<degree[u]; i++) {
      int v = neighbors[u][i];
      if(u < v) {
        int already_shown = 0;
        for(int j=0; j<MAX_NEIGHBORS; j++) {
          if(shown[u][j] == v) { already_shown = 1; break; }
        }
        
        if(!already_shown) {
          if(show_redundant && redundant_edge[u][v]) {
            fprintf(f, "  %d -- %d [color=\"#00AA00\",penwidth=2.0];\n", u, v);
          } else {
            fprintf(f, "  %d -- %d [color=black];\n", u, v);
          }
          
          for(int j=0; j<MAX_NEIGHBORS; j++) {
            if(shown[u][j] == 0) { shown[u][j] = v; break; }
          }
        }
      }
    }
  }
  
  fprintf(f, "}\n");
  fclose(f);
  LOG_INFO("Exported %s\n", fname);
}

void generate_images(void) {
  LOG_INFO("Generating PNG images...\n");
  
  double start = get_time_ms();
  int ret1 = system("sfdp -Tpng dodag_old.dot -o dodag_old.png 2>/dev/null");
  int ret2 = system("sfdp -Tpng dodag_final.dot -o dodag_final.png 2>/dev/null");
  double elapsed = get_time_ms() - start;
  
  if(ret1 == 0 && ret2 == 0) {
    LOG_INFO("SUCCESS: Generated PNG files (%.2f ms)\n", elapsed);
  } else {
    LOG_INFO("Install Graphviz: sudo apt-get install graphviz\n");
    LOG_INFO("Manual: sfdp -Tpng dodag_old.dot -o dodag_old.png\n");
  }
}

void print_statistics(void) {
  time_t now;
  struct tm *timeinfo;
  char timestamp[100];
  
  time(&now);
  timeinfo = localtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
  
  printf("\n╔════════════════════════════════════════════════════════════╗\n");
  printf("║           MESHIFICATION RESULTS & STATISTICS              ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ Timestamp: %-47s ║\n", timestamp);
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ NETWORK CONFIGURATION                                      ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ Network Size:               %6d nodes                   ║\n", n_nodes);
  printf("║ Max Supported:              %6d nodes                   ║\n", MAX_NODES);
  printf("║ Connection Probability:     %6.2f                        ║\n", connection_prob);
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ TOPOLOGY METRICS                                           ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ Original Edges:             %6d                          ║\n", original_edges);
  printf("║ Redundant Edges Added:      %6d                          ║\n", redundant_edges_added);
  printf("║ Total Edges (Final):        %6d                          ║\n", original_edges + redundant_edges_added);
  printf("║ Edge Overhead:              %6.2f%%                       ║\n", 
         100.0 * redundant_edges_added / (original_edges > 0 ? original_edges : 1));
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ DEGREE DISTRIBUTION                                        ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ Avg Degree (Initial):       %6.2f                        ║\n", avg_degree_initial);
  printf("║ Avg Degree (Final):         %6.2f                        ║\n", avg_degree_final);
  printf("║ Max Degree (Final):         %6d                          ║\n", max_degree_final);
  printf("║ Degree Increase:            %6.2f%%                       ║\n", 
         100.0 * (avg_degree_final - avg_degree_initial) / (avg_degree_initial > 0 ? avg_degree_initial : 1));
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ BICONNECTIVITY ANALYSIS                                    ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ Biconnected Components:     %6d                          ║\n", num_blocks);
  printf("║ Leaf Blocks:                %6d                          ║\n", num_leaf_blocks);
  printf("║ Cut Vertices (Initial):     %6d                          ║\n", initial_cut_vertices);
  printf("║ Cut Vertices (Final):       %6d                          ║\n", final_cut_vertices);
  printf("║ Cut Vertices Eliminated:    %6d (%.1f%%)                 ║\n", 
         initial_cut_vertices - final_cut_vertices,
         initial_cut_vertices > 0 ? 100.0 * (initial_cut_vertices - final_cut_vertices) / initial_cut_vertices : 0);
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ EXECUTION TIME BREAKDOWN                                   ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ Topology Generation:        %8.2f ms                     ║\n", time_topology_gen);
  printf("║ Initial Analysis (Tarjan):  %8.2f ms                     ║\n", time_initial_analysis);
  printf("║ Redundancy Addition:        %8.2f ms                     ║\n", time_redundancy_addition);
  printf("║ Final Analysis (Tarjan):    %8.2f ms                     ║\n", time_final_analysis);
  printf("║ DOT Export:                 %8.2f ms                     ║\n", time_dot_export);
  printf("║ ─────────────────────────────────────────────────────────  ║\n");
  printf("║ TOTAL EXECUTION TIME:       %8.2f ms                     ║\n", time_total);
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ ALGORITHM EFFICIENCY                                       ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ Time per Node:              %8.3f ms/node               ║\n", time_total / n_nodes);
  printf("║ Time per Edge:              %8.3f ms/edge               ║\n", 
         (original_edges + redundant_edges_added) > 0 ? time_total / (original_edges + redundant_edges_added) : 0);
  printf("║ Theoretical Complexity:     O(V + E)                       ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ OUTPUT FILES                                               ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ • dodag_old.dot     (Original topology)                   ║\n");
  printf("║ • dodag_final.dot   (Meshified topology)                  ║\n");
  printf("║ • dodag_old.png     (Original visualization)              ║\n");
  printf("║ • dodag_final.png   (Meshified visualization)             ║\n");
  printf("╚════════════════════════════════════════════════════════════╝\n\n");
}

/* ----------------- Main algorithm ------------------ */

void run_meshification(void) {
  double start_total = get_time_ms();
  
  LOG_INFO("Starting meshification...\n");
  
  init_arrays();
  
  /* Topology generation */
  double start = get_time_ms();
  generate_random_topology();
  time_topology_gen = get_time_ms() - start;
  
  /* Initial analysis */
  start = get_time_ms();
  find_biconnected_components();
  time_initial_analysis = get_time_ms() - start;
  
  initial_cut_vertices = 0;
  for(int i=0; i<n_nodes; i++) if(is_cut[i]) initial_cut_vertices++;
  
  LOG_INFO("Initial: %d cut vertices, %d blocks\n", initial_cut_vertices, num_blocks);
  
  /* Export original */
  start = get_time_ms();
  export_dot_graph("dodag_old.dot", 0);
  double export_time1 = get_time_ms() - start;
  
  /* Add redundancy if needed */
  if(initial_cut_vertices > 0) {
    start = get_time_ms();
    add_optimal_redundant_edges();
    time_redundancy_addition = get_time_ms() - start;
    
    start = get_time_ms();
    find_biconnected_components();
    time_final_analysis = get_time_ms() - start;
  } else {
    LOG_INFO("Graph is already biconnected!\n");
    time_redundancy_addition = 0.0;
    time_final_analysis = 0.0;
  }
  
  /* Export final */
  start = get_time_ms();
  export_dot_graph("dodag_final.dot", 1);
  double export_time2 = get_time_ms() - start;
  
  time_dot_export = export_time1 + export_time2;
  
  /* Compute metrics */
  compute_network_metrics();
  
  /* Generate images */
  generate_images();
  
  time_total = get_time_ms() - start_total;
  
  /* Print statistics */
  print_statistics();
}

/* ----------------- Contiki process ------------------ */

PROCESS(cut_vertex_mesh_process, "RPL Cut-Vertex Detection");
AUTOSTART_PROCESSES(&cut_vertex_mesh_process);

PROCESS_THREAD(cut_vertex_mesh_process, ev, data)
{
  PROCESS_BEGIN();
  
  /* Parse command-line arguments */
  if(contiki_argc > 1) {
    int user_nodes = atoi(contiki_argv[1]);
    if(user_nodes >= 10 && user_nodes <= MAX_NODES) {
      n_nodes = user_nodes;
      LOG_INFO("Using node count: %d\n", n_nodes);
    } else {
      printf("Invalid node count. Must be 10-%d. Using: %d\n", 
             MAX_NODES, n_nodes);
    }
  }
  
  printf("\n╔════════════════════════════════════════════════════════════╗\n");
  printf("║         RPL MESHIFICATION ALGORITHM DEMO                  ║\n");
  printf("╠════════════════════════════════════════════════════════════╣\n");
  printf("║ Algorithm: Block-Cut Tree Optimal Edge Addition           ║\n");
  printf("║ Target: Eliminate All Cut Vertices (Biconnectivity)       ║\n");
  printf("╚════════════════════════════════════════════════════════════╝\n\n");
  
  run_meshification();
  
  LOG_INFO("Process complete. Check output files.\n");
  
  PROCESS_END();
}
