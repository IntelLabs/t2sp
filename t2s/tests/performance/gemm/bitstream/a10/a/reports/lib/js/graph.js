"use strict";

// disable JSHint warning: Use the function form of "use strict".
// This warning is meant to prevent problems when concatenating scripts that
// aren't strict, but we shouldn't have any of those anyway.
/* jshint -W097 */

var GRAPH_MESSAGE_PREFIX = "<div id='graph_message' style='height:150px;width:100%;display:table;text-align:center;'><span style='vertical-align:middle;display:table-cell;'>";
var GRAPH_MESSAGE_SUFFIX = "</span></div>";

var GRAPH_LOADING_DIV = GRAPH_MESSAGE_PREFIX + "Loading..." + GRAPH_MESSAGE_SUFFIX;
var OPT_AS_REG_DIV = GRAPH_MESSAGE_PREFIX + "This variable was implemented in registers." + GRAPH_MESSAGE_SUFFIX;
var UNSYNTH_DIV = GRAPH_MESSAGE_PREFIX + "All uses have been optimized away; this variable was not synthesized." + GRAPH_MESSAGE_SUFFIX;
var UNTRACK_DIV = GRAPH_MESSAGE_PREFIX + "Variable implementation type could not be determined." + GRAPH_MESSAGE_SUFFIX;
var NO_NODES_DIV = GRAPH_MESSAGE_PREFIX + "No nodes to render for this graph." + GRAPH_MESSAGE_SUFFIX;
var SELECT_GRAPH_DIV = GRAPH_MESSAGE_PREFIX + "Select a graph to display." + GRAPH_MESSAGE_SUFFIX;

// constants for adding views to the system viewers
// type is temporary until the rendering option is exposed at the function level
var VIEWER_CONST = {
    CSPV: { name: "cspv_graph", elemId: "cspg",  gid: "GVG"  , type:"CSPV" },  // TODO: merge with GV
    LMEM: { name: "lmem_graph", elemId: "lmemg", gid: "LMEMG", type:"LMEM" },
    SV:   { name: "sv_chart",   elemId: "svc",   gid: "SVC",   type:"SV"   }, // Schedule Viewer
    GV:   { name: "gv_graph",   elemId: "gvg",   gid: "GVG",   type:"GV"   }  // Graph Viewer
};

// Converstion function from global view to viewer constants
function getViewerConst(){
  switch (view) {
    case VIEWS.LMEM:
      return VIEWER_CONST["LMEM"];
    case VIEWS.GV:
      return VIEWER_CONST["GV"];
    case VIEWS.SCHEDULE:
      return VIEWER_CONST["SV"];
  }
  return undefined;
}

// Graph variables
var gv_graph;
var lmem_graph;
var cspv_graph;
var top_node_id = -1;  // node ID selected from fancy tree

// Internet Explorer 6-11
var isIE = /*@cc_on!@*/false || !!document.documentMode;

// Edge 20+
var isEdge = !isIE && !!window.StyleMedia;

/* @Return: the graph in SVG format
 *
 * Arguments:
 *  mavData    : the data to be rendered
 *  graphConstObj: Constant values from VIEWER_CONST use to set graph name, element ID, and GID
 * Optional arguments to graph filter flags. Right now, they are only for local memory
 *  kernelName : CSPV, name of component - TO BE OBSELETED with new schema
 *  lmemName   : LMEM, name of memsys    - TO BE OBSELETED with new schema
 *  bankList   : LMEM, a list of banks to render
 *  bankName   : LMEM, memsys bank name
 * TODO:
 *  intfcFlag  : ["showall"|true|false] true means show interface for HLS and IORW for OpenCL if 
 *               exist. showall means don't add checkbox filter option, i.e. in SV, we don't want to 
 *               have the filter because it will resulted in just rendering a bunch of boxes.
 *  ememFlag   : [showall|true|false] CSPV - true means show external memory and LSU's if exist
 *  lmemFlag   : [showall|true|false] CSPV - true means show local memory and LSU's if exist
 *  depFlag    : [true|false] BV - true means only show loop carried dependency
 *  replFlag   : [true|false] LMEM - add replication under bank
 *  uniqueFlag : [true|false] - don't uniquify node copies when creating nodes for rendering
 * Optional arguments to control rendering options:
 *  direction  : 
 *  width      :
 */
function StartGraph(mavData, graphConstObj, kernelName, lmemName, bankList, bankName, replFlag) {
    // HTML ID's
    let GID = '#' + graphConstObj.gid;
    let elemId = graphConstObj.elemId;
    let graph = graphConstObj.name;
    let graphType = graphConstObj.type;

    // Graph filter flag
    var uniqueFlag = true;
    if (graphType == "LMEM") uniqueFlag = false;  // don't uniquify instructions for LMV

    // Node and link data from the JSON file
    var allNodes = mavData.nodes,
        allLinks = mavData.links;

    var flattenedNodes = {},    // all nodes included in graph
        flattenedLinks = {},    // all links included in graph
        nodeMap        = {},    // all nodes from JSON file (including duplicate LSUs)
        linkMap        = {},    // all links to nodes
        invisNodes     = [],    // nodes which disappear from layer deselected (eg. control, memory)
        invisLinks     = [],    // links which disappear when layer deselected
        subsetNodes    = [];    // all filtered nodes for the graph

    var clickDown;              // node which was last clicked

    var nodeTypes = [];

    // node and container shape defaults
    var chanWidth        = 5,
        nodeHeight       = 5,
        nodeWidth        = 20,
        portRadius       = 2,
        containerPadding = 15;

    var memsysNodes = [];
    var lmemNode;
    var lmemBankNode;

    var spg, spgSVG, spgGroup;  // spg stands for stall point graph is was our very first product

    var panelWidth, panelHeight, graphWidth, graphHeight;
    var zoomFitScale;
    var zoom;
    var marginOffset = 10;

    // A dictionary of node detail keys that should be hidden
    var hiddenDetails = { "type": true,
                          "Component": true,
                          "Fmax Bottlenecks": true,
                          "Pipelined": true,
                          "Subloops":true,
                          "Loops To":true }; 

    // Add graph to the container
    addGraph();

    // Create maps of nodes and links
    createNodeMap("", allNodes);
    createLinkMap(allLinks);

    // Preprocess the nodes when bankList is not empty
    if (bankList !== undefined) {
        subsetNodes = preProcessNodes(bankList);  // use by Memory viewer
    }
    // TODO: TO BE OBSELETED with new schema
    // Preprocess the nodes if it's CSPV
    else if (graphType == "CSPV") {
        subsetNodes = preProcessCompNodes(kernelName);
    }

    // Collapse similar channels
    preProcessChannels();

    // Create separation container
    spg.setNode("container", {});

    // TODO: remove this case statement later
    // Create nodes and links
    if (graphType == "LMEM") {
        createNodes("", subsetNodes, lmemName, bankList);
    } else if (graphType == "CSPV") {
        top_node_id = -1;
        createNodes("", subsetNodes);
    } else {
        createNodes("", allNodes);
    }
    createLinks(allLinks);  // Also flips the direction of arrow for backedges and Loads to help rendering

    patchLatency();

    // Create the renderer
    var spgRenderer = new dagreD3.render();

    // Render the graph
    spgRenderer(d3.select("#" + elemId + " g"), spg);

    // Setup the graph
    setupGraph();

    // FUNCTIONS
    // --------------------------------------------------------------------------------------------

    function patchLatency() {
        Object.keys(flattenedNodes).filter(function(n) {
            return (flattenedNodes[n].type === 'bb');
        }).forEach(function (bb) {
            var details = getDetails(flattenedNodes[bb]);
            Object.keys(details).forEach(function (d) {
                if (d === "Latency") {
                    var blockLatency = findLatency(flattenedNodes[bb].name);
                    var latency_str = (blockLatency < 0) ? "Unknown" : blockLatency.toString();  // Error handling
                    details[d] = latency_str;
                }
            });
        });
    }

    // Print nodes and links to console
    function printNodesAndLinks() {

        console.log("NODES");
        Object.keys(flattenedNodes).forEach(function (key) {
            console.log(key);
        });

        console.log("LINKS");
        Object.keys(flattenedLinks).forEach(function (key) {
            console.log(key, flattenedLinks[key]);
        });
    }

    function getUniqueNodeName(id) {
      return "_"+id;
    }

    function getNodeID(n) {
      return getUniqueNodeName(n.id);
    }

    function getNode(id) {
      if (!flattenedNodes[id]) return flattenedNodes[getUniqueNodeName(id)];
      return flattenedNodes[id];
    }

    function isStallable(id) {
        var node = flattenedNodes[id];
        if (!node) return false;
        var details = getDetails(node);
        return (details && details['Stall-free'] && details['Stall-free'] === "No");
    }

    // Create graphs for tabs
    // Different graphType uses different rendering options
    function addGraph() {
        // Remove graph_message div
        d3.selectAll("#graph_message").remove();
        // Add graph canvas
        var width = (graphType == "LMEM") ? 1000 : 2000;
        d3.select(GID)
          .append("svg")
          .attr("id", elemId)
          .attr("width", width);
        // Dagre-D3 rendering options
        // nodesep: horizontal distance between nodes
        // ranksep: vertical distance between nodes
        // edgesep: padding between container and nodes
        // rankdir: direction of the graph, TB: top to bottom; LR: left to right
        // acyclicer: choose algorithm for correcting cycles in dagre-d3 (fix case: 409063)
        // edgecomplex: set to "complex" for default edges. Set to "simplify" to render simpler edges with less curve
        // article/#/1408584206: Rendering LR caused issue with zoom for large netlist
        var args_dict = { nodesep: 25, ranksep: 35, edgesep: 15, rankdir: "TB", edgecomplex: "complex", resizeclusters: "resize" };
        if (graphType == "CSPV")      args_dict.acyclicer = "greedy";
        else if (graphType == "LMEM"){
            args_dict.rankdir = "LR";
            args_dict.ranksep = 50;
            args_dict.edgecomplex = "simplify";
            args_dict.resizeclusters = "none";
        }
        // Create the input graph
        spg = new dagreD3.graphlib.Graph({ compound: true })
          .setGraph(args_dict)
          .setDefaultEdgeLabel(function () { return {}; });

        // Create svg group
        spgSVG = d3.select("#" + elemId);
        spgGroup = spgSVG.append("g")
                         .attr('class', 'graph');
    }

    // Create map of all nodes: node id -> node data
    function createNodeMap(parent, nodes) {
        nodes.forEach(function (n) {
            nodeMap[getNodeID(n)] = n;
            n.parent = parent;
            if (n.type == "memsys" || n.type == "romsys") {
                memsysNodes.push(n);
                if (nodeMap[parent].name == "Global Memory") n.global = true;
            }
            if (n.children) createNodeMap(getNodeID(n), n.children);
        });
    }

    // Create map of all links: node id -> all links associated with node
    function createLinkMap(links) {
        links.forEach(function (lnk) {
            if (!linkMap[getUniqueNodeName(lnk.from)]) linkMap[getUniqueNodeName(lnk.from)] = [];
            linkMap[getUniqueNodeName(lnk.from)].push(lnk);

            if (!linkMap[getUniqueNodeName(lnk.to)]) linkMap[getUniqueNodeName(lnk.to)] = [];
            linkMap[getUniqueNodeName(lnk.to)].push(lnk);
        });
    }

    // Filter out the instruction nodes, etc. which are not part of the specified local memory
    function preProcessNodes(bankList) {
        var nodeList = [];

        function addPortNeighbourNodes(port) {
            // Check for neighbouring nodes to the port
            // A port only has one node connected to it: either an arbitration node or an instruction node

            // Given an edge which links to the arbitration, add the ones which have an instruction linked to it
            function preProcessArbLinks(arb_link) {
                var arbNodeLinkFrom = nodeMap[getUniqueNodeName(arb_link.from)];
                var arbNodeLinkTo = nodeMap[getUniqueNodeName(arb_link.to)];

                if (arbNodeLinkFrom.type == "inst" || arbNodeLinkFrom.type == "interface") {
                    nodeList.push(arbNodeLinkFrom);
                } else if (arbNodeLinkTo.type == "inst" || arbNodeLinkTo.type == "interface") {
                    nodeList.push(arbNodeLinkTo);
                }
            }

            linkMap[getUniqueNodeName(port.id)].forEach(function (link) {
                // Check if there is an arbitration node connected to this port and add it
                var nodeLinkFrom = nodeMap[getUniqueNodeName(link.from)];
                var nodeLinkTo = nodeMap[getUniqueNodeName(link.to)];
                if (nodeLinkFrom.type == "arb") {
                    // Add arbitration node to nodeList
                    nodeList.push(nodeLinkFrom);
                    // Find all instruction neighbours of the arbitration node
                    linkMap[getUniqueNodeName(nodeLinkFrom.id)].forEach(preProcessArbLinks);
                }
                else if (nodeLinkTo.type == "arb") {
                    // Add arbitration node to nodeList
                    nodeList.push(nodeLinkTo);
                    // Find all instruction neighbours of the arbitration node
                    linkMap[getUniqueNodeName(nodeLinkTo.id)].forEach(preProcessArbLinks);
                } else if (nodeLinkTo.type == "inst") {
                    nodeList.push(nodeLinkTo);
                } else if (nodeLinkFrom.type == "inst") {
                    nodeList.push(nodeLinkFrom);
                } else if (nodeLinkFrom.type == "interface") {
                    nodeList.push(nodeLinkFrom);
                } else if (nodeLinkTo.type == "interface") {
                    nodeList.push(nodeLinkTo);
                }
            });
        }

        // Start at the memsys nodes and then move outward
        memsysNodes.forEach(function (n) {
            // Get the kernel name of the particular memsys node by getting the parent of parent
            // memsys -> memtype -> kernel    or     memsys -> memgroup -> memtype -> kernel
            // node.parent returns id, then use nodeMap to convert id -> node
            var currKernel;
            if(nodeMap[n.parent].type === "memgroup") currKernel = nodeMap[nodeMap[nodeMap[n.parent].parent].parent];
            else currKernel = nodeMap[nodeMap[n.parent].parent];

            if (n.name == lmemName && currKernel.name == kernelName) {
                // Found the correct local memory
                lmemNode = n;

                // Add to the nodeList
                nodeList.push(n);

                // Look for arbitration nodes
                if (n.children) {
                    //  Get the banks
                    var curBankList = n.children;
                    curBankList.forEach(function (b) {
                        // Check if this bank is supposed to be rendered
                        if (bankList.indexOf(b.name) != -1 || (replFlag && b.name == bankName) ) {
                            // If the bank matches the bankName, then store this node
                            if (b.name == bankName) lmemBankNode = b;

                            // Get the replicates
                            var replList = b.children;
                            if(replList){
                                if(!replFlag){
                                    // For local memory view, want to only show "logical" ports for each Bank:
                                    // 1) Collect all ports in first replicate
                                    // 2) For all subsequent replicates, collect all ports that are not write ports
                                    var firstRepl = true;
                                    var logicalPorts = [];
                                    replList.forEach(function(r){
                                        // Get the physical ports of the replicate
                                        var physicalPorts = r.children;
                                        if (physicalPorts) {
                                            physicalPorts.forEach(function (p) {
                                                if(firstRepl || p.name !== "W"){
                                                    logicalPorts.push(p);
                                                }
                                            });
                                        }
                                        firstRepl = false;
                                    });

                                    // Add logical ports to the node
                                    b.logPorts = logicalPorts;
                                    if (logicalPorts) {
                                        logicalPorts.forEach(addPortNeighbourNodes);
                                    }

                                } else{
                                    // For Bank view, iterate through all physical ports in all replicates
                                    replList.forEach(function(r){
                                        var portList = r.children;
                                        if (portList) {
                                            portList.forEach(addPortNeighbourNodes);
                                        }
                                    });
                                }
                            }
                        }
                    });
                }
           }
        });
        return nodeList;
    }

    // TODO: TO BE OBSELETED with new schema
     // Filter out the instruction nodes, etc. which are not part of the specified components
    function preProcessCompNodes(kernel_name) {
        var nodeList = [];

        mavData.nodes.forEach(function (n) {
            if (n.type == "component" && n.name == kernel_name) {
                nodeList.push(n);
            } else if (n.type == "stream") {
                // Check if the destination of the stream is in the desired component
                var lnk = linkMap[getNodeID(n)][0]; // Should only be one edge
                if (linkMap[getNodeID(n)].length !== 1) console.log("Error: stream node has more than one edge!");
                var destNodeID = nodeMap[getUniqueNodeName(lnk.from)].type == "stream" ? lnk.to : lnk.from;
                var destNode = nodeMap[getUniqueNodeName(destNodeID)];
                var destBBNode = nodeMap[destNode.parent];
                var destCompNode = nodeMap[destBBNode.parent];
                if (destCompNode.name == kernel_name) {
                    nodeList.push(n);
                }
            } else if (n.type == "memtype") {
                nodeList.push(n);
            } else if (n.type == "interface") {
                // Check if the interface node is for this particular component 
                if (getDetails(n) && getDetails(n).Component == kernel_name) {
                    nodeList.push(n);
                }
            }
        });
        return nodeList;
    }

    // Collapse similar channels
    function preProcessChannels() {
        var channels = [],
            rLink,
            wLink,
            read = {},
            write = {},
            found = false;

        Object.keys(nodeMap).forEach(function (key) {
            if (nodeMap[key].type == "stream" && (nodeMap[key].name == "do" || nodeMap[key].name == "return")) {
              nodeMap[key].visible = true;
              return;
            }
            if (nodeMap[key].type == "channel" || nodeMap[key].type == "pipe" || nodeMap[key].type == "stream") {
                read = {};
                write = {};
                rLink = {};
                wLink = {};
                var nodeID = getNodeID(nodeMap[key]);

                if (linkMap[nodeID].length < 2) {
                    nodeMap[key].visible = true;
                    return;
                }

                if (linkMap[nodeID][0].from == nodeMap[key].id) {
                    read = linkMap[nodeID][0].to;
                    rLink = linkMap[nodeID][0];
                    write  = linkMap[nodeID][1].from;
                    wLink = linkMap[nodeID][1];
                } else {
                    write  = linkMap[nodeID][0].from;
                    wLink = linkMap[nodeID][0];
                    read = linkMap[nodeID][1].to;
                    rLink = linkMap[nodeID][1];
                }

                nodeMap[key].read = read;
                nodeMap[key].write = write;
                nodeMap[key].count = 1;
                found = false;

                for (var i = 0; i < channels.length; i++) {
                    if (channels[i].name == nodeMap[key].name &&
                        verifyDetails(nodeMap[getUniqueNodeName(channels[i].read)], nodeMap[getUniqueNodeName(read)]) &&
                        verifyDetails(nodeMap[getUniqueNodeName(channels[i].write)], nodeMap[getUniqueNodeName(write)])) {
                        channels[i].count++;
                        nodeMap[key].visible = false;
                        rLink.from = channels[i].id;
                        wLink.to = channels[i].id;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    nodeMap[key].visible = true;
                    channels.push(nodeMap[key]);
                }
            }
        });
    }

    // Get abbreviated name for instructions to label the node
    function getLabelName(name) {
        if (name.indexOf("Load") != -1) return "LD";
        else if (name.indexOf("Store") != -1) return "ST";
        else if (name.indexOf("Read") != -1) return "RD";
        else if (name.indexOf("Write") != -1) return "WR";
        else return (name);
    }

    // Add highlighting persistence and syncing to editor and details pane
    function addClickFunctions(graph) {

        var nodes = graph.selectAll("g.node rect, g.nodes .label, g.node circle, g.node polygon")
            .on('click', function (d) {

                refreshPersistence(graph);
                if (clickDown == d) {
                    clickDown = null;
                } else {
                    highlightNodes(d, graph);
                    changeDivContent(0, getHTMLDetailsFromJSON(flattenedNodes[d].details, flattenedNodes[d].name));
                    clickDown = d;
                }

                // details and editor syncing (reset if no line number)
                if (getNode(d).hasOwnProperty('debug') && getNode(d).debug.length > 0 && getNode(d).debug[0].length > 0)
                  syncEditorPaneToLine(getNode(d).debug[0][0].line, getFilename(getNode(d).debug[0][0].filename));
                else
                  editorNoHighlightActiveLine();
            });

        var clusters = graph.selectAll("g.cluster rect")
            .on('click', function (d) {

                refreshPersistence(graph);
                if (clickDown == d) {
                    clickDown = null;
                } else if (flattenedNodes[d].type == "memsys" || flattenedNodes[d].type == "bank" || flattenedNodes[d].type == "replicate" || flattenedNodes[d].type == "romsys" || flattenedNodes[d].type == "bb" || flattenedNodes[d].type == "csr") {
                    highlightNodes(d, graph);
                    changeDivContent(0, getHTMLDetailsFromJSON(flattenedNodes[d].details, flattenedNodes[d].name));
                    clickDown = d;
                }

                // details and editor syncing (reset if no line number)
                if (getNode(d).hasOwnProperty('debug') && getNode(d).debug.length > 0 && getNode(d).debug[0].length > 0)
                  syncEditorPaneToLine(getNode(d).debug[0][0].line, getFilename(getNode(d).debug[0][0].filename));
                else
                  editorNoHighlightActiveLine();
            });

      // Allow selecting and highlighting of graph edges.
      graph.selectAll("g.edgePath path")
        .on('click', function(d) {
          refreshPersistence(graph);
          if (clickDown == d) {
            clickDown = null;
          } else {
            var details;
            for (var i = 0; i < flattenedLinks[d.v].length; i++) {
              if ("_"+flattenedLinks[d.v][i].from == d.v && "_"+flattenedLinks[d.v][i].to == d.w) {
                details = getDivDetails(flattenedLinks[d.v][i]);
                break;
              }
            }
            if (details === null) details = "<br>";
            else details = getHTMLDetailsFromJSON(details, "Edge");
            changeDivContent(0, details);
            clickDown = d;
          }
        });
    }

    // Add highlighing to nodes and links
    function addHighlighting(graph) {

        var highlightColor = "#1d99c1";
        var highlightNodeColor = "#8ECCE0";

        var clusterHighlights = graph.selectAll("g.cluster rect")
            .on('mouseover', function (d) {
                if (getNode(d) && (flattenedNodes[d].type == "memsys" || flattenedNodes[d].type == "bank" || flattenedNodes[d].type == "replicate" || flattenedNodes[d].type == "romsys" || flattenedNodes[d].type == "bb" || flattenedNodes[d].type == "csr")) {
                    highlightNodes(d, graph);
                }
                if (!clickDown && flattenedNodes[d] && getDetails(flattenedNodes[d])) {
                    changeDivContent(0, getHTMLDetailsFromJSON(flattenedNodes[d].details, flattenedNodes[d].name));
                }
            })
            .on('mouseout', function (d) {
                if (clickDown != d) {
                    refreshPersistence(graph);
                    highlightNodes(clickDown, graph);
                }
            });

        var nodeHighlights = graph.selectAll("g.node rect, g.label, g.node circle, g.node polygon")
            .on('mouseover', function (d) {
                highlightNodes(d, graph);
                if (!clickDown && flattenedNodes[d] && getDetails(flattenedNodes[d])) {
                    changeDivContent(0, getHTMLDetailsFromJSON(flattenedNodes[d].details, flattenedNodes[d].name));
                }
            })
            .on('mouseout', function (d) {
                if (clickDown != d) {
                    refreshPersistence(graph);
                    highlightNodes(clickDown, graph);
                }

            });


        // Highlight link, associated nodes on mouseover
        var linkHighlights = graph.selectAll("g.edgePath path")
            .on('mouseover', function (d) {

                var connections = graph.selectAll("g.edgePath")
                    .filter(function (k) {
                        return d.v == k.v && d.w == k.w;
                    });

                connections.selectAll("path")
                    .style("opacity", 0.5)
                    .style("stroke-width", 5)
                    .style("stroke", highlightColor);

                var connectedNodes = graph.selectAll("g.node rect, g.node circle, g.node polygon")
                    .filter(function (n) {
                        return n == d.v || n == d.w;
                    })
                    .style("stroke-width", 3)
                    .style("stroke", highlightNodeColor);

                connections.selectAll("marker")
                    .attr({
                        "markerUnits": "userSpaceOnUse",
                        "preserveAspectRatio": "none",
                        "viewBox": "0 0 40 10",
                        "refX": 6,
                        "markerWidth": 40,
                        "markerHeight": 12
                    })
                    .style("stroke-width", 0);
                connections.selectAll("marker path")
                    .attr("style", "fill:" + highlightColor + "; opacity: 1; stroke-width:0");

              if (!clickDown && flattenedLinks[d.v]) {
                var details;
                for (var i = 0; i < flattenedLinks[d.v].length; i++) {
                  if ("_"+flattenedLinks[d.v][i].from == d.v && "_"+flattenedLinks[d.v][i].to == d.w) {
                    details = getDivDetails(flattenedLinks[d.v][i]);
                  }
                }
                if (details !== null) {
                  details = getHTMLDetailsFromJSON(details, "Edge");
                  changeDivContent(0, details);
                }
              }
            })
            .on('mouseout', function (d) {
                if (clickDown != d) refreshPersistence(graph);
                if (clickDown) highlightNodes(clickDown, graph);
            });
    }

    // Highlight associated links and nodes
    function highlightNodes(d, graph) {
        var highlightColor = "#1D99C1";
        var highlightNodeColor = "#8ECCE0";

        var associatedNodes = [];
        associatedNodes.push(d);

        // Find associated links and nodes
        var connections = graph.selectAll("g.edgePath").filter(function (k) {
            if (k == d) return true;
            if (invisNodes.indexOf(k.v) != -1 || invisNodes.indexOf(k.w) != -1) return false;
            if (invisLinks.indexOf(k) != -1) return false;
                if (k.v == d || k.w == d) {
                    if (associatedNodes.indexOf(k.v) == -1) associatedNodes.push(k.v);
                    if (associatedNodes.indexOf(k.w) == -1) associatedNodes.push(k.w);
                    return true;
                }
            return false;
        });

        var numFirstNeighbours = associatedNodes.length;
        // Get additional links if graphType is LMEM if the hovered node is not an arbitration node
        // When hovering over an instr, then want to highlight the path to the port through arb (if any): inst -> arb -> port
        // When hovering over a port, then want to highlight the arb and ALL inst to that arb
        if (graphType == "LMEM" && d && typeof d != 'undefined' && !isNaN(d.replace("_","")) && flattenedNodes[d].type != "arb") {
            connections = graph.selectAll("g.edgePath").filter(function (k) {
                if (invisNodes.indexOf(k.v) != -1 || invisNodes.indexOf(k.w) != -1) return false;
                if (invisLinks.indexOf(k) != -1) return false;
                // Check 2 cases:
                // 1. If d is type instr, then check that tail (k.v) is in one of the 1st neighbours of d.
                // 2. If d is a type port, then check that head (k.w) is in one of the 1st neighbours of d. This essentially gets the inst->arb->port case
                var indexOfKV = associatedNodes.indexOf(k.v);
                var indexOfKW = associatedNodes.indexOf(k.w);
                // Only check the if the edge is connected to the 1st neighbours of d
                if ((indexOfKV != -1 && indexOfKV < numFirstNeighbours) && (flattenedNodes[d].type == "inst" || flattenedNodes[d].type == "interface") ||
                    (indexOfKW != -1 && indexOfKW < numFirstNeighbours && flattenedNodes[d].type == "port")) {
                    if (indexOfKV == -1) associatedNodes.push(k.v);
                    if (indexOfKW == -1) associatedNodes.push(k.w);
                    return true;
                }
                return false;
            });
        }

        // Highlight links
        connections.selectAll("path")
            .attr("style", "stroke:" + highlightColor + "; opacity: 0.5; fill:none; stroke-width:5;");

        // Highlight nodes
        var connectedNodes = graph.selectAll("g.cluster rect, g.node rect, g.node circle, g.node polygon, g.cluster rect")
            .filter(function (n) {
                if (associatedNodes.indexOf(n) == -1) return false;
                else if (getNode(n).type == "kernel" || getNode(n).type == "component" || getNode(n).type == "memtype") return false;
                else return true;
            })
            .style("stroke-width", 3)
            .style("stroke", highlightNodeColor);

        // Color and highlight arrowheads
        connections.selectAll("marker")
            .attr({
                "markerUnits": "userSpaceOnUse",
                "preserveAspectRatio": "none",
                "viewBox": "0 0 40 10",
                "refX": 6,
                "markerWidth": 40,
                "markerHeight": 12
            })
        .style("stroke-width", 0);
        connections.selectAll("marker path")
            .attr("style", "fill:" + highlightColor + "; opacity: 1; stroke-width:0");
    }

    // Add tooltips to display details
    function addToolTips(graph) {
        // Returns HTML formated. TODO: should be refactored with details
        var tt = function (n) {
            var name = "";
            if (flattenedNodes[n].type === "channel" || flattenedNodes[n].type === "pipe" || flattenedNodes[n].type === "stream") {
              // Safe guard to show pipe wording for SYCL
              if (product === PRODUCTS.SYCL && flattenedNodes[n].type === "channel") {
                flattenedNodes[n].type = flattenedNodes[n].type.replace("channel", "pipe");
              }
              name += flattenedNodes[n].type + " ";
            }

            // Safe guard to show pipe wording for SYCL
            if  (product === PRODUCTS.SYCL && flattenedNodes[n].name.includes("Channel")) {
              flattenedNodes[n].name = flattenedNodes[n].name.replace("Channel", "Pipe");
            }
            name += getHTMLName(flattenedNodes[n].name);

            if (flattenedNodes[n].count && flattenedNodes[n].count > 1) name += " (x" + flattenedNodes[n].count + ")";

            var text = "<h6 class=\"card-title custom_tooltip\">" + name + "</h6><p class=\"card-text\">";
            var details = getDetails(flattenedNodes[n]);
            Object.keys(details).filter(function(d) {
                return (d.indexOf("Address bit information") === -1) && (d.indexOf("Memory layout information") === -1) &&
                       (d.indexOf("Memory word address pattern") === -1);
            }).forEach(function (k) {
                if (typeof details[k] !== "string" && typeof details[k] !== "number") return;
                if (details[k] === "") return;  // empty string
                if (!(hiddenDetails[k]) && !(k.indexOf("Arguments from") != -1 && k.indexOf(kernelName) == -1)) {
                    text += k + ": " + details[k] + "<br>";
                }
            });
            text += "</p>";
            return text;
        };

        graph.selectAll("g.node rect, g.cluster rect, g.node circle, g.node polygon, g.node table")
            .filter(function (d) {
                if (d.indexOf("container") != -1 || d == "glbmem") return false;
                return (flattenedNodes[d] && getDetails(flattenedNodes[d]));
            })
            .style("fill", "white")
            .attr({
              "title": function (d) { return tt(d); },
              "data-html": "true",
              "data-placement": "right"
             })
            .each(function (v) { $(this).tooltip({ content: function() {return $(this).attr('title');}, position: {my: "left bottom", at: "center top-2"} }); }); 

        var ee = function (n) {
          var text = "<p class=\"card-text\">";
          var item;
          for (var i = 0; i < flattenedLinks[n.v].length; i++) {
            if ("_"+flattenedLinks[n.v][i].from == n.v && "_"+flattenedLinks[n.v][i].to == n.w) {
              item = flattenedLinks[n.v][i];
            }
          }
          var details = getDetails(item);
          Object.keys(details).forEach(function (k) {
            if (typeof details[k] !== "string" && typeof details[k] !== "number") return;
            if (!(hiddenDetails[k]) && !(k.indexOf("Arguments from") != -1 && k.indexOf(kernelName) == -1)) {
              text += k + ": " + details[k] + "<br>";
            }
          });
          text += "</p>";
          return text;
        };

        // Edges
        graph.selectAll("g.edgePath path")
          .filter(function (d) {
            for (var i = 0; i < flattenedLinks[d.v].length; i++) {
              if ("_"+flattenedLinks[d.v][i].from == d.v && "_"+flattenedLinks[d.v][i].to == d.w) {
                return getDetails(flattenedLinks[d.v][i]);
              }
            }
            return false;
          })
          .attr({
            "title": function (d) { return ee(d); },
            "data-html": "true",
            "data-placement": "right"
          })
          .each(function (v) { $(this).tooltip({ content: function() {return $(this).attr('title');}, position: {my: "left bottom", at: "center top-2"} }); }); 
    }

    // Return true if node is merge or branch
    function isMergeOrBranch(node) {
        return (flattenedNodes[node].name == "loop" || flattenedNodes[node].name == "begin" || flattenedNodes[node].name == "end" || flattenedNodes[node].name == "loop end");
    }

    this.refreshGraph = function () {
        if (clickDown) {
            if (clickDown.hasOwnProperty('v') && clickDown.hasOwnProperty('w')) {
              // This is an edge - don't update div.
              clickDown = null;
              return;
            }
            changeDivContent(0, getHTMLDetailsFromJSON(getDivDetails(flattenedNodes[clickDown]), flattenedNodes[clickDown].name));
        }
    };

    // Refresh persistent highlighting
    function refreshPersistence(graph) {

        graph.selectAll("g.edgePath path")
            .style("opacity", 0.3)
            .style("stroke-width", 2)
            .style("stroke", "#333");

        graph.selectAll("g.cluster rect, g.node rect, g.node circle, g.node polygon")
            .filter(function(d) { return getNode(d); })
            .style("stroke-width", 1.5)
            .style("stroke", "#999");

        colorArrowheads(graph);
        if (graph == spgSVG)
            colorNodes(graph);
    }

    // Format arrowheads
    function colorArrowheads(graph) {
        var markers = graph.selectAll("marker")
            .attr({
                "markerUnits": "userSpaceOnUse",
                "preserveAspectRatio": "none",
                "viewBox": "0 0 40 10",
                "refX": 8,
                "markerWidth": 30,
                "markerHeight": 8
            })
            .style("stroke-width", 0);
        graph.selectAll("marker path")
            .attr("style", "fill:#333; opacity: 1; stroke-width:0");
    }

    // Color basic blocks with loops that cannot be unrolled
    function colorNodes(graph) {
        var loopBlockColor   = "#ff0000",
            singlePumpColor  = "#5cd6d6",
            doublePumpColor  = "#239090",
            glbmemColor      = "#006699",
            loopEdgeColor    = "#000099",
            mergeBranchColor = "#ff8533",
            channelColor     = "#bf00ff",
            kernelColor      = "#666699",
            bankColor        = replFlag ? singlePumpColor : "#ffffff",
            replColor        = "#ffffff",
            stableArgColor   = "#bf59c6";

        // Fill all clusters (necessary for mouseover)
        var nodes = graph.selectAll("g.node rect, g.cluster rect")
            .filter(function (d) {
                return (d.indexOf("container") == -1 && d != "glbmem" && flattenedNodes[d] &&
                    flattenedNodes[d].type != "kernel" && flattenedNodes[d].type != "component");
            })
            .style("fill-opacity", 0.5)
            .style("fill", "white");

        // Color loop basic blocks
        nodes.filter(function (d) {
            var node = flattenedNodes[d];
            var details = getDetails(node);
            return (node && node.type == "bb" && details && details.Subloops == "No" &&
                (details.Pipelined == "No" ||
                details.II > 1 ||
                (details.II == 1 && details["Fmax Bottlenecks"] == "Yes")));
        })
        .style("fill-opacity", 0.5)
        .style("fill", loopBlockColor)
        .style("stroke-width", 0);

        // Color kernel outlines
        graph.selectAll("g.node rect, g.cluster rect")
            .filter(function (d) {
                return (flattenedNodes[d] && (flattenedNodes[d].type == "kernel" || flattenedNodes[d].type == "component"));
            })
            .style("stroke-width", 2)
            .style("stroke", kernelColor);

        // Select all memsys nodes
        var mem = nodes.filter(function (d) {
            return (flattenedNodes[d] && flattenedNodes[d].type == "memsys");
        }).style("fill", singlePumpColor)
          .style("stroke", singlePumpColor);

        // Color all banks
        var bank = nodes.filter(function (d) {
            return (flattenedNodes[d] && flattenedNodes[d].type == "bank");
        }).style("fill", bankColor)
          .style("stroke", singlePumpColor);

        // Color all replicates
        var replicate = nodes.filter(function (d) {
            return (flattenedNodes[d] && flattenedNodes[d].type == "replicate");
        }).style("fill", replColor)
          .style("stroke", singlePumpColor);

        // Color all copies
        var copies = nodes.filter(function (d) {
            return (flattenedNodes[d] && flattenedNodes[d].type == "copies");
        }).style("fill", replColor)
          .style("stroke", singlePumpColor);

        // Select all romsys nodes
        var rom = nodes.filter(function (d) {
            return (flattenedNodes[d] && flattenedNodes[d].type == "romsys");
        }).style("fill", singlePumpColor)
          .style("stroke", singlePumpColor);

        // Color global memory systems
        mem.filter(function (d) {
                return (flattenedNodes[d] && flattenedNodes[d].global);
            })
            .style("fill", glbmemColor)
            .style("stroke", glbmemColor);

        // Color all stable arguments
        var stable_arg = nodes.filter(function (d) {
            var node = flattenedNodes[d];
            var details = getDetails(node);
            return (node &&
                    node.type == "interface" &&
                    details &&
                    details.Stable === "Yes");
        }).style("fill", stableArgColor)
          .style("stroke", stableArgColor);

        var insts = graph.selectAll("g.nodes circle, g.nodes polygon");

        // Color stallable nodes
        insts.filter(isStallable)
        .style("stroke", loopBlockColor)
        .style("fill", loopBlockColor)
        .style("fill-opacity", 0.6);

        // Color arbitration nodes
        var arbs = nodes.filter (function (d) {
            return (flattenedNodes[d] && flattenedNodes[d].type == "arb" && flattenedNodes[d].name == "ARB");
        }).style("stroke", loopBlockColor)
          .style("fill", loopBlockColor)
          .style("fill-opacity", 0.6);

        insts.filter(function (d) {
            if (!isStallable(d)) return false;
            if (flattenedLinks[d]) {
              for (var i = 0; i < flattenedLinks[d].length; i++) {
                var parentFrom = getNode(flattenedLinks[d][i].from);
                var parentTo = getNode(flattenedLinks[d][i].to);
                if (parentFrom.global || parentTo.global) return true;
              }
            }
            var node = flattenedNodes[d];
            if (!node) return false;
            var details = getDetails(node);
            return (details && details['Global Memory'] && details['Global Memory'] === "Yes");
        })
        .style("fill-opacity", 0.5)
        .style("stroke", glbmemColor)
        .style("fill", glbmemColor);

        var connections = graph.selectAll("g.edgePath");

        // Color channel connections
        var channelConnections = connections.filter(function (k) {
            return (flattenedNodes[k.v].type == "channel" ||
                flattenedNodes[k.v].type == "pipe" ||
                flattenedNodes[k.v].type == "stream" ||
                flattenedNodes[k.w].type == "channel" ||
                flattenedNodes[k.w].type == "pipe" ||
                flattenedNodes[k.w].type == "stream" ||
                flattenedNodes[k.v].type == "interface");
        });

        channelConnections.selectAll("path")
            .style("stroke", channelColor);

        channelConnections.selectAll("marker")
            .attr({
                "markerUnits": "userSpaceOnUse",
                "preserveAspectRatio": "none",
                "viewBox": "0 0 40 10",
                "refX": 6,
                "markerWidth": 40,
                "markerHeight": 12
            })
            .style("stroke-width", 0);
        channelConnections.selectAll("marker path")
            .attr("style", "fill:" + channelColor + "; opacity: 0.8; stroke-width:0");

        // Color loop connections
        var mergeBranchConnections = connections.filter(function (k) {
            var nodeV = getNode(k.v),
                nodeW = getNode(k.w);

            if ((isMergeOrBranch(k.v) || nodeV.type == "bb") &&
                getDetails(nodeV) &&  getDetails(nodeV)["Loops To"] &&
                (isMergeOrBranch(k.w) || nodeW.type == "bb") &&
                getDetails(nodeV)["Loops To"] == nodeW.id) {
                return true;
            }
            return false;
        });

        // Color MG and BR in loops
        insts.filter(function (d) {
            var node = flattenedNodes[d];
            var bb = flattenedNodes[node.parent];
            var details = getDetails(bb);
            if (node && isMergeOrBranch(d) && details &&
                details.Subloops === "No" &&
                (details.Pipelined == "No" ||
                details.II > 1 ||
                (details.II == 1 && details["Fmax Bottlenecks"] == "Yes"))) {
                node.isHighlighted = true;
                return true;
            }
            node.isHighlighted = false;
            return false;
        })
        .style("stroke", mergeBranchColor)
        .style("fill", mergeBranchColor);

        // Color loop back edges
        mergeBranchConnections.selectAll("path")
            .style("opacity", 0.5)
            .style("stroke", loopEdgeColor);

        // Color highlighted loop back edges
        var loopHighlight = mergeBranchConnections.filter(function (k) {
            return (flattenedNodes[k.v].isHighlighted && flattenedNodes[k.w].isHighlighted);
            });
        loopHighlight.selectAll("path")
            .style("stroke", loopBlockColor);

        // Color loop back edge arrowheads
        mergeBranchConnections.selectAll("marker")
        .style("stroke-width", 0);

        mergeBranchConnections.selectAll("marker path")
            .style("fill", loopEdgeColor)
            .style("stroke-width", 0)
            .style("opacity", 0.8);

        loopHighlight.selectAll("marker path")
            .style("fill", loopBlockColor)
            .style("stroke-width", 0)
            .style("opacity", 0.8);

    }

    // GRAPH VIEWERS FEATURES
    // --------------------------------------------------------------------------------------------

    // Setup the graph viewer features, SPG stands for Stall Point Graph
    function setupGraph() {

        if(replFlag && lmemNode.type === "memsys") {
            // Delete all dummy edges that were added to get proper placement of copies and ports inside replicates
            removeDummyEdges(spgSVG);
            // Align ports to the border of the replicates, center the copies and the bank
            alignReplicatePorts(spgSVG);
        }

        // Add layers menu
        addCheckBox();
        // Add highlighting for links
        addHighlighting(spgSVG);
        // Add syncing to line and persistence for link highlights
        addClickFunctions(spgSVG);
        // Color link arrowheads
        colorArrowheads(spgSVG);
        // Add tooltips to nodes to display details
        addToolTips(spgSVG);
        // Color basic blocks with loops (also calls colorNodes)
        refreshPersistence(spgSVG);
        // Hide container border
        spgSVG.selectAll("g.cluster rect")
            .filter(function (d) { return d.indexOf("container") != -1 || d == "glbmem"; })
            .style("stroke-width", "0px");

        // Adjust the size of the window before getting the pane dimensions to get accurate scaling
        adjustToWindowEvent();

        panelWidth = $(GID)[0].getBoundingClientRect().width - 2 * marginOffset;
        panelHeight = $(GID)[0].getBoundingClientRect().height - 2 * marginOffset;

        graphWidth = spgGroup[0][0].getBoundingClientRect().width + 2 * marginOffset;
        graphHeight = spgGroup[0][0].getBoundingClientRect().height + 2 * marginOffset;
        zoomFitScale = Math.min(panelWidth/graphWidth, panelHeight/graphHeight);

        var offsetX = marginOffset + panelWidth/2 - graphWidth/2 * zoomFitScale; // Offset to center the graph in the middle
        var offsetY = marginOffset + panelHeight/2 - graphHeight/2 * zoomFitScale; // Offset to center the graph in the middle

        // Add zoom and drag
        var zoom_range = [];
        // Calculate the zoom range depending on if the zoomFitScale is greater or less than one
        if (zoomFitScale <= 1) {
            zoom_range = [1, 2 / zoomFitScale];
        } else {
            zoom_range = [1, 2];
        }
            
        zoom = d3.behavior.zoom().scaleExtent(zoom_range).on("zoom", function () {
            var panelWidth = $(GID)[0].getBoundingClientRect().width - 2 * marginOffset;
            var panelHeight = $(GID)[0].getBoundingClientRect().height - 2 * marginOffset;
            
            zoomFitScale = Math.min(panelWidth/graphWidth, panelHeight/graphHeight);
        
            var offsetX = marginOffset + panelWidth / 2 - graphWidth / 2 * zoomFitScale; // Offset to center the graph in the middle
            var offsetY = marginOffset + panelHeight / 2 - graphHeight / 2 * zoomFitScale; // Offset to center the graph in the middle
        
            var x = d3.event.translate[0] + offsetX * d3.event.scale; 
            var y = d3.event.translate[1] + offsetY * d3.event.scale;

            d3.select(GID).select("g")
                .attr("transform", "translate(" + x + "," + y + ")" +
                                        "scale(" + d3.event.scale * zoomFitScale + ")");
            $('g.cluster rect, g.node circle, g.node rect, g.node polygon').trigger('mouseleave');
        });

        spgSVG.call(zoom);

        // Place the graph in top left corner
        spgGroup.attr("transform", "translate( " + offsetX + ", " + offsetY + ") scale(" + zoomFitScale + ")");
        spgSVG.attr("height", Math.max(spg.graph().height + 40, panelHeight));

        // If memory viewer, then change details pane to be the memory's detail
        if (graphType == "LMEM") {
            // The memory name is clicked, not the bank name
            if ( (typeof bankName === 'undefined') || (typeof lmemBankNode === 'undefined' )) {
                changeDivContent(0, getHTMLDetailsFromJSON(getDivDetails(lmemNode), lmemNode.name));
            } else {
                changeDivContent(0, getHTMLDetailsFromJSON(getDivDetails(lmemBankNode), lmemBankNode.name));
            }
        }
    }

    // The dummy edges added between Ports and copies inside the replicate views are removed from the final render
    function removeDummyEdges(graph){
        var lmemType = lmemNode.type;

        graph.selectAll("g.edgePath")
            .filter(function(e){
                return (flattenedNodes[e.v].type === "copies" || flattenedNodes[e.w].type === "copies");
            })
            .remove();   
    }

    // This function is only called when running with replFlag set true and when rendering
    // the bank/replication view for RAMs. This is not called for ROMs since they do not have copies
    function alignReplicatePorts(graph){

        // Add more padding to the bank clusters for better visibility
        var additionalBankPadding = 20;

        // Dagre-d3 uses the "translate" transform attribute to place nodes/clusters
        // by assigning them their x-y coordinates as translate(x,y)
        // As a general rule, the coordinates are parsed and set in that same manner

        function setCoordinates(x,y,node){
            function translate(x,y) {
                return "translate(" + x + "," + y + ")";
            }
            var newTransform = translate(x,y);
            node.setAttribute("transform",newTransform);
        }

        var portX = [];

        // Iterate through all ports in the graph and get their x-coordinates
        var ports = graph.selectAll("g.node")
                         .filter(function(p){
                            return flattenedNodes[p].type === "port";
                         }).each(function(h){
                            var centerCoords = d3.transform(d3.select(this).attr("transform"));
                            var floats = centerCoords.translate;
                            portX.push(floats[0]); // push x-coordinate
                         });
        
        // Check that all ports are aligned - i.e that their x-coordinates are identical
        var centerX;
        portX.forEach(function(x){
            if(typeof centerX === "undefined") centerX = x;
            warn(centerX === x, "Replicate ports are not aligned!");
        });

        // Get all replicate clusters
        var replicateClusters = graph.selectAll("g.cluster")
                            .filter(function(c){
                                if(typeof flattenedNodes[c] === "undefined") return false;
                                else return (flattenedNodes[c].type === "replicate");
                            });
        
        var replCenters = [];

        // Position replicate clusters such that their left side is aligned with the ports
        replicateClusters.each(function(c){
            var centerCoords = d3.transform(d3.select(this).attr("transform"));
            var currCoordinates = centerCoords.translate;
            var clusterWidth = this.querySelector("rect").getAttribute("width");

            // Dagre-d3 uses the y attribute to specify the vertical offset of each cluster
            // This check keeps track of the y coordinate of the top-most and bottom-most replicate
            // boundaries inside the given bank view
            var clusterHeight = parseFloat(this.querySelector("rect").getAttribute("height"));
            var yOffset = parseFloat(this.querySelector("rect").getAttribute("y"));
            var topBorder = yOffset;
            var bottomBorder = yOffset + clusterHeight;

            // center cluster on new x-coordinate while maintaining old y-coordinate
            var clusterCenter = centerX + clusterWidth/2;
            replCenters.push(clusterCenter);
            setCoordinates(clusterCenter,currCoordinates[1],this);
        });


        var replCenterX;

        replCenters.forEach(function(x){
            if(typeof replCenterX === "undefined") replCenterX = x;
            warn(replCenterX === x, "Replicate clusters are not aligned!");
        });

        var bankClusters = graph.selectAll("g.cluster")
                    .filter(function(c){
                        if(typeof flattenedNodes[c] === "undefined") return false;
                        else return (flattenedNodes[c].type === "bank");
                    });

        // Position bank clusters by centering them on replicates
        bankClusters.each(function(c){
            // Fetch current cluster height and add padding
            var clusterHeight = parseFloat(this.querySelector("rect").getAttribute("height"));
            clusterHeight += additionalBankPadding;

            // center cluster on new x-coordinate and set new y-coordinate due to padding
            var centerCoords = d3.transform(d3.select(this).attr("transform"));
            var currCoordinates = centerCoords.translate;

            // To fix known issue of asymmetry of replicates inside the bank view, the distance
            // between the bank and the top-most replicate and the bottom-most replicate is compared.
            // In the case that the spacing at the bottom is larger than the spacing on top, restore
            // symmetry by equalizing the spacing difference and then adding container padding
            var yOffset = parseFloat(this.querySelector("rect").getAttribute("y"));
            var topBank = yOffset;
            var bottomBank = yOffset + clusterHeight;

            this.querySelector("rect").setAttribute("height",clusterHeight);
            var clusterY = currCoordinates[1] - additionalBankPadding/2;
            setCoordinates(replCenterX,clusterY,this);
       });
    }

    // Create nodes for graphs recursively - DFS 
    // It currently assumes memory system is always after all the functions
    // TODO: make this function depends on rendering flag instead of depend on views
    // TODO: argument "kernelName" TO BE OBSELETED with new schema
    function createNodes(group, nodes, lmemName, bankList) {
        var isInst = false;
        var insts = [];
        var index = 0;
        var name = "";
        var seenGlobal = false; // For determining if this component stores to a global variable

        // if (ememFlag) {
            // TODO: first determine this kernel/function has connection to Global memory
        // }

        function createCopiesNode(num_copies, copies_name, parent_name, copies_details) {
            // Create a node for copies, add it to the flattenedNodes
            // Propagate the details over to allow for tooltip hover info
            // and details if they are serialized in new_lmvJSON
            var dummynode = {};
            dummynode.type = "copies";
            dummynode.name = parent_name + " copies";
            dummynode.id = copies_name;
            dummynode.details = copies_details;
            flattenedNodes[dummynode.id] = dummynode;
            // Flow for chrome, firefox, etc.
            if( !isIE && !isEdge){
                // Node contains an HTML table that enumerates
                // the number of copies, adds dashed lines between
                // the different copies in the table. If there are more
                // than 4 copies, then only the first 3 and last are shown
                // with a "..." row added to indicate the abstraction to the user
                spg.setNode(copies_name, {
                  label: function() {
                    var table = document.createElement("table");
                    var rows = [];
                    var max_visible_copies = (num_copies > 4) ? 3 : num_copies;
                    var i, j, row;
                    for (i = 0; i < max_visible_copies; i++){
                        row = table.insertRow(i).insertCell(0);
                        row.innerHTML = "Copy " + i;
                        rows.push(row);
                    }

                    // Height per copy is divided evenly as percentages
                    // among rows containing the copy number
                    // Row containing "..." is 0.25 of their height
                    var heightPerCopy = (num_copies > 4) ? (100/4.25) : (100/num_copies);
                    rows.forEach(function(r){
                        r.style.width = "100%";
                        r.style.height = heightPerCopy + "%";
                        r.style.color = "black";
                    });

                    if(num_copies > 4){
                        for (j = 0; j < 2; j++,i++){
                            row = table.insertRow(i).insertCell(0);
                            row.innerHTML = (j === 0) ? "&middot&middot&middot" : ("Copy " + (num_copies-1));
                            if(j === 0) {
                                // row containing "..."
                                row.style.fontSize = "20px";
                                row.style.height = (heightPerCopy/4) + "%";
                            } else{
                                row.style.height = heightPerCopy + "%";
                            }
                            row.style.width = "100%";
                            row.style.color = "black";

                            rows.push(row);
                        }
                    }

                    // Dashed lines between rows in table
                    for (j = 0; j < i-1; j++){
                        rows[j].style.borderBottom = "thin dashed #5cd6d6";
                    }

                    // Total table height and width
                    table.style.height = "250px";
                    table.style.width = "125px";

                    return table;
                  },
                  padding: 0
                });

            } else {
                // Internet explorer or Edge since they do not support foreignObjects
                // which are  required to render html table for the copies node
                // Instead a rectangle is rendered containing information about # of copies
                spg.setNode(copies_name, { label: num_copies + " copies", clusterLabelPos: "top", shape: "rect", paddingTop: containerPadding,  width: 150, height: 250 });
            }
        }

        nodes.forEach(function (n) {

            // Only add nodes which are visible
            if (n.hasOwnProperty("visible") && !n.visible) return;

            // Add nodes to those in graph (instructions added after collapsing)
            if (n.type != "inst") flattenedNodes[getUniqueNodeName(n.id)] = n;

            if (group !== "") n.parent = group;

            // Collect node types for link filtering checkboxes
            if (nodeTypes.indexOf(n.type) == -1) nodeTypes.push(n.type);

            if (n.children) {
                // Add container (aka group) nodes
                // 1) It's a function, 
                // 2) bank that user has checked, 
                // 3) global memory only if there are instructions that connects to it
                if (graphType != "CSPV" && (n.type == "kernel" || n.type == "component" || n.type == "task")) {
                    spg.setNode(getNodeID(n), { label: n.type + " " + n.name, clusterLabelPos: "top", paddingTop: containerPadding });
                } else if(replFlag){
                    // If rendering Bank view, then do not render the local memory itself: only render the clicked Bank and all of its children
                    if( (n.type == "memsys" || n.type == "romsys") && n.name == lmemName){
                        createNodes("", n.children, lmemName, bankList);
                        return;
                    }
                    if(n.type == "bank" && n.name != bankName) return;
                    // Only the clicked bank and its replicates should get to this point. Add them their nodes to the render
                    spg.setNode(getNodeID(n), { label: n.name, clusterLabelPos: "top", paddingTop: containerPadding,  width: nodeWidth, height: nodeHeight });
                } else if (n.type == "bank" && bankList && bankList.indexOf(n.name) == -1)  {
                    // Set the collapsed bank to not have any padding and default width/height
                    spg.setNode(getNodeID(n), { label: n.name, clusterLabelPos: "top", width: nodeWidth, height: nodeHeight });
                } else if (graphType == "CSPV" && n.name == "Global Memory" && !seenGlobal) {
                    // Don't render the global memory if no instructions sees it 
                    return; 
                } else {
                    spg.setNode(getNodeID(n), { label: n.name, clusterLabelPos: "top", paddingTop: containerPadding,  width: nodeWidth, height: nodeHeight });
                }

                // Place in correct group
                if (n.name == "Global Memory") {
                    spg.setNode("glbmem", {});
                    spg.setParent(getNodeID(n), "glbmem");
                } else if (group !== "") {
                    spg.setParent(getNodeID(n), group);
                } else {
                    spg.setParent(getNodeID(n), "container");
                }

                // Render bank ports or replications for if it's in bank list
                if(replFlag && n.type == "bank" && n.name == bankName) {
                    // If in replication view, render the replicates and physical ports
                    createNodes(getNodeID(n), n.children, lmemName, bankList);
                } else if (n.type == "bank" && bankList !== undefined) {
                    if (bankList.indexOf(n.name) != -1){
                        // If in local mem view and bank is checked, render the logical ports only
                        createNodes(getNodeID(n), n.logPorts, lmemName, bankList);
                    }
                } else if (n.type == "replicate") {
                    // Create nodes for ports
                    createNodes(getNodeID(n), n.children, lmemName, bankList);

                    if(typeof n.copies !== "undefined") {
                        // create copy node and add "dummy" edges from ports to copy node for proper placement
                        var num_copies = n.copies.num;
                        var copies_name = lmemName + "_" + n.name + "_copies";

                        if(num_copies > 0) {
                            createCopiesNode(num_copies, copies_name, n.name, n.copies.details);
                            spg.setParent(copies_name, getNodeID(n));
                            // Add dummy edges from ports to copies node to encourage dagre-d3 to place it in the center
                            n.children.forEach(function(p){
                                spg.setEdge(getNodeID(p),copies_name, {weight: 1});
                            });
                        }
                    }
                } else {
                    // Create nodes from children
                    createNodes(getNodeID(n), n.children, lmemName, bankList);
                }

            } else {
                // Create regular node, inst, or channel
                if (n.type == "inst") {
                    if (uniqueFlag) {
                        // Uniquify instructions to show less nodes and edges for all other views
                        index = checkInst(insts, n, true);
                        if (index == -1) {
                            n.count = 1;
                            insts.push(n);
                        } else {
                            insts[index].count += 1;
                        }
                        // Only check if the instruction goes to a Global Memory if we haven't seen the Global Memory yet
                        if (!seenGlobal) {
                            var d = getNodeID(n);
                            if (linkMap[d] !== undefined) {
                                for (var i = 0; i < linkMap[d].length; i++) {
                                    var parentFrom = nodeMap[getUniqueNodeName(linkMap[d][i].from)];
                                    var parentTo = nodeMap[getUniqueNodeName(linkMap[d][i].to)];
                                    if (parentFrom.global || parentTo.global) seenGlobal = true;
                                }
                            } else {
                                console.log("Error! linkMap not found.", d);
                            }
                        }
                    } else {
                        insts.push(n);
                    }

                    isInst = true;
                } else if (n.type == "channel" || n.type == "pipe" || n.type == "stream" || n.type == "interface") {
                    if (n.visible || !n.hasOwnProperty("visible")) {
                        // If name length > 10, truncate to the form "abcde...vwxyz"
                        name = n.name;
                        if (n.name.length > 13) name = name.substring(0, 5) + "..." + name.substring(name.length - 5, name.length);
                        if (n.count > 1) name += " (x" + n.count + ")";
                        var chanWidth = name.length * 4 + 2;
                        spg.setNode(getNodeID(n), { label: name, width: chanWidth, height: nodeHeight });
                        if (group === "") spg.setParent(getNodeID(n), "container");
                        else spg.setParent(getNodeID(n), group);
                    }
                } else if (n.type == "memsys" || n.type == "romsys") {
                    // Check if this memsys needs to be rendered for HLS Component Viewer
                    // TODO: this looks redundant with seenGlobal - should commonize it and do it at the beginning
                    var memsysLinkList = linkMap[getNodeID(n)];
                    if (graphType == "CSPV" && (getNode(group) && getNode(group).name == "Global Memory") && !isGlobMemsysInComp(memsysLinkList, kernelName)) {
                        return;
                    }

                    name = n.name;
                    if (n.details) {
                      var numbanks = /(\d+)/.exec(getDetails(n)["Number of banks"]);
                      if (numbanks !== null && numbanks !== undefined) name += " [" + numbanks[0] + "]";
                      var replication = /(\d+)/.exec(getDetails(n)["Total replication"]);
                      if (replication !== null && replication !== undefined && replication[0] > 1) name += " (x" + replication[0] + ")";
                    }
                    var padding = name.length < 8 ? 10 : 2; // Padding for memsys with very short names so that "Global Memory is not cut off"
                    var memsysWidth = name.length*5 + padding;
                    spg.setNode(getNodeID(n), { label: name, clusterLabelPos: "top", paddingTop: containerPadding, width: memsysWidth});
                    spg.setParent(getNodeID(n), group);
                } else if (n.type == "port") {
                    var portShape = (replFlag && lmemNode.type !== "romsys") ? "diamond" : "circle";
                    spg.setNode(getNodeID(n), { label: n.name, shape: portShape, width: portRadius, height: portRadius });
                    spg.setParent(getNodeID(n), group);
                } else if (n.type == "bb") {
                    // If name length > 10, truncate to the form "abcde...vwxyz"
                    name = n.name;
                    var bbWidth = name.length * 5 + 2;
                    spg.setNode(getNodeID(n), { label: n.name, width: bbWidth, height: nodeHeight });
                    spg.setParent(getNodeID(n), group);
                } else {
                    spg.setNode(getNodeID(n), { label: n.name, width: nodeWidth, height: nodeHeight });

                    if (n.name == "Global Memory") {
                        spg.setNode("glbmem", {});
                        spg.setParent(getNodeID(n), "glbmem");
                    } else if (group !== "") {
                        spg.setParent(getNodeID(n), group);
                    } else {
                        spg.setParent(getNodeID(n), "container");
                    }
                }
            }
        });

        if (isInst) setInsts(group, insts);
    }

    // Create links for stall point
    function createLinks(links) {
        links.forEach(function (lnk) {
            if (flattenedNodes.hasOwnProperty(getUniqueNodeName(lnk.from)) && flattenedNodes.hasOwnProperty(getUniqueNodeName(lnk.to))) {

                if (!flattenedLinks[getUniqueNodeName(lnk.from)]) flattenedLinks[getUniqueNodeName(lnk.from)] = [];
                flattenedLinks[getUniqueNodeName(lnk.from)].push(lnk);

                if (!flattenedLinks[getUniqueNodeName(lnk.to)]) flattenedLinks[getUniqueNodeName(lnk.to)] = [];
                flattenedLinks[getUniqueNodeName(lnk.to)].push(lnk);

                // For these specific cases, reverse the edge direction when passing into Dagre-D3 engine to help alignment of the nodes
                if (graphType == "LMEM") {
                    // 1. INST <-- ARB. Render it as INST --> ARB
                    // 2. INST <-- Port. Render it as INST --> Port
                    // 3. ARB <-- Port. Render it as ARB --> Port
                    if ((getNode(lnk.to).type == "inst" && getNode(lnk.from).type == "arb") ||
                        (getNode(lnk.to).type == "inst" && getNode(lnk.from).type == "port") ||
                        (getNode(lnk.to).type == "arb" && getNode(lnk.from).type == "port")) {
                        spg.setEdge(getUniqueNodeName(lnk.to), getUniqueNodeName(lnk.from), { arrowhead: "normal", lineInterpolate: "basis", weight: 1 });
                    } else {
                        // Render the edge in the normal direction as specified in the JSON
                        spg.setEdge(getUniqueNodeName(lnk.from), getUniqueNodeName(lnk.to), { arrowhead: "normal", lineInterpolate: "basis", weight: 1 });
                    }
                } else {
                    // Reverse the edge when:
                    // 4. LD <-- MemSys. Still render in that direction
                    // 5. Loop <-- LoopEnd. Still render in that direction
                    if (getNode(lnk.to).type == "inst" && (getNode(lnk.from).type == "memsys" || getNode(lnk.from).type == "romsys")) {
                        spg.setEdge(getUniqueNodeName(lnk.to), getUniqueNodeName(lnk.from), { arrowhead: "reversed", lineInterpolate: "basis", weight: 1 });
                    } else if (getDetails(getNode(lnk.to)) && getDetails(getNode(lnk.to)).hasOwnProperty("Loops To") && getDetails(getNode(lnk.to))["Loops To"] == getNode(lnk.from).id) {
                        spg.setEdge(getUniqueNodeName(lnk.to), getUniqueNodeName(lnk.from), { arrowhead: "reversed", lineInterpolate: "basis", weight: 1 });
                    } else if (getNode(lnk.to).type == "inst" && (getNode(lnk.from).type == "channel" || getNode(lnk.from).type == "pipe")) {
                        // Workaround OpenCL designs that can deadlock. Deadlock is caused by IO Read that precedes IO write in multiple kernels
                        // Example:
                        //  2 kernels, both need to do an IO read from the other one. Now we end up with a deadlock
                        spg.setEdge(getUniqueNodeName(lnk.to), getUniqueNodeName(lnk.from), { arrowhead: "reversed", lineInterpolate: "basis", weight: 1 });
                    } else if (lnk.to == lnk.from) {
                        // Reverse the arrowhead for self-loops for blocks so that it points back up instead of downward
                        spg.setEdge(getUniqueNodeName(lnk.from), getUniqueNodeName(lnk.to), { arrowhead: "reversed", lineInterpolate: "basis", weight: 1 });
                    } else if (lnk.hasOwnProperty("reverse")) {
                        spg.setEdge(getUniqueNodeName(lnk.to), getUniqueNodeName(lnk.from), { arrowhead: "reversed", lineInterpolate: "basis", weight: 1 });
                    } else {
                        spg.setEdge(getUniqueNodeName(lnk.from), getUniqueNodeName(lnk.to), { arrowhead: "normal", lineInterpolate: "basis", weight: 1 });
                    }
                }
            }
        });
    }

     // Check if two insts are the same
    function checkInst(insts, node, isSPG) {
        var index = 0;
        for (var i = 0; i < insts.length; i++) {
            if (   node.type == "inst" &&
                node.name == insts[i].name &&
                verifyDetails(node, insts[i]) &&
                verifyLinks(insts[i], node, isSPG)) { return index; }
            index++;
        }

        return -1;
    }

    // Verify details and debug information between nodes
    function verifyDetails(a, b) {
      var aDetails = getDetails(a);
      var bDetails = getDetails(b);

      // Verify that details sections match
      if (aDetails && bDetails) {
          var akeys = Object.keys(aDetails);
          var bkeys = Object.keys(bDetails);
          if (akeys.length != bkeys.length) return false;

          for (var i = 0; i < akeys.length; ++i) {
              var key = akeys[i];
              if (!bDetails.hasOwnProperty(key) || (aDetails[key] != bDetails[key] && key != "Reference")) return false;
          }
      } else if (aDetails || bDetails) {
          return false;
      }
      
      // Verify that debug sections match
      var aLength = (a.debug) ? ((a.debug.length) ? a.debug[0].length : 0) : 0;
      var bLength = (b.debug) ? ((b.debug.length) ? b.debug[0].length : 0) : 0;
      
      if (aLength === bLength)
        return (!aLength || (a.debug[0][0].filename === b.debug[0][0].filename &&
                             a.debug[0][0].line     === b.debug[0][0].line));
      else
        return false;
    }

    // Verify that links are the same between duplicates
    function verifyLinks(inst, node, isSPG) {
        var instLinks = linkMap[getNodeID(inst)];
        var nodeLinks = linkMap[getNodeID(node)];
        var found = false;

        if (!instLinks && !nodeLinks) return true;
        else if (!instLinks || !nodeLinks) return false;

        for (var j = 0; j < instLinks.length; j++) {
            found = false;
            var i;
            if (instLinks[j].from == inst.id) {
                for (i = 0; i < nodeLinks.length; i++) {
                    if ( nodeLinks[i].from == node.id &&
                        nodeLinks[i].to == instLinks[j].to ) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            } else if (instLinks[j].to == inst.id) {
                for (i = 0; i < nodeLinks.length; i++) {
                    if ( nodeLinks[i].from == instLinks[j].from &&
                        nodeLinks[i].to == node.id ) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
        }

        return true;
    }

    // Set insts - including duplicates
    function setInsts(group, insts) {
        var name;

        insts.forEach(function (n) {
            flattenedNodes[getUniqueNodeName(n.id)] = n;

            name = getLabelName(n.name);
            if (n.hasOwnProperty('count') && n.count > 1) name += " (x" + n.count + ")";

            if (getNode(n.id).name == "end" || getNode(n.id).name == "loop end") spg.setNode(getUniqueNodeName(n.id), { label: name, shape: "diamond", width: 1, height: 1 });
            else spg.setNode(getUniqueNodeName(n.id), { label: name, shape: "circle", width: 1, height: 1 });

            if (group !== "") spg.setParent(getUniqueNodeName(n.id), group);
            else spg.setParent(getUniqueNodeName(n.id), "container");
        });
    }

    // Insert html for layer menus: not applicable to memory viewer
    // All viewer have various options to show more information, see top of the file for more details
    // addCheckBox only adds applicable check boxes based on the instructions are available
    function addCheckBox() {
        var CHECK_BOX_PREFIX = "<input id='linkCheck' type='checkbox' checked='checked' name='linkType' value='";
        var menu = "";
        menu += "<form id='layerMenu'>";
        menu += "<button title=\"Zoom to fit\" type='button' onclick='" + graph + ".zoomToFit(500)' style=\"padding:0\">Reset Zoom</button><button title=\"Remove highlights\" type='button' onclick='" + graph + ".removeHighlights()' style=\"padding:0\">Clear Selection</button>&nbsp&nbsp&nbsp&nbsp";

        // TODO: change if (graphType) condition to flags: ememFlag: memsys; lmemFlag: memsys; intfcFlag: channel and stream
        //       exceptions: SPV: always show intfc, LMEM: show lmem
        // Add checkbox for to show/hide connections types
        // TODO: Make this call startGraph instead because we want to netlist to re-render instead
        // This issue we are trying to address is that with large design, it can cause the graph not render at all during loading
        // The original design was intented not to re-render because the location of the node could change
        if ((graphType == "GV") || (graphType == "CSPV")) {
            nodeTypes.forEach(function (nt) {
                switch (nt) {
                    case "inst": if (graphType != "GV") menu += CHECK_BOX_PREFIX + nt + "' onClick='" + graph + ".resetVisibleLinks()'>&nbspControl&nbsp&nbsp";
                        break;
                    case "memsys": menu += CHECK_BOX_PREFIX + nt + "' onClick='" + graph + ".resetVisibleLinks()'>&nbspMemory&nbsp&nbsp";
                        break;
                    case "channel": 
                    case "pipe":
                        if (product === PRODUCTS.SYCL) {
                          menu += CHECK_BOX_PREFIX + nt + "' onClick='" + graph + ".resetVisibleLinks()'>&nbspPipes&nbsp&nbsp";
                        } else {
                          menu += CHECK_BOX_PREFIX + nt + "' onClick='" + graph + ".resetVisibleLinks()'>&nbspChannels&nbsp&nbsp";
                        }
                        break;
                    case "stream": menu += CHECK_BOX_PREFIX + nt + "' onClick='" + graph + ".resetVisibleLinks()'>&nbspStreams&nbsp&nbsp";
                        break;
                }
            });
        }

        menu += "</form>";

        if (graphType == "LMEM") {
            $("#layers-" + VIEWS[graphType].id).html(menu);
        } else {
            $("#layers").html(menu);
        }
    }

    // TODO: To be obsoleted, won't delete yet
    // Reset visibility on links after checked and unchecked in layers menu
    this.resetVisibleLinks = function () {

        d3.selectAll("g.edgePath path").style("visibility", "visible");
        d3.selectAll("g.node rect, g.label").style("visibility", "visible");
        refreshPersistence(spgSVG);
        clickDown = null;
        invisNodes = [];
        invisLinks = [];

        $('#layerMenu input').each(function () {
            var tempBox = (this);

            if (!tempBox.checked) {
                switch (tempBox.getAttribute("value")) {

                    // Remove streams and links to channels
                    case "stream":
                        spgSVG.selectAll("g.edgePath path").filter(function (k) {
                                if (flattenedNodes[k.v].type == "stream" || flattenedNodes[k.w].type == "stream") {
                                    invisLinks.push(k);
                                    return true;
                                }
                                return false;
                            })
                            .style("visibility", "hidden");

                        spgSVG.selectAll("g.node rect, g.nodes .label").filter(function (n) {
                                if (flattenedNodes[n].type == "stream") {
                                    if (invisNodes.indexOf(n) == -1) invisNodes.push(n);
                                    return true;
                                }
                                return false;
                            })
                            .style("visibility", "hidden");
                        break;

                    // Remove channels and links to channels
                    case "channel":
                    case "pipe":
                        spgSVG.selectAll("g.edgePath path").filter(function (k) {
                                if (flattenedNodes[k.v].type == "channel" || flattenedNodes[k.w].type == "channel" ||
                                    flattenedNodes[k.v].type == "pipe" || flattenedNodes[k.w].type == "pipe") {
                                    invisLinks.push(k);
                                    return true;
                                }
                                return false;
                            })
                            .style("visibility", "hidden");

                        spgSVG.selectAll("g.node rect, g.nodes .label").filter(function (n) {
                                if (flattenedNodes[n].type == "channel" || flattenedNodes[n].type == "pipe") {
                                    if (invisNodes.indexOf(n) == -1) invisNodes.push(n);
                                    return true;
                                }
                                return false;
                            })
                            .style("visibility", "hidden");
                        break;

                    // Remove links between instructions
                    case "inst":
                        spgSVG.selectAll("g.edgePath path").filter(function (k) {
                                if (flattenedNodes[k.v].type == "inst" && flattenedNodes[k.w].type == "inst") {
                                    invisLinks.push(k);
                                    return true;
                                }
                                return false;
                            })
                            .style("visibility", "hidden");
                        break;

                        // Remove all links to and from memory
                    case "memsys":
                        spgSVG.selectAll("g.edgePath path").filter(function (k) {
                                if (flattenedNodes[k.v].type == "memsys" || flattenedNodes[k.w].type == "memsys") {
                                    invisLinks.push(k);
                                    return true;
                                }
                                return false;
                            })
                            .style("visibility", "hidden");
                        break;
                }
            }

        });
    };

    // Remove highlighting
    this.removeHighlights = function () {
        refreshPersistence(spgSVG);
        clickDown = null;
    };

    // Zoom to fit function
    this.zoomToFit = function (duration) {
        var panelWidth = $(GID)[0].getBoundingClientRect().width - 2 * marginOffset;
        var panelHeight = $(GID)[0].getBoundingClientRect().height - 2 * marginOffset;

        zoomFitScale = Math.min(panelWidth/graphWidth, panelHeight/graphHeight);

        var offsetX = marginOffset + panelWidth/2 - graphWidth/2 * zoomFitScale; // Offset to center the graph in the middle
        var offsetY = marginOffset + panelHeight/2 - graphHeight/2 * zoomFitScale; // Offset to center the graph in the middle

        // Reset the graph's zoom level

        spgGroup.transition()
                .duration(duration)
                .attr("transform", "translate( " + offsetX + ", " + offsetY + ") scale(" + zoomFitScale + ")");

        // Reset the zoom object scale level and translate so that subsequent pan and zoom behaves like from beginning
        zoom.scale(1);
        zoom.translate([0,0]);
    };

    // Check that the edge links to an instruction to that component
    function isGlobMemsysInComp(memsysLinkList, kernelName) {
        for (var i = 0; i < memsysLinkList.length; i++) {
            var nodeLinkTo = getNode(memsysLinkList[i].to);
            var nodeLinkFrom = getNode(memsysLinkList[i].from);
            var compNode;
            if (nodeLinkTo && nodeLinkTo.type == "inst") {
                compNode = getCompNodeFromInstNode(nodeLinkTo);
                if (compNode.name == kernelName) {
                    return true;
                }
            } else if (nodeLinkFrom && nodeLinkFrom.type == "inst") {
                compNode = getCompNodeFromInstNode(nodeLinkFrom);
                if (compNode.name == kernelName) {
                    return true;
                }
            }
        }
        return false;
    }

    function getCompNodeFromInstNode(node) {
        // Go up 2 hierarchies
        // Component -> basicblock -> instruction node
        var bbNode = getNode(node.parent);
        var compNode = getNode(bbNode.parent);
        return compNode;
    }

    // return details object or null
    function getDetails(node) {
        if (!node || !node.details) return null;
        warn(node.details.length <= 1, "Details should not have more than 1 item!");
        return node.details[0];
    }

    // return details object or null -- doesn't index into details
    function getDivDetails(node) {
        if (!node || !node.details) return null;
        warn(node.details.length <= 1, "Details should not have more than 1 item!");
        return node.details;
    }
    
}

// Converts element Attribute to rendering node
function startGraphForBank(element) {
    var kernelName = element.getAttribute("data-kernel");
    var lmemName = element.getAttribute("data-lmem");
    var bankName = element.getAttribute("name");
  
    renderGraphForBank(new_lmvJSON, kernelName, lmemName, bankName);
}

/* Display message indicating why there is no graph rendered for the selected
 * entity.
 */
function renderNoGraphForType(graph,title,type,details,message){
    var contentText;

    switch(type){
        case "reg":
            contentText = OPT_AS_REG_DIV;
            break;
        case "unsynth":
            contentText = UNSYNTH_DIV;
            break;
        case "untrack":
            contentText = UNTRACK_DIV;
            break;
        case "no_nodes":
            contentText = NO_NODES_DIV;
            break;
        case "choose_graph":
            contentText = SELECT_GRAPH_DIV;
            break;
        case "message":
            contentText = GRAPH_MESSAGE_PREFIX + message + GRAPH_MESSAGE_SUFFIX;
            break;
        default:
            contentText = "";
    }
    top_node_id = -1;

    $(graph).html(contentText);
    changeDivContent(0, getHTMLDetailsFromJSON(details,title));
}

/* Renders a graph for specific bank list that has been checked
 * Use for Local Memory Viewer only, later expand to External Memory Viewer
 */
function renderGraphForBank(graphJSON, kernel_name, lmem_name, bank_name, replFlag, bankList) {
  let graphValue = getViewerConst();
  if (graphValue === undefined) {
    console.warn("Error! Trying to add Graph but view is not " + VIEWS["LMEM"].name);
    return;  // Error! Cannot invoke graph
  }

  // Start a new graph
  $('#LMEMG').html(GRAPH_LOADING_DIV);
  // Clear details pane before rendering
  clearDivContent();
  setTimeout(function() {
          lmem_graph = new StartGraph(graphJSON, graphValue, kernel_name, lmem_name, bankList, bank_name, replFlag);
          lmem_graph.refreshGraph();
      }, 20);
}

/* Render graph. This is the entry point for all hierarchies of graph viewer
 * choose the rendering flag based on graphType
 */
function renderGraph(graphJSON, graphID) {
    if (graphID !== undefined && top_node_id === graphID) return;  // do nothing if user clicks the same ID
    let graphValue = getViewerConst();
    if (graphValue === undefined) {
      console.warn("Error! Trying to add Graph but view is not " + VIEWS["GV"].name);
      return;  // Error! Cannot invoke graph
    }
    top_node_id = graphID;  // update top node
    clearDivContent();  // Clear details pane before rendering

    // Restart the new graph
    let GID = graphValue.gid;
    let loading_message = "Loading...";

    // Sanity check graphJSON
    if (graphJSON === undefined) {
        top_node_id = -1;
        renderNoGraphForType("#" + GID, "", "choose_graph", "");
        $(".card-header [id^=layers]").html('');
        return;
    } else if (graphJSON.nodes === undefined || graphJSON.nodes.length === 0) {
        top_node_id = -1;
        if (graphJSON.message) {
            renderNoGraphForType("#" + GID, "", "message", "", graphJSON.message);
        } else {
            renderNoGraphForType("#" + GID, "", "no_nodes", "");
        }
        $(".card-header #layers*").html('');
        return;
    }
    var nodes = graphJSON.nodes.length;
    if (nodes > 100) {
        loading_message += "<br>Graph contains " + nodes + " nodes; please be patient.";
    }

    $('#' + GID).html(GRAPH_MESSAGE_PREFIX + loading_message + GRAPH_MESSAGE_SUFFIX);
    setTimeout(function() {
        gv_graph = new StartGraph(graphJSON, graphValue);
        gv_graph.zoomToFit(0);
        refreshAreaVisibility();
        adjustToWindowEvent();
    }, 20);
}

/**
 * @function addViewerPanel create report pane for viewer data to populate later.
 * 
 * @param {Object} view an element in VIEWS object.
 * @param {String} layerId id to be created for the graph.
 * @param {String} panelId id of the report body defined in VIEWER_CONST.
 * @returns {HTML div element} to be append to the report body div.
 */
function addViewerPanel(view, layerId, panelId) {
  let headerName = view.name;
  let paneHeading = document.createElement('div');
  paneHeading.className = "card-header";
  paneHeading.innerHTML = headerName;

  let entitySpan = document.createElement('span');
  entitySpan.className = "currentEntity";
  paneHeading.appendChild(entitySpan);

  let buttonSpan = document.createElement('span');
  buttonSpan.setAttribute("style", "float:right");
  let buttonDiv = document.createElement('div');
  buttonDiv.id = layerId;
  buttonSpan.appendChild(buttonDiv);
  paneHeading.appendChild(buttonSpan);

  let reportBody = document.createElement('div');
  reportBody.id = "report-panel-body";
  reportBody.className = "card";
  reportBody.appendChild(paneHeading);

  let viewBody = document.createElement('div');
  viewBody.id = panelId;
  viewBody.className = "card-body"; //  fade in active";
  reportBody.appendChild(viewBody);

  let idName = view.id;
  let reportBodyContainer = document.createElement('div');
  reportBodyContainer.id = idName;
  reportBodyContainer.className = "classWithPad";
  reportBodyContainer.appendChild(reportBody);

  let reportCol = document.createElement('div');
  reportCol.id = "report-pane-col2";
  reportCol.className = "col col-sm-9";
  reportCol.appendChild(reportBodyContainer);

  return reportCol;
}

/**
 * @function setViewerPanelHeight changes the report body panel height during resizing.
 * 
 * @param {*} reportBodyDiv 
 * @param {*} reportPaneHeight 
 */
function setViewerPanelHeight(reportBodyDiv, reportPaneHeight) {
  let viewerValues = getViewerConst();
  if (viewerValues !== undefined) {
    // Only make adjustment if view is from system viewer and memory viewer
    let HeaderList = reportBodyDiv.find( ".card-header" );
    if (HeaderList.length === 0) { console.warn("Warning! Viewer report body has no header."); }
    let viewerHeaderHeight = HeaderList.eq(0).height();
    let newHeightValue = reportPaneHeight - viewerHeaderHeight - 48;

    let GID = viewerValues.gid;
    let elemId = viewerValues.elemId;
    $('#' + GID).css('height', newHeightValue);
    $('#' + elemId).css('height', newHeightValue);
    $('#' + elemId).css('width', $('#report-pane').innerWidth());  
  }
}