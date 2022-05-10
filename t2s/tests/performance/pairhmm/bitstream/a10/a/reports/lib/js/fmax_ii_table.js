"use strict";

// disable JSHint warning: Use the function form of "use strict".
// This warning is meant to prevent problems when concatenating scripts that
// aren't strict, but we shouldn't have any of those anyway.
/* jshint -W097 */

// columns names showed in table
var report_cols = ["", "Target II", "Scheduled fMAX", "Block II", "Latency", "Max Interleaving Iterations"];
var g_padding = "        ";

// utility function for repeat string Num times
function repeatStringNumTimes(string, times) {
  var repeatedString = "";
  while (times > 0) {
    repeatedString += string;
    times--;
  }
  return repeatedString;
}

// Returns substring before or after %L
function filterString(str,range) {
  var n = str.indexOf("%L");
  if (n != -1) {
    if (range === "after") return str.substring(n + 2);
    if (range === "before") return str.substring(0, n );
  } else {
    if (range === "after") return "";
    if (range === "before") return str;
  }
  return str;
}

// list is a UL, recursively call itself
function generateDetailsInList(details, list) {
  details.forEach(function(item){
    if (item.type == "text") {
      var li = document.createElement("LI");
      // if links
      var code_links = [];
      if ("links" in item) {
        item.links.forEach(function(link){
          var line_number = link.line;
          var file_name = link.filename;
          var createA = document.createElement("A");
          createA.appendChild(document.createTextNode(getShortFilename(file_name) + ":" + line_number));
          createA.setAttribute('href', "#");
          createA.setAttribute("onclick", "syncEditorPaneToLine(" + line_number + "," + "\"" + file_name + "\")");
          code_links.push(createA);
        });
      }
      var text = item.text;
      if ( (text.match(/%L/g) || []).length !== code_links.length ) {
        console.log("WARNING: Mismatch between number of %L's and links in details");
      }
      // Replace %L instances with debug links
      var i;
      for ( i = 0; (text.match(/%L/g) || []).length; i++) {
        var toInsert = filterString(text,"before");
        text = filterString(text,"after");
        li.appendChild(document.createTextNode(toInsert));
        li.appendChild(code_links[i]);
      }

      li.appendChild(document.createTextNode(text));

      var inner_ul = document.createElement("UL");
      if ("details" in item) {
        generateDetailsInList(item.details, inner_ul);
      }
      li.appendChild(inner_ul);
      list.appendChild(li);
    }

  });
}

function generateDetailsInfoForBB(details) {
  var ul = document.createElement("UL");
  generateDetailsInList(details, ul);
  // detailValues is a global var declared in main.js
  detailValues.push("<ul>" + ul.innerHTML + "</ul>");
  return ul;
}

// render loop information in table
function renderLoopInfo(func_data, basicblock_info, loop_header_block, top_div) {
  for (var j = 0; j < func_data[loop_header_block].length; j++) {
    var block_info = basicblock_info[func_data[loop_header_block][j]];

    var tr = document.createElement('TR');
    tr.className = "nohover selected-highlight";

    tr.setAttribute("clickable", 1);
    top_div.appendChild(tr);

    var comments = block_info.comment;
    var details = block_info.details;

    // Add detail in the panel
    // detailIndex is a global var declared in main.js
    if (details !== undefined) {
      generateDetailsInfoForBB(details);
      tr.setAttribute("index", ++detailIndex);
    } else {
      tr.setAttribute("index", 0);
    }

    // Basic Block Name:
    var td_bb_name = document.createElement("TD");
    var bb_name_p = document.createElement("P");
    bb_name_p.innerHTML = "Block: " + block_info.name;
    bb_name_p.setAttribute("STYLE", "display:inline");
    var heirachy_padding = document.createElement("P");
    heirachy_padding.innerHTML = repeatStringNumTimes(g_padding, block_info.loop_layer + 1);
    heirachy_padding.setAttribute("STYLE", "display:inline");
    //heirachy_padding.className = "BlockHierachyPadding";
    td_bb_name.appendChild(heirachy_padding);
    td_bb_name.appendChild(bb_name_p);
    tr.appendChild(td_bb_name);

    // If the block is a inner loop
    if (block_info.is_loop_header == "Yes" && block_info.name != loop_header_block) {
      var file_name = block_info.loop_location.details[0].links[0].filename;
      var line_number = block_info.loop_location.details[0].links[0].line;
      var loop_location_str = "(" + getShortFilename(file_name) + ":" + line_number + ")";
      bb_name_p.innerHTML = "<b>Loop: " + block_info.name + " " + loop_location_str + "</b>";

      heirachy_padding.innerHTML = repeatStringNumTimes(g_padding, block_info.loop_layer);
      td_bb_name.setAttribute("colspan", report_cols.length);
      renderLoopInfo(func_data, basicblock_info, block_info.name, top_div);
      
      tr.setAttribute("onclick", "syncEditorPaneToLine(" + line_number + "," + "\"" + file_name + "\")");
      // leave the rest of row empty
      continue;
    }

    // Target II
    var td_bb_target_ii = document.createElement("TD");
    var bb_target_ii = "Not Specified";
    if(block_info.target_ii !== undefined){
      bb_target_ii = block_info.target_ii;
    }
    td_bb_target_ii.appendChild(document.createTextNode(bb_target_ii));
    tr.appendChild(td_bb_target_ii);

    // Scheduled Fmax
    var td_bb_scheduled_fmax = document.createElement("TD");
    var is_fmax_bottleneck = block_info.is_fmax_bottleneck;
    td_bb_scheduled_fmax.appendChild(document.createTextNode(block_info.achieved_fmax));
    if (is_fmax_bottleneck == "Yes") {
      td_bb_scheduled_fmax.setAttribute("STYLE", "color:red");
    }
    tr.appendChild(td_bb_scheduled_fmax);

    // Scheduled II
    var td_bb_scheduled_ii = document.createElement("TD");
    td_bb_scheduled_ii.appendChild(document.createTextNode(block_info.achieved_ii));
    tr.appendChild(td_bb_scheduled_ii);

    // Latency
    var td_latency = document.createElement("TD");
    // Workaround Find the latency based on name based on block_info.name in schedule.json
    var blockLatency = findLatency(block_info.name);
    var latency_str = (blockLatency < 0) ? "Unknown" : blockLatency.toString();  // Error handling
    td_latency.appendChild(document.createTextNode(latency_str));
    tr.appendChild(td_latency);

    // Max Interleaving Support
    var td_interleaving = document.createElement("TD");
    var interleaving_str = block_info.max_interleaving;
    td_interleaving.appendChild(document.createTextNode(interleaving_str));
    tr.appendChild(td_interleaving);
  }
}

function renderFunc(function_name, func_data, basicblock_info, tableBody) {
  var tr_func = document.createElement('TR');
  tr_func.className = " selected-highlight";
  tableBody.appendChild(tr_func);
  tr_func.setAttribute("bgcolor", '#bfbfbf');

  var td_func_name = document.createElement("TD");
  var func_name = document.createElement("b");

  var func_data_hierachy = func_data.loop_hierachy;
  var func_loc_data = func_data.debug[0];

  // get a basic block fmax value from this function
  var basicblock_key = func_data_hierachy[Object.keys(func_data_hierachy)[0]][0];
  var target_fmax_str =  "Not Specified";
  if("target_fmax" in basicblock_info[basicblock_key])
    if(basicblock_info[basicblock_key].target_fmax != target_fmax_str)
      target_fmax_str  = basicblock_info[basicblock_key].target_fmax + " MHz";

  var file_name = func_loc_data.filename;
  var line_number = func_loc_data.line;
  var func_loc_str = "( " + getShortFilename(file_name) + ":" + line_number + " )";
  var func_fmax_str = "( Target Fmax : " + target_fmax_str +  " )";
  
  // function_prefix is global var declared in main.js
  func_name.innerHTML = function_prefix + ": " + function_name + " " + func_fmax_str + " " + func_loc_str;

  td_func_name.appendChild(func_name);
  td_func_name.setAttribute("colspan", report_cols.length);
  tr_func.appendChild(td_func_name);
  tr_func.setAttribute("onclick", "syncEditorPaneToLine(" + line_number + "," + "\"" + file_name + "\")");

  var loops = Object.keys(func_data_hierachy);

  for (var i = 0; i < loops.length; i++) {
    var loop_header_block = loops[i];
    if (loop_header_block in basicblock_info) {
      if (basicblock_info[loop_header_block].loop_layer > 1)
        continue;
    }

    var tr_loop = document.createElement('TR');
    tr_loop.className = "selected-highlight";
    tableBody.appendChild(tr_loop);
    // none loop is no in basicblock_info
    if (loop_header_block in basicblock_info) {
      // Loop Name:
      var loop_block = basicblock_info[loop_header_block];
      var td_loop_name = document.createElement("TD");
      var loop_name = document.createElement("b");

      // this access method is weird because
      // current compiler get link API generate link in a fixed JSON format
      var file_name_inner = loop_block.loop_location.details[0].links[0].filename;
      var line_number_inner = loop_block.loop_location.details[0].links[0].line;
      var loop_location_str = "(" + getShortFilename(file_name_inner) + ":" + line_number_inner + ")";
      loop_name.innerHTML = "Loop: " + loop_header_block + " " + loop_location_str;

      var heirachy_padding = document.createElement("P");
      heirachy_padding.innerHTML = repeatStringNumTimes(g_padding, 1);
      heirachy_padding.setAttribute("STYLE", "display:inline");

      td_loop_name.appendChild(heirachy_padding);
      td_loop_name.appendChild(loop_name);
      td_loop_name.setAttribute("colspan", report_cols.length);
      tr_loop.appendChild(td_loop_name);
      tr_loop.setAttribute("onclick", "syncEditorPaneToLine(" + line_number_inner + "," + "\"" + file_name_inner + "\")");
    }

    renderLoopInfo(func_data_hierachy, basicblock_info, loop_header_block, tableBody);
  }
}

function renderHeader() {
  var tableHeader = document.createElement('THEAD');
  var trHeader = document.createElement("TR");
  trHeader.setAttribute("ID", "table-header");
  tableHeader.appendChild(trHeader);

  // Insert table header titles
  for (var i = 0; i < report_cols.length; i++) {
    var th = document.createElement("TH");
    th.appendChild(document.createTextNode(report_cols[i]));
    trHeader.appendChild(th);
  }
  return tableHeader;
}

function renderBody(data) {
  var tableBody = document.createElement('TBODY');

  // first row
  // this is id identified in global env
  var first_row = document.createElement('TR');
  first_row.setAttribute("ID", "first-row");
  first_row.setAttribute("data-level", "0");

  for (var i = 0; i < report_cols.length; i++) {
    var td = document.createElement("TD");
    td.appendChild(document.createTextNode(report_cols[i]));
    first_row.appendChild(td);
  }
  tableBody.appendChild(first_row);

  var function_list = Object.keys(data.functions);
  for (var j = 0; j < function_list.length; j++) {
    var func_name = function_list[j];
    var func_data = data.functions[func_name];
    renderFunc(func_name, func_data, data.basicblocks, tableBody);
  }

  return tableBody;
}

function createTable(data) {
  var content_table = document.createElement("TABLE");

  // insert header
  var tableHeader = renderHeader();
  content_table.appendChild(tableHeader);

  // insert body
  var tableBody = renderBody(data);
  content_table.appendChild(tableBody);

  return content_table;
}

// entry function of Fmax/II report
function renderFmaxIITable(data) {
  var table = createTable(data);
  return table.innerHTML;
}

// find the latency from scheduleJSON
// returns -1 if fails to find the block
function findLatency(blockName) {
  var latency = -1;
  var found = false;
  Object.keys(scheduleJSON).forEach( function(funcID) {
    var func = scheduleJSON[funcID];
    if (func.hasOwnProperty("nodes")) {
      func["nodes"].forEach( function(block) {
        if (block.name === blockName) {
          found = true;
          latency = block.end - block.start;
          return;
        }
      });
    }
    if (found) return;
  });
  return latency;
}