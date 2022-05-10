"use strict";

// The chart below is created by refering to this example
// https://bl.ocks.org/smokbel/c23acfed102dbcbaab0df7e07fa6bb9a
// with the MIT License

// Global variable:
// Commonly used by HLD report
var scheduleData;
var sv_chart;

var xAxis, x2Axis, yAxis;
var barClickedDown;
var startIndex, latestData, latestRegion;
var scheduleXTickWidth = 28;
var scheduleBarHeight = 26;
var scheduleBarSpacing = 6;
var scheduleMaxXValue;  // the maximum value for x-axis
var scrollUpDownSize = 1;  // number of element to change everytime up and down was pressed. Default max is 3
var XAxisIntervalSize = 1;  // Integer, number of clock cycle per division

/**
 * @function 100 getMaxNodeCount computes the number of nodes that can fit veritically
 * margin 50 for top and bottom, 32 from 26 bar height plus 6 for spacing
 *
 * @param {Intger} divHeight is height of the div that contains floating bar graph
 * @return number of nodes the div can fit
 */
function getMaxNodeCount(divHeight) {
  return Math.floor( (divHeight-150)/(scheduleBarHeight+scheduleBarSpacing) );
}

function fitInDiv(divWidth, dataRange) {
  let requiredWidth = Math.ceil(dataRange / XAxisIntervalSize) * scheduleXTickWidth;
  return (divWidth > requiredWidth);
}

var ScheduleViewer = function(opts) {
  // load in arguments from config object
  this.data = opts.data;
  this.region = this.createXAxis(this.data);
  this.element = opts.element;
  
  if (this.region === undefined || this.region.length === 0) {
    console.warn("Warning! Incorrect axis.");
    return;
  }
  // create the ScheduleViewer
  this.draw();
}

/**
 * @function createXAxis Iterate through the schedule data and find all the clusters (aka static scheduled region),
 * colour the static region gray. Restart the counter for beginning of the region and end region.
 * Possible issue may be a corner case with runOnce.
 * 
 * @returns axis object. Example: { "start": 1, "end":16, "latency": 15, "color": "#D3D3D3" };
 */
ScheduleViewer.prototype.createXAxis = function() {
  let colorMap = { "static" : "#D3D3D3", // static schedule gray
                   "other" : "#FFFFFF"   // dynamic schedule white
                 };

  let staticBlocksAndClustersList = [];  // A list that contains static block and clusters only
  this.data.forEach( function(curNode) {
    if (curNode.type === 'cluster') {  // TODO: add blocks with no IP that can generate stall's, i.e. LSU
      if (staticBlocksAndClustersList.length > 0) {
        let prevCluster = staticBlocksAndClustersList[staticBlocksAndClustersList.length-1];
        if (prevCluster.start === curNode.start) {
          staticBlocksAndClustersList[staticBlocksAndClustersList.length-1] = curNode;
          return;
        }
      }
      staticBlocksAndClustersList.push(curNode);
    }
  });

  let axis = [];
  // No cluster
  if (staticBlocksAndClustersList.length === 0) {
    let curRegion = { "start": d3.min(this.data, function(d) { return Math.floor(d.start) }) };  // initial condition
    let lastInstEnd = d3.max(this.data, function(d) { return Math.ceil(d.end) });
    curRegion.end     = lastInstEnd;
    curRegion.color   = colorMap["other"];
    axis.push(curRegion);
    return axis;
  }

  let curRegion = { "start": this.data[0].start };  // initial condition
  for (let ci=0 ; ci<staticBlocksAndClustersList.length ; ci++) {
    let clusterNode = staticBlocksAndClustersList[ci];
    if (clusterNode.start > curRegion.start) {
      // There's a gap, i.e. due to stallable nodes
      // set curRegion as dynamic scheduled and create a new static
      curRegion.end = clusterNode.start;
      curRegion.color = colorMap["other"];
      axis.push(curRegion);
      curRegion = { "start": clusterNode.start };
    }
    // Make a new static schedule
    curRegion.end     = clusterNode.end;
    curRegion.color   = colorMap["static"];
    axis.push(curRegion);
    // update to next region
    curRegion = { "start": clusterNode.end };
  }
  // handle boundary condition, last instruction is stallable
  let lastInstEnd = this.data[this.data.length-1].end;
  if (curRegion.start < lastInstEnd) {
    curRegion.end     = lastInstEnd;
    curRegion.color   = colorMap["other"];
    axis.push(curRegion);
  }
  return axis;
}

ScheduleViewer.prototype.clearPopover = function() {
  d3.selectAll(".popover").remove();  // remove existing popover
}

ScheduleViewer.prototype.draw = function() {
  this.clearPopover();

  let divObj = $('#' + this.element.id);

  // Add scroll up and down button
  let upIcon = document.createElement("span");
  upIcon.className = "body glyphicon";
  upIcon.innerHTML = "&#xe093;";
  let upButton = document.createElement("button");
  upButton.type = "button";
  upButton.id = "schUpButton";
  if (this.data[0].id === scheduleData[0].id) { upButton.disabled = true; }  // disable button if first element is already top
  upButton.appendChild(upIcon);
  let buttonsDiv = document.createElement("div");
  buttonsDiv.className = "text-left";
  buttonsDiv.appendChild(upButton);
  divObj.append(buttonsDiv);

  let downIcon = document.createElement("span");
  downIcon.className = "body glyphicon";
  downIcon.innerHTML = "&#xe094;";
  let downButton = document.createElement("button");
  downButton.type = "button";
  downButton.id = "schDownButton";
  if (getMaxNodeCount(divObj.height()) >= scheduleData.length ||
    this.data[this.data.length-1].id === getLastNonHiddenNodeID()) {
    // disable down when the screen can load more nodes or I am at the bottom
    downButton.disabled = true; 
  }
  downButton.appendChild(downIcon);
  buttonsDiv.appendChild(downButton);  // Put near the top for alpha level

  // add zoom button to handle the case where nodes below has large latency cause the x-axis scale too be too large
  let zoomInIcon = document.createElement("span");
  zoomInIcon.className = "body glyphicon";
  zoomInIcon.innerHTML = "&#xe015;";
  let zoomInButton = document.createElement("button");
  zoomInButton.type = "button";
  zoomInButton.id = "schZoomInButton";
  zoomInButton.disabled = (XAxisIntervalSize === 1) ? true : false;
  zoomInButton.appendChild(zoomInIcon);
  buttonsDiv.appendChild(zoomInButton);
  let zoomOutIcon = document.createElement("span");
  zoomOutIcon.className = "body glyphicon";
  zoomOutIcon.innerHTML = "&#xe016;";
  let zoomOutButton = document.createElement("button");
  zoomOutButton.type = "button";
  zoomOutButton.id = "schZoomOutButton";
  zoomOutButton.appendChild(zoomOutIcon);
  buttonsDiv.appendChild(zoomOutButton);

  // calculate width, height
  // make a minimum size if # of nodes is too less, i.e. if I have collapsed everything
  var width = (divObj.width() > 0) ? divObj.width() : 800;
  let height = calculateHeight(this.data);

  var svg = d3.select(this.element).append("svg")
              .attr("width", width)
              .attr("height", height);

  // Margin definitions: top=50 and bottom=100 for axis label
  // Left margin have instruction labels, so truncate the names or reduce the margin if all short names
  let leftMargin = truncateLongNames(this.data);
  this.margin = {top: 50, right: 30, bottom: 100, left: leftMargin},
  this.width = width - this.margin.left - this.margin.right,
  this.height = height - this.margin.top - this.margin.bottom; 

  this.plot = svg.append("g")
                 .attr("transform", "translate(" + this.margin.left + "," + this.margin.top + ")") ;		

  // All functions to draw scheduleViewer
  this.calculateXAxisAttributes();

  // Enable when X-axis max less scheduleMaxXValue
  zoomOutButton.disabled = (this.XMaxDataValue >= scheduleMaxXValue);

  this.createScales();
  this.addAxes();
  this.drawGridLine();
  this.updateBackgroundColor();
  this.displayAxisTitle();
  this.renderBar();
  this.renderBarAction();
  this.renderAxesAction();

  // Returns the height based on number of nodes or use maximum
  function calculateHeight(nodeList) {
    if (divObj.height() <= 0) { 
      console.warn("Warning! Element height is 0.");
      return undefined;
    }
    // compare if there are more nodes than can display too less
    let maxNodeCount = getMaxNodeCount(divObj.height());
    if (nodeList.length < maxNodeCount) {
      return nodeList.length*40 + 150;
    }
    return divObj.height();
  }

  // truncate long names and returns the left margin use by the chart
  // Set max equals 15% of width and long words should be truncated. Design can always overwrite with less.
  function truncateLongNames(nodeList) {
    // reset all short name
    nodeList.forEach( function(nodeObj) {
      if (nodeObj.hasOwnProperty("short name")) {
        delete nodeObj["short name"];
      }
    });

    // set 20% of width or a readable minimum
    let maxWidth = 0.2*(width-50);
    maxWidth = (maxWidth < 80) ? 80 : maxWidth;
    // Find the maximum string length and see if it fits
    let maxNLenNode = nodeList.reduce(function(total, a) {
      if (a.name.length > total.name.length) { return a; }
      return total;
    });
    let maxNLen = maxNLenNode.name.length * 10;  // string to pixel conversion factor 1char:10px
    if (maxNLen <= maxWidth) { return maxNLen; }  // everything fits

    let maxStrLen = Math.floor(maxWidth/10);
    nodeList.forEach( function(nodeObj) {
      if (nodeObj.name.length > maxStrLen) {
        nodeObj["short name"] = "..." + 
                                nodeObj["name"].substring(nodeObj.name.length-maxStrLen+3, nodeObj.name.length);
      }
    });
    return maxWidth;
  }
}

// Determine the tickSize based on interval size, calculate max value based on tickSize
// Set this.XNumTicks and this.XMaxDataValue
// TODO: this can be extended to do horizontal scrolling
ScheduleViewer.prototype.calculateXAxisAttributes = function() {
  // Determine number of ticks based on data
  // The maximum and minimum value on x-axis is rounded up to the nearest integer from the data
  let minData = d3.min(this.data, function(d) { return Math.ceil(d.start) });
  let maxData = d3.max(this.data, function(d) { return Math.ceil(d.end) });
  let dataRange = Math.ceil(maxData - minData);
  this.XNumTicks = dataRange;
  let dataNotFit = fitInDiv(this.width, this.XNumTicks);
  if (XAxisIntervalSize > 1) {
    // reduce the number of ticks
    this.XNumTicks = Math.ceil(this.XNumTicks / XAxisIntervalSize);
  }
  if (!dataNotFit) {
    // width doesn't fit the whole thing, so reduce the max value
    let XMaxValue = Math.floor(this.width / scheduleXTickWidth);
    this.XMaxDataValue = XMaxValue * XAxisIntervalSize;
    this.XNumTicks = XMaxValue;
  }
  else {
    // everything fits, now scale max value as it zooms out
    this.XMaxDataValue = maxData + dataRange * (XAxisIntervalSize-1);
    if (this.XMaxDataValue > scheduleMaxXValue) {
      this.XMaxDataValue = scheduleMaxXValue;  // boundary condition, cap max
    }
  }
}

ScheduleViewer.prototype.createScales = function() {
  this.y = d3.scale.ordinal()
                  .rangeRoundBands([0, this.height])
                  .domain(this.data.map(function(d) { return d.id }));

  this.x = d3.scale.linear()
                  .range( [0, this.width] )
                  .domain( [d3.min(this.data,function(d){ return d.start; }), 
                    this.XMaxDataValue ] );
}

ScheduleViewer.prototype.addAxes = function() {
  var _this = this;

  // create and append axis elements
  xAxis = d3.svg.axis()
              .scale(this.x)
              .orient("top")
              .ticks(this.XNumTicks)
              .tickFormat(function(d) {
                // Show region's clock cycle
                // 1st find the region it is in, 2nd subtract from start
                let r = 0;
                let found = false;
                for (let y=0; y< _this.region.length; y++) {
                  if(d < _this.region[y].start) { 
                    r = (y>0) ? y-1 : 0; 
                    found = true;  // found the region
                    break; 
                  }
                }
                if (!found) { 
                  // means last one
                  r = _this.region.length-1;
                }
                if (r === 0 && _this.region[0].start !== 0 && _this.region[0].color === "#FFFFFF") {
                  // corner case when 1st non-static region not 0
                  return d;
                }
                return d - _this.region[r].start;
              });

  x2Axis = d3.svg.axis()
                 .scale(this.x)
                 .orient("bottom")
                 .ticks(this.XNumTicks)
                 .tickFormat(function(d) {return d;});

  yAxis = d3.svg.axis()
                .scale(this.y)
                .orient("left")
                .tickFormat(function(d) {
                    // To map by ID
                    let nodeObj = _this.getNode(d);
                    if (nodeObj) {
                      return (nodeObj.hasOwnProperty("short name")) ? nodeObj["short name"] : nodeObj["name"];
                    } else {
                      console.log("Warning! Can't find node in data");
                      return "Unknown name"; // Else return original data
                    }
                });

  this.plot.append("g")
          .attr("class", "x axis")
          .call(xAxis);

  this.plot.append("g")
          .attr("class", "x2 axis")  
          .attr("transform", "translate(0," + this.height + ")")
          .call(x2Axis);

  this.plot.append("g")
          .attr("class", "y axis")
          .call(yAxis);
}

ScheduleViewer.prototype.drawGridLine = function(){
    // Draw grid on x axis
    this.plot.append("g")
        .attr("class", "grid") 
        .call(xAxis
            .tickSize(-this.height, 0, 0)
            .tickFormat("")
        )
}

ScheduleViewer.prototype.updateBackgroundColor = function() {
    var _this = this;

  // Loop for regions and assign background colour
  if(_this.region.length>0){
    for( var i=0 ; i < _this.region.length; i++) {
      this.plot.append("rect")
      .attr("x", _this.x(_this.region[i].start) )
      .attr("y", 0)
      .attr("width", _this.x(_this.region[i].end) - _this.x(_this.region[i].start))
      .attr("height", this.height)
      .attr("fill", _this.region[i].color)
      .attr("opacity", 0.2);
    }
  }
} 

ScheduleViewer.prototype.displayAxisTitle = function(){	
  var _this = this;

  //xAxis title on top 
  _this.plot.append("g")	
    .attr("transform", "translate(0,-10)")		
    .append("text")
    .attr("class", "axisTitle")
    .attr("x", this.width-200)  
    .attr("y", -this.margin.top-5)
    .attr("dy", "2.8em")
    .text("Cluster instruction schedule cycle");

  //xAxis title on bottom 
  _this.plot.append("g")	
    .attr("transform", "translate(0," + this.height + ")")	
    .append("text")
    .attr("class", "axisTitle")
    .attr("x", this.width-120)  
    .attr("y", -this.margin.top+55)
    .attr("dy", "2.8em")
    .text("Absolute clock cycle");
}

ScheduleViewer.prototype.renderBar = function(){	 
  var _this = this;

  this.plot.selectAll(".bar")
           .data(this.data)
           .enter()
           .append("rect")
           .attr("id", function(d) { return "bar-" + d.id; })
           .attr("y", function(d) { return _this.y(d.id); })
           .attr("height", scheduleBarHeight)
           .attr("x", function(d) { return _this.x(d.start); })
           .attr("width", function(d) {
             // not show if start > max, chop width if end > max, or it's okay
             if(d.start>=_this.XMaxDataValue) { return 0; }
             else if (d.end>_this.XMaxDataValue) { return _this.x(_this.XMaxDataValue)-_this.x(d.start); }
             else { return _this.x(d.end)-_this.x(d.start); }
           })
           .attr("fill", function(d) {
              // Show color by type variable
              var colorType = {
                'bb': "#003C71",           // indigo
                'cluster': "#0071C5",      // blue
                'inst': "#00AEEF",         // light blue
                'limiterBlock': "#FFA300", // orange
                'speculation': "#505050",  // dark gray
                'iteration': "#009933"     // green
              }
              if (colorType.hasOwnProperty(d.type)) {
                return colorType[d.type];
              } else {
                return "#C4D600";
              }
          })
          .style("cursor", function(d) { if (d.hasOwnProperty("details")) return "pointer"})
          .on('click', function(d) {
            changeDetailsPane(getDetails(d), d.name);  // Show details
          });
}

ScheduleViewer.prototype.renderBarAction = function(){	 
	var _this = this;
  _this.plot.selectAll(".editableBar")
            .on("click", function(d) { clickBar(d); });
}

ScheduleViewer.prototype.renderAxesAction = function() {
  var _this = this;

  // Up, down, zoom buttons
  let upButton = d3.select("#schUpButton");
  upButton.on('click', function(d) { 
    let nodeIndex = getNodeIndex(_this.data[0].id);
    // minus to shift up
    if (nodeIndex >= scrollUpDownSize) { 
      nodeIndex -= scrollUpDownSize;
      document.getElementById("schDownButton").disabled = false;  // enable down
    }
    top_node_id = scheduleData[nodeIndex].id;  // update new top
    startScheduleChart(false);
  });
  let downButton = d3.select("#schDownButton");
  downButton.on('click', function(d) { 
    let nodeIndex = getNodeIndex(_this.data[0].id);
    // plus to shift down
    if (nodeIndex < (scheduleData.length-scrollUpDownSize-1)) { 
      nodeIndex += scrollUpDownSize;
      document.getElementById("schUpButton").disabled = false;  // enable up
    }
    top_node_id = scheduleData[nodeIndex].id;  // update new top
    startScheduleChart(false);
  });
  let zoomInButton = d3.select("#schZoomInButton");
  zoomInButton.on('click', function(d) {
    XAxisIntervalSize /= 2;  // decrease the size when zoom in
    startScheduleChart(false);
  })
  let zoomOutButton = d3.select("#schZoomOutButton");
  zoomOutButton.on('click', function(d) {
    XAxisIntervalSize *= 2;
    startScheduleChart(false);
  })

  // Define 'div' for tooltips
  let div = d3.select("body")
              .append("div")  // declare the tooltip div 
              .attr("class", "popover")  // use popover for persistent for clicking
              .style("opacity", 0);      // set the opacity to nil

  _this.plot.selectAll('.y.axis .tick')
            .style("cursor", function(d) { 
              let nodeObj = _this.getNode(d); 
              if (nodeObj) {
                  if (hasDebug(nodeObj) || nodeObj.hasOwnProperty("short name") || 
                      getMetadataValue(nodeObj, "children") !== undefined ) {
                    return "pointer"; 
                  }
              }
            })
            .on('click', function(d) { createPopOver(d) });

  function createPopOver(d) {
    let nodeObj = _this.getNode(d);

    function toggleChildren(nodeId) {
      // toggle all children of node hidden
      // rerender the whole graph by startScheduleChart
      let nodeIndex = getNodeIndex(nodeId);
      if (nodeIndex < 0) {
        console.warn("Warning! Cannot find node in schedule data:" + nodeId);
        return;
      }
      let node = scheduleData[nodeIndex];
      node.expand =! node.expand;
      let childrenIDList = getMetadataValue(node, "children");
      for(let ni=nodeIndex+1; ni<scheduleData.length; ni++) {
        let curNode = scheduleData[ni];
        if ( childrenIDList.includes(curNode.id) ) {
          curNode.hidden =! curNode.hidden;  // toggle it
          // do this recursively if child is expanded
          let curChildList = getMetadataValue(curNode, "children");
          if (curChildList !== undefined && curNode.expand) {
            childrenIDList = childrenIDList.concat(curChildList);
          }
        }
      }
    }

    if (nodeObj) {
      if (nodeObj.hasOwnProperty("tooltip") && nodeObj["tooltip"]) {
        // remove tooltip capabilities if shown
        div.transition()
          .duration(200)
          .style("opacity", 0);
        nodeObj["tooltip"] = false;
        return;
      }
      // show tooltip
      nodeObj["tooltip"] = true;
      let divHTML = "";   // display the full name
      if (hasDebug(nodeObj)) {
        // add debug location and sync to editor
        let debugObj = getFirstDebug(nodeObj);
        let debugLoc = formatDebugLocation(debugObj);
        divHTML = "<a href=# onClick='syncEditorPaneToLine(" + debugObj.line + 
                  ", \"" + debugObj.filename + "\")'>" + nodeObj.name + " " + debugLoc + "</a><br/>";
      } else {
        divHTML = "<p>" + nodeObj.name +"</p>";
      }
      div.transition()
        .duration(200)
        .style("opacity", 1);

      if (nodeObj.hasOwnProperty("expand")) {
        // add expand and collapse button if it as children
        divHTML += "<button class=\"btn btn-secondary btn-sm\" id=\"schChildren\" data=" + nodeObj.id;
        if (nodeObj.expand) { divHTML += ">collapse</button>"; }
        else { divHTML += ">expand</button>"; }
      }

      div.html(divHTML)
        .style("left", (d3.event.pageX) + "px")
        .style("top", (d3.event.pageY - 28) + "px");  // 28 is from trial and error

      $('#schChildren').on('click', function () {
        toggleChildren( parseInt($(this).attr("data")) );
        startScheduleChart(false);
      });
    }
  }
}

ScheduleViewer.prototype.getNode = function (id) {
  if (id <= 0) return undefined;
  for(let node of this.data) {
    if (node.id === id) { return node; }
  }
  return undefined;
}

function clickBar(d) {
  // Placeholder for showing details persistently
  if (barClickedDown === d) {
      changeDetailsPane(getDetails(d), d.name);
  }
}

function redrawScheduleChart() {
  if (sv_chart === undefined) return;

  // check height to determine if redraw is necessary
  let CID = getViewerConst().gid;
  let reportBodyHeight = $('#' + CID).height();
  let maxNodeCount = getMaxNodeCount(reportBodyHeight);
  let prevNodeCount = sv_chart.data.length;
  if (maxNodeCount === prevNodeCount) {
    setScheduleChartWidth();  // no need to redraw
    return;
  }
  startScheduleChart(false);
}

/**
 * @function setScheduleChartWidth removes the existing and redraw for new width.
 */
function setScheduleChartWidth() {
  let viewerValues = getViewerConst();
  if (viewerValues !== undefined) {
    // currently available
    let CID = getViewerConst().gid;
    $('#' + CID).html("");
    sv_chart.draw();
  }
}

/**
 * @function startScheduleChart removes the existing and redraw with more nodes.
 */
function startScheduleChart(force_top) {
  // Restart the new graph
  let CID = getViewerConst().gid;
  $('#' + CID).html("");

  barClickedDown = ''; // Reset the bar click down, this is to show the details persistently

  let nodes = sliceSchedule(top_node_id, force_top);
  if (nodes.length <= 0) {
    if (scheduleData.length > 0) {
      $('#' + CID).html("Screen size too small to load data.");
      return;
    }
    $('#' + CID).html("No schedule to load.");
    return;
  }

  sv_chart = new ScheduleViewer({ element: document.querySelector('#' + CID),
                                  data: nodes,
                                });
}

function flattenObj(obj, to) {
  var newObj = {};

  Object.keys(obj).forEach((key) => {
    if (key !== "children") {
      newObj[key] = obj[key];
    }
  });
  if (newObj.hasOwnProperty("start") && newObj.hasOwnProperty("end")) {
    newObj.start = parseInt(newObj.start);
    newObj.end = parseInt(newObj.end)
    if (newObj.start < 0 || newObj.end < 0) {
      console.log("Error! Data error: negative start cycle or latency: " + newObj.name);
      return;  // don't add to avoid crashing
    }
    if (newObj.start === newObj.end) {
      // Temporary workaround by setting the latency to 1/2 clock cycle
      newObj.end = newObj.start + 0.5;
    }
  }
  else {
    console.log("Warning! Node has no start and end " + newObj.name);
    return;  // don't add
  }
  newObj['hidden'] = false;  // for collapsing
  newObj['tooltip'] = false; // for tooltip
  newObj['metadata'] = {};
  if (obj.hasOwnProperty("children")) {
    newObj['metadata']['children'] = [];
    obj['children'].forEach(function (childObj) {
      newObj['metadata']['children'].push(childObj['id']);
    });
    newObj['expand'] = true;  // default is expanded
  }
  to.push(newObj);
  if (obj.hasOwnProperty("children")) {
    obj['children'].forEach(function (childObj) {
      flattenObj(childObj, to);
    });
  }
}

function flattenScheduleJSON(scheduleJSON, funcNode) {
  var newJSON = {'nodes':[]};

  scheduleJSON['nodes'].forEach(function (node) {
    flattenObj(node, newJSON['nodes']);
  });
  let funcEnd = 0;
  let bbList = [];
  // find the latency and name.
  newJSON['nodes'].forEach(function(a) {
    if (a.type === "bb") {
      bbList.push(a.id);
      if (a.end > funcEnd) { funcEnd = a.end; }
    } 
  });
  scheduleMaxXValue = funcEnd;
  // insert to the top of the list
  let schFuncNode = { "name": funcNode['name'],
                    "id": funcNode['id'],
                    "start": 0,
                    "end": funcEnd,
                    "metadata": { "children": bbList },
                    "hidden": false,
                    "tooltip": false,
                    "expand": true
                  };
  newJSON['nodes'].unshift(schFuncNode);
  return newJSON;
}

/**
 * @function renderSchedule clears everything that was there and restart the whole graph
 * 
 * @param {Object} scheduleDataJSON is a list of children nodes for a given C++ function level
 * @param {Integer} chartID the ID selected from tree.
 * @param {Object} funcNode is the function node from tree.
 */
function renderSchedule(scheduleDataJSON, chartID, funcNode){
  // error and update check
  if (chartID !== undefined && top_node_id === chartID) return;  // do nothing if user clicks the same ID
  top_node_id = chartID;

  // Clear details pane before rendering
  clearDivContent();

  scheduleDataJSON = flattenScheduleJSON(scheduleDataJSON, funcNode);

  // Format the data for the first time
  scheduleData = scheduleDataJSON["nodes"];

  startScheduleChart(true);
  return;
}

// set the first element to the ID selected and don't show collapsed hierarchy 
// chop the longest node if longer than the last node since we don't support horizontal scroll
function sliceSchedule(selectedNodeID, force_top) {
  let selectedNodeIndex = getNodeIndex(selectedNodeID);
  if (selectedNodeIndex === -1) { 
    console.warn("Warning! Selected ID does not exist."); 
    return []; 
  }
  let filteredNodes = [];
  let CID = getViewerConst().gid;
  let reportBodyHeight = $('#' + CID).height();
  let maxNodeCount = getMaxNodeCount(reportBodyHeight);
  // check if the number of nodes exceeds the height
  // 2 cases: too many nodes --> push up to a certain amount based on size
  //          not enough nodes to utilitize full height
  //          - check to shift nodes preceed this index
  let nodeCount = 0;
  for (let ni=selectedNodeIndex; ni<scheduleData.length && nodeCount<maxNodeCount; ni++) {
    let node = scheduleData[ni];
    if (!node.hidden && !isAFictitiousNode(node)) {
      // reset the node end to previous value
      filteredNodes.push(node);  // copy by reference since we want the original data
      nodeCount++;
    }
  }
  if (!force_top && nodeCount<maxNodeCount && selectedNodeIndex>0) {
    // shift more node preceed to this node if this is not the first node
    for (let ni=selectedNodeIndex-1; ni>=0 && nodeCount<maxNodeCount; ni--) {
      let node = scheduleData[ni];
      if (!node.hidden && !isAFictitiousNode(node)) {
        filteredNodes.unshift(node);
        nodeCount++;
      }
    }
  }
  return filteredNodes;
}


/**
 * @function getLastNonHiddenNodeID returns the ID of the last rendered non fictitious node.
 * Returns 0 and issues a message if nothing is found.
 */
function getLastNonHiddenNodeID() {
  for (let di=scheduleData.length-1; di>=0; di--) {
    let node = scheduleData[di];
    if (!node.hidden && !isAFictitiousNode(node)) {
      return node.id;
    }
  }
  console.log("Warning! All nodes are hidden.");
  return 0;
}

/**
 * @function getNodeIndex find the index of a non fictitious node in scheduleData list.
 * @param {Integer} nodeID id of the node.
 * @returns the index of a real node if exist, -1 otherwise.
 */
function getNodeIndex(nodeID) {
  for (let di=0; di<scheduleData.length; di++) {
    let node = scheduleData[di];
    if (node.id === nodeID && !isAFictitiousNode(node)) {
      return di;
    }
  }
  return -1;
}

// A fictitious node is a speculative or iteration node.
function isAFictitiousNode(node) {
  return (node.type === 'speculation' || node.type === 'iteration')
}

// Get the metadata off the node
function getMetadataValue(node, key) {
  if (node.hasOwnProperty("metadata") && node.metadata.hasOwnProperty(key)) {
    return node.metadata[key]; 
  }
  return undefined;
}

function setMetadataValue(node, key, value) {
  node.metadata[key] = value;
}
