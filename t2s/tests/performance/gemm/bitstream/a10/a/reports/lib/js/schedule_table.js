"use strict";

// disable JSHint warning: Use the function form of "use strict".
// This warning is meant to prevent problems when concatenating scripts that
// aren't strict, but we shouldn't have any of those anyway.
/* jshint -W097 */

function formatNumberLength(num, length) {
  var r = "" + num;
  while (r.length < length) {
    r = "0" + r;
  }
  return r;
}

function showRow(index){
  var row = d3.select('#' + 'table_row_' + index);
  if (row[0][0]) {
    var dis = row.style("display");
    if (dis == "none") {
      row.style("display", "table-row");
    }
  }
}

function hideRow(index){
  var row = d3.select('#' + 'table_row_' + index);
  if (row[0][0]) {
    var dis = row.style("display");
    if (dis == "table-row") {
      row.style("display", "none");
    }
  }  
}

function getSelectedBasicblockIndex() {
  var elm_basicblock = document.getElementById('basicblock_select');
  var basicblock_id = elm_basicblock.options[elm_basicblock.selectedIndex].value;
  return basicblock_id;
}

function getSelectedFunctionIndex() {
  var elm_func = document.getElementById('function_select');
  var func_id = elm_func.options[elm_func.selectedIndex].value;
  return func_id;
}

function tabulate(eid, columns, data) {
  var table = d3.select(eid).append('table').attr('id', 'main_table').attr('class', 'schedule_table');
  var thead = table.append('thead');
  var tbody = table.append('tbody');

  thead.append('tr')
    .selectAll('th')
    .data(columns).enter()
    .append('th')
    .text(function (column) {
      return column;
    });

  var func_id = getSelectedFunctionIndex();
  var basicblock_id = getSelectedBasicblockIndex();

  console.log(func_id, basicblock_id);

  var block_indexs = data.hierarchy[func_id][basicblock_id];
  var block_info = data.data;

  // create a row for each object in the data
  var rows = tbody.selectAll('tr')
    .data(block_indexs)
    .enter()
    .append('tr')
    .attr('id', function (d, i) {
      var array_index = block_info[d].array_index;
      return "table_row_" + array_index;
    })
    .attr('display', function (d, i) {
      if (i === 0) {
        return "table-row";
      }
      return "none";
    })
    .text(function (d, i) {
      return "";
    });

  // create a Cell in each row for each column
  var cells = rows.selectAll('td')
    .data(function (row, i) {
      return columns.map(function (column, j) {
        var index = block_indexs[i];
        var node_name = "";
        if (j === 0) {
          node_name = i;
        }
        if (j == 1) {
          var indent = "";
          node_name = indent + block_info[index].nice_name;
        }
        if (block_info[index].start_cycle == j - 2 || block_info[index].end_cycle == j - 2) {
          node_name = j - 2;
        }
        return {
          column: column,
          value: node_name,
          row: index,
          col: j
        };
      });
    })
    .enter()
    .append('td')
    .attr("class", function (d, i) {
      if (d.col === 0) return "control_col";
      if (d.col == 1) return "node_name_col";
      if (block_info[d.row].start_cycle <= d.col - 2 && block_info[d.row].end_cycle >= d.col - 2)
        return "scheduled_cell";
      return "unscheduled_cell";
    })
    .text(function (d, i) {
      return d.value;
    });

  //hideRow(block_info);
  return table;
}

// Filter Data for Display in Select Bar
function filterDataForSelect(data) {
  var i;
  var select_data = [];
  for (i = 0; i < data.length; i++) {
    if (data[i].can_be_top_level) {
      select_data.push(data[i]);
    }
  }
  return select_data;
}

function updateBasicBlockSelect(eid, data) {
  var node_info_array = data.data;
  var func_id = getSelectedFunctionIndex();
  var basicblock_index_list = Object.keys(data.hierarchy[func_id]);

  if (d3.select('#basicblock_select').empty()) {
    d3.select(eid)
      .append('select')
      .attr('name', 'basicblock_select')
      .attr('id', 'basicblock_select')
      .attr('width', "500px");
  }

  d3.select('#basicblock_select')
    .selectAll('option').remove();

  d3.select('#basicblock_select')
    .selectAll('option').data(basicblock_index_list).enter().append('option')
    .attr('value', function (d, i) {
      return basicblock_index_list[i];
    })
    .text(function (d, i) {
      var index = parseInt(basicblock_index_list[i], 10);
      return node_info_array[index].nice_name;
    });
}

function initializeSelectBar(eid, data) {
  if (!d3.select('#function_select').empty()) return;

  var node_info_array = data.data;
  var function_index_list = Object.keys(data.hierarchy);
  var select_bar = d3.select(eid)
    .append('select')
    .attr('name', 'function_select')
    .attr('id', 'function_select')
    .attr('width', '500px');

  d3.select("#function_select")
    .selectAll('option').data(function_index_list).enter().append('option')
    .attr('value', function (d, i) {
      return function_index_list[i];
    })
    .text(function (d, i) {
      var index = function_index_list[i];
      return node_info_array[index].nice_name;
    });

  d3.select('#function_select')
    .on('change', function () {
      updateBasicBlockSelect(eid, data);
    });

  updateBasicBlockSelect(eid, data);
}

function initializeSelectButton(eid, data, o_data) {
  if (!d3.select('#select_button').empty()) return;
  d3.select(eid)
    .append('button')
    .attr('id', 'select_button')
    .text('GO')
    .on('click', function () {
      var basicblock_id = getSelectedBasicblockIndex();
      var start = data[basicblock_id].start_cycle;
      var end = data[basicblock_id].end_cycle;

      var columns = ["", "Node Name"];
      var i = 0;
      var len = end.toString().length;
      for (; i <= end; i++) {
        var t = "C" + formatNumberLength(i, len);
        columns.push(t);
      }
      d3.select("#main_table").remove();
      tabulate(eid, columns, o_data); // 2 column table
    });
}

function initializeSearchBar(eid, data) {
  if (!d3.select('#search_model').empty()) return;
  d3.select(eid)
    .append('input')
    .attr('name', 'search_pattern')
    .attr('id', 'search_model');

  d3.select(eid)
    .append('button')
    .attr('id', 'search_button')
    .text('SEARCH')
    .on('click', function () {
      if(d3.select("#main_table").empty()) return ;
      var elm = document.getElementById('search_model');
      var pattern = elm.value.replace(/\s/g, '').toLowerCase();
      // pattern match value
      var func_id = getSelectedFunctionIndex();
      var basicblock_id = getSelectedBasicblockIndex();
      var block_indexs = data.hierarchy[func_id][basicblock_id];
      var block_info = data.data;

      var k = 0;
      for (; k < block_indexs.length; k++) {
        var index = block_indexs[k];
        var index_in_table = block_info[index].array_index;
        var name = block_info[index].nice_name;
        var to_match = name.replace(/\s/g, '').toLowerCase();

        if(to_match.indexOf(pattern) != -1){
          showRow(index_in_table);
        } else {
          hideRow(index_in_table);
        }
      }

    });

}

function renderScheduleTable(eid, data) {
  var node_info = data.data;
  var function_index_list = Object.keys(data.hierarchy);

  // Main Logic Start Here
  //
  // Filter Blocks that can be in Select Bar
  var select_data = filterDataForSelect(node_info);

  initializeSelectBar(eid, data);
  initializeSelectButton(eid, node_info, data);
  initializeSearchBar(eid, data);
}
