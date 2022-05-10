"use strict";

// disable JSHint warning: Use the function form of "use strict".
// This warning is meant to prevent problems when concatenating scripts that
// aren't strict, but we shouldn't have any of those anyway.
/* jshint -W097 */

// Renders hierarchy tree for different graphs
// Error check of valid JSON should be done before we try rendering
function addHierarchyTree(view) {
  var ICON_LOC = "lib/external/fancytree/skin-win8/";
  var KERNEL_ICON = "kernelicon.png";
  var MEMORY_ICON = "memicon.png";
  var REG_ICON = "regicon.png";
  var ROM_ICON = "romicon.png";

  var viewValue = view.value;
  // TODO: further commonize after schema switch
  // if (viewValue === VIEWS.LMEM.value) lmem_graph.refreshGraph();
  // else if (viewValue === VIEWS.CSPV.value) cspv_graph.refreshGraph();

  var hierList = [];
  var treeNodes;
  var memTypeNameMap = { "memsys": "RAMs", "memgroup": "RAMs", "romsys": "ROMs", "reg": "Registers", "unsynth": "Optimized Away", "untrack": "Untrackable"};
  var memTypeExists = { "RAMs": false, "ROMs" : false , "Registers": false, "Optimized Away":false, "Untrackable":false};

  // Keep track of memory types for LMEM tree filtering
  function addMemType(type){
    if(memTypeExists[memTypeNameMap[type]] === false) memTypeExists[memTypeNameMap[type]] = true;
  }
  // Get memory system and its banks to fancy tree
  function getMemEntry(node, functionName) {

    addMemType(node.type);
    var entryIcon = (node.type === "memsys") ? MEMORY_ICON : ROM_ICON;
    var memEntry = { title: node.name, type: node.type, kernel: functionName, expanded: false, icon: ICON_LOC + entryIcon, children: [] };
    var bankID = 0;
    while (bankID < node.children.length) {
        var bankNode = node.children[bankID];
        // Render the first bank by default only. This is to prevent a large memory structure with many banks to hinder the users
        var isCheck = (bankID === 0);
        var bankName = bankNode.name;
        var bankEntry = { title: bankName, type: bankNode.type, selected:isCheck, bank: bankNode.name, lmem: node.name, kernel: functionName, expanded: true, icon: false };
        memEntry.children.push(bankEntry);
        bankID++;
    }
    return memEntry;
  }

  // This function filters out all memory types that are not selected
  function filterTreeNodes(tree, selectedElements){
    var types = [];
    // Get all selected types
    for( var t in memTypeNameMap ) {
      var label = memTypeNameMap[t];
      if(typeof label !== "undefined"){
        if(selectedElements.indexOf(label) > -1) types.push(t);
      }
    }

    // Filter tree by type
    tree.filterBranches(function(node){
      if(types.indexOf(node.data.type) > -1) return true;
      else return false;
    });
  }

  // backward compitable begin
  if (viewValue == VIEWS.LMEM.value) treeNodes = new_lmvJSON;
  else if (typeof treeJSON != "undefined" && !$.isEmptyObject(treeJSON)) treeNodes = treeJSON;
  else if (VIEWS.CSPV !== undefined && viewValue == VIEWS.CSPV.value) treeNodes = mavJSON;
  // end backward compitable

  var systemEntry = { title: "System", type: "system", hideCheckbox: true, expanded: true, icon: ICON_LOC + KERNEL_ICON, children: [] };
  hierList.push(systemEntry);
  // If there are kernel/components to render, then add it to the fancytree:
  if (treeNodes.nodes.length !== 0) {
    // Iterate through each element in the module: kernel|component
    // TODO: It'd be preferable to sort the functions, blocks, and clusters in
    // the backend instead of here.
    treeNodes.nodes.sort(function(a,b) { if (a.name < b.name) return -1; if (a.name > b.name) return 1; return 0; });
    treeNodes.nodes.forEach(function (element) {
      // Check whether it's either a kernel (OpenCL) or component (HLS)
      if (element.type == "kernel" || element.type == "component") {
        var functionName = element.name;
        var functionEntry = { title: functionName, type: element.type, hideCheckbox: true, expanded: true, icon: ICON_LOC + KERNEL_ICON, children: [] };
        // Iterate through each node: local memory, basic block, interface
        element.children.sort(function(a,b) { if (a.name < b.name) return -1; if (a.name > b.name) return 1; return 0; });
        element.children.forEach(function (node) {
          if (viewValue == VIEWS.GV.value) {
            if (node.type == "bb") {
              // Add all blocks to function, block and cluster viewer
              var bbName = node.name;
              var bbEntry = { title: bbName, type: node.type, nodeId: node.id, hideCheckbox: true, expanded: true, icon: ICON_LOC + KERNEL_ICON, children: [] };
              functionEntry.children.push(bbEntry);
              // Iterate through each: cluster
              if (node.children !== undefined) {
                node.children.sort(function(a,b) { if (a.name < b.name) return -1; if (a.name > b.name) return 1; return 0; });
                node.children.forEach(function (cluster) {
                  var cname = cluster.name;
                  var clusterEntry = { title : cname, type: cluster.type, nodeId: cluster.id, hideCheckbox: true, expanded: true, icon: ICON_LOC + MEMORY_ICON };
                  bbEntry.children.push(clusterEntry);
                });
              }
            }
          }
          else if (viewValue === VIEWS.LMEM.value) {
            // backward compitable begin
            if (node.type == "memtype" && node.name == "Local Memory") {  
              // Add all the local memories (RAMs and ROMs)
              node.children.filter(function(n){ return (n.type === "memsys" || n.type === "romsys");}).forEach(function (lmemNode) {
                functionEntry.children.push(getMemEntry(lmemNode, functionName));
              });
              // Add all the local memory groups
              node.children.filter(function(n){ return n.type === "memgroup";}).forEach(function (lmemGroup) {
                var memGroupEntry = { title: lmemGroup.name, type: lmemGroup.type, kernel: functionName, hideCheckbox: true, expanded: true, icon: ICON_LOC + MEMORY_ICON, children: [] };
                lmemGroup.children.forEach(function(lmemNode){
                  memGroupEntry.children.push(getMemEntry(lmemNode, functionName));
                });
                functionEntry.children.push(memGroupEntry);
              });
              // Add all registers, unsynthesized variables and untrackable variables
              node.children.filter(function(n){ return ((n.type === "reg") || (n.type === "unsynth") || (n.type === "untrack"));}).forEach(function(mnode) {
                addMemType(mnode.type);
                var iconLocation = (mnode.type === "reg") ? (ICON_LOC + REG_ICON) : false;
                var nodeEntry = { title: mnode.name, type: mnode.type, kernel: functionName, hideCheckbox: true, expanded: true, icon: iconLocation, details: [], children: [] };
                nodeEntry.details = mnode.details;
                functionEntry.children.push(nodeEntry);
              });

            } // end backward compitable
            else if (node.type == "memsys") {
              // Add the local memory system, i.e. banks and replication
              functionEntry.children.push(getMemEntry(node, functionName));
            } 
            else if (node.type == "memgroup") {
              // Add inlined and unrolled copies
              var memGroupEntry = { title: node.name, type: node.type, kernel: functionName, hideCheckbox: true, expanded: true, icon: ICON_LOC + MEMORY_ICON, children: [] };
              node.children.forEach(function(lmemNode){
                memGroupEntry.children.push(getMemEntry(lmemNode, functionName));
              });
              functionEntry.children.push(memGroupEntry);
            } 
            else if (node.type == "romsys") {
              // Add the ROM, i.e. banks and replication
              functionEntry.children.push(getMemEntry(node, functionName));
            } 
            else if (node.type == "reg" || node.type == "unsynth" || node.type == "untrack"){
              // Add registers, unsynthesized variables and untrackable variables
              addMemType(node.type);
              var iconLocation = (node.type === "reg") ? (ICON_LOC + REG_ICON) : false;
              var nodeEntry = { title: node.name, type: node.type, kernel: functionName, hideCheckbox: true, expanded: true, icon: iconLocation, details: [], children: [] };
              nodeEntry.details = node.details; // save details in tree element to load it into details pane when entry is clicked
              functionEntry.children.push(nodeEntry);
            }
          }
        });
        systemEntry.children.push(functionEntry);
      }
    });

    // Add fancy tree to the right element ID
    var elemId = "#" + getTreeIDName(view.id);
    $(elemId).fancytree({
      extensions: ["filter"],
      checkbox: true,
      selectMode: 3, // 1:single selection, 2:multiple selection, 3:hierarchical selection
      source: hierList,
      icon: true, // Disable the default icons
      clickFolderMode: 3, // 1:activate, 2:expand, 3:activate and expand, 4:activate (dblclick expands)
      filter: {
        fuzzy: false,
        nodata: false,  // Don't display a 'no data' status node if result is empty
        mode: "hide"  // Remove unmatched nodes (use "dimm" to grey them out instead)
      },

      // This event handler is entered before data.node is about to be (de)activated
      // Only allow memsys, bank, reg, romsys type elements to be activated in LMEM viewer
      beforeActivate: function(event, data) {
        var curElemID = this.getAttribute('id');
        if (curElemID === getTreeIDName(VIEWS.LMEM.id)){
          if(data.node.data.type !== "memsys" && data.node.data.type !== "bank" && data.node.data.type !== "reg" && data.node.data.type !== "unsynth" && data.node.data.type !== "untrack" && data.node.data.type !== "romsys"){
            data.node.setFocus(false);
            return false;
          }
        }
      } ,
      // Click event handler: When user clicks inside fancytree, data.node will contain the clicked node
      // data.targetType will contain the region ("checkbox","expander","icon","prefix","title")
      click: function (event, data) {
        var curElemID = this.getAttribute('id');
        if (curElemID === getTreeIDName(VIEWS.GV.id)) {
          var nodeId = data.node.data.nodeId;
          if (pipelineJSON[nodeId] !== undefined) {
            $("#GVG").parent().find(".currentEntity").text(": " + data.node.parent.title + " > " + data.node.title);
            renderGraph(pipelineJSON[nodeId], "GV", nodeId);
          } else if (blockJSON[nodeId] !== undefined) {
            $("#GVG").parent().find(".currentEntity").text(": " + data.node.title);
            renderGraph(blockJSON[nodeId], "GV", nodeId);
          } else if (data.node.data.type === "system") {
            if (systemJSON !== undefined && !$.isEmptyObject(systemJSON)) {
              // HLS uses new backend infrastructure to serilize system graph --> systemJSON
              $("#GVG").parent().find(".currentEntity").text("");
              renderGraph(systemJSON, "GV", 0);
            } else if (mavJSON !== undefined && !$.isEmptyObject(mavJSON)) {
              // OpenCL uses old backend infrastructure to serialize system graph --> mavJSON
              $("#GVG").parent().find(".currentEntity").text("");
              renderGraph(mavJSON, "GV", 0);
            }
          } else {
            if (flow == FLOWS.HLS) {
              // HLS: Component viewer uses old infrastructure --> mavJSON
              // TODO: when the new infrastructure is used for the HLS function
              // viewer, we should be simply call renderGraph(funcJSON, "GV").
              $("#GVG").parent().find(".currentEntity").text(": " + data.node.title);
              var comp_name = data.node.title;
              // backward compitable begin
              // Pass the name of the component into the rendering
              if (isValidSystemViewer) {
                $('#GVG').html(GRAPH_LOADING_DIV);
                setTimeout(function() {
                  cspv_graph = new StartGraph(mavJSON, "CSPV", comp_name);
                  cspv_graph.refreshGraph();
                }, 20);
              } // end backward compitable
            } else {
              // OpenCL: no function viewer yet
              renderGraph(undefined, "GV", -2);
            }
          }
        }
        else if (curElemID === getTreeIDName(VIEWS.LMEM.id)) {

          // To avoid rendering twice when clicking on a checkbox (will enter the select function instead)
          // only render if "title" (element name) is clicked and if the node is not active
          // This check is important to prevent rendering twice when a checkbox is clicked
          if(data.node.isActive()) return;
          if(data.targetType !== "title") return;

          var lmem_name, kernel_name, bank_name;
          var bankList;
          // Check if a local memory or bank is clicked
          if (data.node.data.type === "memsys" || data.node.data.type === "romsys") {
            // Pass the name of the local memory and kernel into the rendering
            lmem_name = data.node.title;
            kernel_name = data.node.data.kernel;

            // Query for list of selected banks that belong to this local memory and kernel and pass them as a list to render the graph
            var activeNode = data.tree.activeNode;
            var selNodes = data.tree.getSelectedNodes().filter(function (n) {
              return (n.data.type === "bank" && n.data.kernel === kernel_name && n.data.lmem === lmem_name);
            });
            bankList = jQuery.map(selNodes, function(a){ return a.data.bank;});
            renderGraphForBank(new_lmvJSON, kernel_name, lmem_name, bank_name, false, bankList);
          } else if (data.node.data.type === "bank") {
            // Pass the name of the local memory, bank, and kernel into the rendering
            kernel_name = data.node.data.kernel;
            lmem_name = data.node.data.lmem;
            bank_name = data.node.data.bank;
            // Call renderer with replFlag set to true
            bankList = [];
            renderGraphForBank(new_lmvJSON, kernel_name, lmem_name, bank_name, true, bankList);
          } else if (data.node.data.type === "reg" || data.node.data.type === "unsynth" || data.node.data.type === "untrack") {
            renderNoGraphForType("#LMEMG", data.node.title, data.node.data.type, data.node.data.details);
          }
        }
      } ,
      // Select event handler: When user (de)selects an entry inside the fancytree, data.node will
      // contain the (de)selected node. Render the LMEM view with updated list of selected banks
      select: function(event, data) {
          var curElemID = this.getAttribute('id');
          
          if (curElemID === getTreeIDName(VIEWS.LMEM.id)) {
            // Check if a local memory checkbox is clicked
            var activeNode = data.tree.activeNode;

            // Deactivate previously active node when any checkbox is clicked to avoid mismatch between rendered and highlighted elements
            if(typeof activeNode !== "undefined" && activeNode) {activeNode.setFocus(false); activeNode.setActive(false);}

            if (data.node.data.type === "memsys" || data.node.data.type === "romsys" || data.node.data.type === "bank") {
              var lmem_name, kernel_name, bank_name;
              
              // Pass the name of the local memory and kernel into the rendering
              kernel_name = data.node.data.kernel;

              if (data.node.data.type === "memsys" || data.node.data.type === "romsys") {
                  lmem_name = data.node.title;
              } else {
                  lmem_name = data.node.data.lmem;
              }

              // Query for list of selected banks and pass them as a list to render the graph
              var selNodes = data.tree.getSelectedNodes().filter(function (n) {
                          return (n.data.type === "bank" && n.data.kernel === kernel_name && n.data.lmem === lmem_name);
                      });
              var bankList = jQuery.map(selNodes, function(a){ return a.data.bank;});
              renderGraphForBank(new_lmvJSON, kernel_name, lmem_name, bank_name, false, bankList);  
          }
        }
      }
    });
    
    // Add connectors to fancytree
    $(".fancytree-container").addClass("fancytree-connectors");

    // Add filtering for memory types
    if (viewValue === VIEWS.LMEM.value) {
      var lmem_tree = $(elemId).fancytree("getTree");
      var rootNode = lmem_tree.rootNode;

      // Sort nodes in the tree (and subtrees) alphanumerically (e.g. Bank 0 -> Bank 1 -> Bank 2 ...)
      var sortingFunc = function(a,b){
        var a_title = a.title.toLowerCase(),
            b_title = b.title.toLowerCase();
        return a_title.localeCompare(b_title, 'en', {numeric: true});
      };

      if(typeof rootNode !== "undefined" && rootNode) rootNode.sortChildren(sortingFunc,true);

      // All memory types are selected by default, this array tracks selected types
      var filterOptions = [];

      // Add filtering to fancytree for all memtypes that exist
      var treeSelection = "";
      for( var typeLabel in memTypeExists ) {
        if(memTypeExists[typeLabel] === true){
          var isChecked = "checked"; // show all memory types by default
          filterOptions.push(typeLabel);
          treeSelection += "<li><a href=\"#\" class=\"small\" data-value=\"" + typeLabel + "\" tabIndex=\"-1\"><input type=\"checkbox\" " + isChecked + "/>&nbsp;&nbsp;&nbsp;" + typeLabel + "</a></li>";
        }
      }

      if(treeSelection === ""){
        // If no entries exist, then hide filter dropdown menu
        var treeButtonId = getTreeIDName(view.id) + "-button";
        var treeButtonElement = document.getElementById(treeButtonId)
                                .style.visibility = "hidden";
      } else {
        // Insert filter dropdown menu into html
        var treeFilterId = elemId + "-filter";
        $(treeFilterId).html(treeSelection);
        
        // Dropdown Filter menu event handler
        $(treeFilterId + ' a').on( 'click', function( event ) {
          var $target = $( event.currentTarget ),
              val = $target.attr( 'data-value' ),
              $inp = $target.find( 'input' ),
              idx;

          // setTimeout(fn,0) here is needed to allow the browser to catch up and update
          // the states of the dropdown menu checkboxes in order to achieve optimal rendering
          if ((idx = filterOptions.indexOf(val)) > -1 ) {
            filterOptions.splice( idx, 1 );
            setTimeout( function() { $inp.prop('checked', false); }, 0);
          } else {
            filterOptions.push(val);
            setTimeout( function() { $inp.prop('checked', true); }, 0);
          }

          $( event.target ).blur();
          filterTreeNodes(lmem_tree,filterOptions);
          
          // Need to return false to keep the dropdown menu open after selection
          // to allow the user to (de)select multiple types at once
          return false;
        });

        // Show all memory types
        filterTreeNodes(lmem_tree, filterOptions);
      }
    }

    return true;
  } else {

    if(viewValue === VIEWS.LMEM.value) {
      // If no entries exist, then hide filter dropdown menu
      var buttonId = getTreeIDName(view.id) + "-button";
      document.getElementById(buttonId)
              .style.visibility = "hidden";
    }

    return false;
  }
}

function getTreeIDName(id) {
  return id + "-tree";
}
