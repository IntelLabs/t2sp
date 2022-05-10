"use strict";

// disable JSHint warning: Use the function form of "use strict".
// This warning is meant to prevent problems when concatenating scripts that
// aren't strict, but we shouldn't have any of those anyway.
/* jshint -W097 */

/// Global variables

// map< file name/path, position of fileinfo in fileJSON array (which is
// the same as the tab index)>
// i.e. map< file name/path, tab index >
var PREFIX_PAD = "___";
var function_prefix = "";
var fileIndexMap = {};
var PRODUCTS = {NONE:  -1,
                ALL:    0,
                OPENCL: 1,
                HLS:    2,
                SYCL:   3
                };

var portable_mode = false;  // False means the html can only be loaded by IDE
var show_hidden = (((typeof enable_dot != "undefined") && enable_dot == 1) || (typeof schedule_infoJSON != "undefined"))  ? true : false;  // TODO: change this with a compiler flag later

// Main menu properties
var MENU = {
    SUMMARY:       { value: 1, name: "Summary", view: "SUMMARY" },
    THROUGHPUT:    { value: 2, name: "Throughput Analysis", submenu: ["OPT", "divider", "FMAX_II", "VERIF"] },
    RESOURCE_UTIL: { value: 3, name: "Area Analysis", submenu: ["AREA_SYS", "AREA_SRC", "INCREMENTAL"] },
    VIEWERS:       { value: 4, name: "System Viewers", submenu: ["GV", "divider", "LMEM", "SCHEDULE"] },
    ALPHA_TOOLS:   { value: 9, name: "Alpha Tools", submenu: ["DOT", "SCH_TABLE"], hidden: true}   // only unhide with compiler flag
};
// Menu views properties
var VIEWS = {
    NONE:        { value: 0, name: "",                         product: PRODUCTS.NONE},
    SUMMARY:     { value: 1, name: "Summary",                  product: PRODUCTS.ALL},

    //THROUGHPUT:     { value: 20, name: "Performance analysis",   product: PRODUCTS.ALL},
    //PROFILER:    { value: 21, name: "Dynamic profiler",       product: PRODUCTS.ALL},  // TODO: replace with Profiler
    VERIF:       { value: 21, name: "Verification Statistics", product: PRODUCTS.HLS},
    OPT:         { value: 22, name: "Loops Analysis",          product: PRODUCTS.ALL},  // TODO: rename this
    FMAX_II:     { value: 23, name: "f<sub>MAX</sub> II Report",          product: PRODUCTS.ALL},
    //QII_STA:     { value: 24, name: "Quartus Critical Paths",       product: PRODUCTS.ALL },
    //II_EST:      { value: 25, name: "Dynamic II/Latency Estimator", product: PRODUCTS.NONE}, // TODO: change to correct product once activated

    AREA_SYS:    { value: 30, name: "Area Analysis of System",  product: PRODUCTS.ALL},
    AREA_SRC:    { value: 31, name: "Area Analysis of Source (deprecated)",  product: PRODUCTS.ALL},
    //QII_RES:     { value: 31, name: "Quartus Resource Utilization", product: PRODUCTS.ALL},
    INCREMENTAL: { value: 32, name: "Incremental Compile",      product: PRODUCTS.OPENCL},
    // PHY_MEM    { value: 33, name: "Physical memory viewer",   product: PRODUCTS.ALL}

    GV:          { value: 41, name: "Graph Viewer (beta)",         product: PRODUCTS.ALL,   id: "pipeline" },
    LMEM:        { value: 42, name: PREFIX_PAD + " Memory Viewer", product: PRODUCTS.ALL,   id: "lmem"  },
    //EMEM:        { value: 43, name: "External memory viewer", product: PRODUCTS.OPENCL},
    SCHEDULE:    { value: 45, name: "Schedule Viewer (alpha)",     product: PRODUCTS.ALL,   id: "schedule" },

    // Alpha or internal tools
    DOT:         { value: 90, name: "Dot Graph Viewer",         product: PRODUCTS.ALL, hash: "#dot"},
    SCH_TABLE:    { value: 92, name: "Schedule Table",          product: PRODUCTS.ALL}
    //OPTIMIZE_ADVISOR: { value: 5, name: "Optimization advisor", view: "" },  // to be enabled by a flag in the future
};
var viewHash = []; // initialized in main::initializeViews()

// Main menu properties
var CTRLS = {
    SOURCE:  { name: "Show/Hide source code", id: "collapse_source" },
    DETAILS: { name: "Show/Hide details",     id: "collapse_details" }
};

// vector< fileInfo objects >
var detailValues = [""];
var detailIndex = 0;
var curFile;
var previousFilehash = "#file0";
var detailOptValues = [];
var browserInfo = {};

var sideCollapsed = false; // true means Editor pane is collapsed
var detailCollapsed = false;
var view = VIEWS.SUMMARY;
var product = PRODUCTS.ALL;
var currentPane = null;

var LOOP_ANALYSIS_NAME = VIEWS.OPT.name + "<span style='float:right'><input id='showFullyUnrolled' type='checkbox' checked='checked' value='Fully unrolled loops'>&nbspShow fully unrolled loops&nbsp</span>";
var REPORT_PANE_HTML = "<div class='classWithPad' id='opt-area-panel'><div class='card' id='report-panel-body'><div class='card-header'>";
var NO_SOURCE = "No Source Line";

var isValidFileList      = false;  // Also purpose to disable Editor pane and rm #collapse_source button when false
                                   // Editor is disabled when 1) invalid File JSON like -g0 or 2) use by IDE
var isValidInfo          = true;
var isValidQuartus       = true;

var isValidSystemViewer  = true;

var isValidDot           = true;
var isValidScheduler     = true;

var incrementalJSON = null;

function buildFPGAReportMainMenu() {
  // validate JSON data
  validateJSONData();
  function validateJSONData() {
    let isValidSummary = true;
    let isValidWarnings = true;

    // check if all information is valid
    isValidSummary       = (typeof summaryJSON != "undefined") && (summaryJSON = tryParseJSON(summaryJSON)) !== null && !$.isEmptyObject(summaryJSON);
    isValidWarnings      = (typeof warningsJSON!= "undefined") && (warningsJSON= tryParseJSON(warningsJSON))!== null && !$.isEmptyObject(warningsJSON);
    isValidInfo          = (typeof infoJSON    != "undefined") && (infoJSON    = tryParseJSON(infoJSON))    !== null && !$.isEmptyObject(infoJSON);
    isValidQuartus       = (typeof quartusJSON != "undefined") && (quartusJSON = tryParseJSON(quartusJSON)) !== null && !$.isEmptyObject(quartusJSON);
    VIEWS.SUMMARY.valid  = (isValidSummary && isValidWarnings && isValidInfo);

    if (portable_mode) {
        isValidFileList  = (typeof fileJSON    != "undefined") && (fileJSON    = tryParseJSON(fileJSON))    !== null && !$.isEmptyObject(fileJSON);
    }

    // Throughput Analysis
    // ThroughputSummary = throughputJSON
    VIEWS.OPT.valid  = (typeof loopsJSON   != "undefined") && (loopsJSON   = tryParseJSON(loopsJSON))   !== null && !$.isEmptyObject(loopsJSON);
    VIEWS.FMAX_II.valid  = (typeof fmax_iiJSON != "undefined") && (fmax_iiJSON = tryParseJSON(fmax_iiJSON)) !== null && !$.isEmptyObject(fmax_iiJSON);
    VIEWS.VERIF.valid = (typeof verifJSON   != "undefined") && (verifJSON   = tryParseJSON(verifJSON))   !== null && !$.isEmptyObject(verifJSON);
    // QuartusCriticalPath = QuartusSTAJSON
    // DynamicIILatencyEst = DynIIJSON

    // Area Analysis
    VIEWS.AREA_SYS.valid    = (typeof areaJSON    != "undefined") && (areaJSON    = tryParseJSON(areaJSON))    !== null && !$.isEmptyObject(areaJSON);

    // Deal with JSON files for incremental compilation flow.  There may be two different
    // incremental JSON files, depending on whether this is an incremental setup
    // compile (incremental_initialJSON), or an incremental change
    // (incremental_changeJSON).  We deal with both of them the same throughout
    // the rest of the reports, so just merge them into 'incrementalJSON'.
    var isValidIncrementalInitial = (typeof incremental_initialJSON != "undefined") && (incremental_initialJSON=tryParseJSON(incremental_initialJSON)) !== null && !$.isEmptyObject(incremental_initialJSON);
    var isValidIncrementalChange  = (typeof incremental_changeJSON  != "undefined") && (incremental_changeJSON =tryParseJSON(incremental_changeJSON))  !== null && !$.isEmptyObject(incremental_changeJSON);
    VIEWS.INCREMENTAL.valid = isValidIncrementalInitial || isValidIncrementalChange;
    incrementalJSON = isValidIncrementalInitial ? incremental_initialJSON : isValidIncrementalChange ? incremental_changeJSON : null;

    // Viewers
    isValidHierarchyTree = isValidHierarchyTreeData();
    isValidSystemViewer  = (typeof mavJSON     != "undefined") && (mavJSON     = tryParseJSON(mavJSON))     !== null && !$.isEmptyObject(mavJSON);
    let isValidSoTViewer     = (typeof systemJSON  != "undefined") && (systemJSON  = tryParseJSON(systemJSON))  !== null && !$.isEmptyObject(systemJSON);
    // FunctionViewer    = FuncJSON
    VIEWS.LMEM.valid  = (typeof new_lmvJSON     != "undefined") && (new_lmvJSON     = tryParseJSON(new_lmvJSON))     !== null && !$.isEmptyObject(new_lmvJSON);
    let isValidBlockViewer    = (typeof blockJSON    != "undefined") && (blockJSON    = tryParseJSON(blockJSON))    !== null && !$.isEmptyObject(blockJSON);
    let isValidClusterViewer  = (typeof pipelineJSON != "undefined") && (pipelineJSON = tryParseJSON(pipelineJSON)) !== null && !$.isEmptyObject(pipelineJSON);
    VIEWS.GV.valid = (isValidHierarchyTree || isValidSystemViewer || isValidSoTViewer || isValidBlockViewer || isValidClusterViewer);

    VIEWS.SCHEDULE.valid = (typeof scheduleJSON != "undefined") && (scheduleJSON = tryParseJSON(scheduleJSON)) !== null && !$.isEmptyObject(scheduleJSON);

    // Alpha level tools
    VIEWS.AREA_SRC.valid = (typeof area_srcJSON!= "undefined") && (area_srcJSON= tryParseJSON(area_srcJSON))!== null && !$.isEmptyObject(area_srcJSON);
    isValidDot           = (typeof enable_dot  != "undefined") && enable_dot == 1;
    isValidScheduler  =    (typeof schedule_infoJSON != "undefined") && (schedule_infoJSON = tryParseJSON(schedule_infoJSON)) !== null && !$.isEmptyObject(schedule_infoJSON);
  }
    if (!isValidDot) VIEWS.DOT.product = PRODUCTS.NONE;
    if (!isValidScheduler) VIEWS.SCH_TABLE.product = PRODUCTS.NONE;

    // Add the view number
    Object.keys(VIEWS).forEach(function(v) {
        var index = VIEWS[v];
        if (index.hash === undefined) index.hash = "#view" + index.value;
    });

  // Set page title and determine the product
  product = setTitle();
  // Sets the title of the html and returns the product value
  function setTitle() {
    let product = undefined;
    // TODO: need to update when info.json gets new schema
    var pageTitle = "Report";
    if (isValidInfo && infoJSON.hasOwnProperty('rows')) {
      for (var r = 0; r < infoJSON.rows.length; ++r) {
        if (infoJSON.rows[r].hasOwnProperty('name')) {
          if (infoJSON.rows[r].name === "Project Name") {
            pageTitle += ": " + infoJSON.rows[r].data;
          } else if (infoJSON.rows[r].name === "Command") {
            if (infoJSON.rows[r].data[0].includes("i++")) {
              product = PRODUCTS.HLS;
            } else if (infoJSON.rows[r].data[0].includes(" -sycl ") || infoJSON.rows[r].data[0].includes(".spv ")) {
              product = PRODUCTS.SYCL;  // include .spv in the search for backward compatible
            } else if (infoJSON.rows[r].data[0].includes("aoc") >= 0) {  // check AOC last since SYCL also have AOC
              product = PRODUCTS.OPENCL;
            } else {
              console.log("Warning! Unknown product.");
            }
          }
        }
      }
      // Temporary renamed to SYCL in front-end
      if (product !== undefined && product === PRODUCTS.SYCL) {
        for (var r = 0; r < infoJSON.rows.length; ++r) {
          if (infoJSON.rows[r].hasOwnProperty('name')) {
            if (infoJSON.rows[r].name.includes("AOC")) {
              infoJSON.rows[r].name = infoJSON.rows[r].name.replace("AOC", "SYCL");
            }
          }
        }
      }
    }
    $('#titleText').html(pageTitle);
    return product;
  }

  // initialize the prefix used for submenu
  function_prefix =  (product === undefined)   ? "" :  // Quartus or profiler standalone or infoJSON is broken for some unknown reason
                   ( (product == PRODUCTS.HLS) ? "Function" :  // HLS
                                                 "Kernel" );   // OpenCL or SYCL
  // remove unused/invalid views and disable views that doesn't have data yet
  disableInvalidViews();

  generateTablesHTML();
  // This is to be obseleted as we can render the table on demand
  // parse the valid JSONs that's rendering into HTML tables. 
  function generateTablesHTML() {
    // Populate all the table data: area, summary, loops, fmax/II, verification
    // Sets a valid flag in the VIEW
    if (VIEWS.hasOwnProperty("AREA_SYS") && VIEWS.AREA_SYS.valid) {
      VIEWS.AREA_SYS.source =  parseAreaData(areaJSON);
    }
    if (VIEWS.hasOwnProperty("AREA_SRC") && VIEWS.AREA_SRC.valid) {
      VIEWS.AREA_SRC.source = parseAreaData(area_srcJSON);
    }

    // Three cases: full compile, -rtl, and Quartus only
    if (VIEWS.hasOwnProperty("SUMMARY") && VIEWS.SUMMARY.valid && isValidQuartus) {
      VIEWS.SUMMARY.source = parseSummaryData(infoJSON, warningsJSON, summaryJSON, quartusJSON);
    }
    else if (VIEWS.hasOwnProperty("SUMMARY") && VIEWS.SUMMARY.valid) {
      VIEWS.SUMMARY.source = parseSummaryData(infoJSON, warningsJSON, summaryJSON, undefined);
    }
    else if (isValidQuartus) {  // Allow viewing of Quartus summary by itself with no original compile data
      VIEWS.SUMMARY.source = parseQuartusData(quartusJSON);
    }
    else {  // Display something here since this is the front page
      VIEWS.SUMMARY.source = "&nbspSummary data is invalid!";
    }

    if (VIEWS.hasOwnProperty("OPT") && VIEWS.OPT.valid) {
      VIEWS.OPT.source = parseLoopData(loopsJSON);
    }

    if(VIEWS.hasOwnProperty("FMAX_II") && VIEWS.FMAX_II.valid) {
      var data = fmax_iiJSON;
      var table = renderFmaxIITable(data);
      VIEWS.FMAX_II.source = table;
    }

    if (VIEWS.hasOwnProperty("VERIF") && VIEWS.VERIF.valid) {
      VIEWS.VERIF.source = parseVerifData(verifJSON);
    }

    if (isValidDot) {
      VIEWS.DOT.source = "<div id='svg_container' style='width:100%;height:100%;margin:0 auto;overflow:hidden;'></div>";
    }

    if ((VIEWS.hasOwnProperty("INCREMENTAL") && VIEWS.INCREMENTAL.valid)) {
      VIEWS.INCREMENTAL.source = parseIncrementalData(incrementalJSON);
    }
  }

  // control menu for collipasing source and details, navigation bar, and views
  initializeControlMenu();
  initializeNavBar();

    if (isValidFileList) {
      // Add file tabs to the Editor pane dropdown
      addFileTabs( fileJSON );
    } else {
        if (!sideCollapsed) collapseAceEditor();
    }
    adjustToWindowEvent();

    //----------- VIEWS FUNCTION ------------//

    // create the collapsible hamburger menu
    // controls are specified in CTRL dictionary
    function initializeControlMenu() {
      Object.keys(CTRLS).forEach(function(c) {
        // don't create controls for source in menu if no file list
        if (c == "SOURCE" && !isValidFileList) return;

        var collpase_link = document.createElement('a');
        collpase_link.className = "dropdown-item";
        collpase_link.id = CTRLS[c].id;
        collpase_link.innerHTML = CTRLS[c].name;
        collpase_link.href = "#";
        $("#collapse-ctrl").append(collpase_link);
      });
    }

  // Populate main-menu html element with MENU and VIEWS data structure
  // The MENU name becomes navigation bar. The view name becomes drop down item when exist
  function initializeNavBar() {
    // create a div for each view, and set it to "hidden"; also create
    // a menu entry for each view
    Object.keys(MENU).forEach(function(m) {
      var views = MENU[m];
      // hide hidden menu - TODO: consolide isValidScheduler the flag to show_hidden flag
      if (views.hidden !== undefined && !show_hidden && !isValidScheduler) return;
      if (views.view !== undefined) {
        // no submenu, the menu is the view
        let v = MENU[m].view;
        // TODO: Factor these out to initializeViews to allow unittest initializeNavBar
        let index = VIEWS[v];
        viewHash[index.hash] = v;
        addReportColumn(index);

        let navButton = document.createElement("a");
        navButton.setAttribute("role", "button");
        navButton.innerHTML = index.name;
        navButton.setAttribute("data-toggle", "tooltip");
        navButton.setAttribute("data-html", "true");
        navButton.setAttribute("data-placement", "right");
        if (index.valid) {
          navButton.className = "btn btn-outline-primary";
          navButton.href = index.hash;
          // if (index.hasOwnProperty("title")) { navButton.title = index.title; }
        } else { 
          navButton.className = "btn btn-outline-primary disable";
          // if (index.hasOwnProperty("secondTitle")) { navButton.title = index.secondTitle; }
        }
        // Add tooltip if exist
        let toolTipHtml = lookUpTerminology(v, !index.valid);
        if (toolTipHtml !== "") { navButton.title = toolTipHtml; }
        $("#main-menu").append(navButton);
      } else {
        // has submenu, add the menu as dropdown
        let navDropDownGroup = document.createElement("div");
        navDropDownGroup.className = "dropdown";
        let navButton = document.createElement("button");
        navButton.className = "btn btn-outline-primary dropdown-toggle";
        navButton.type = "button";
        navButton.setAttribute('data-toggle', "dropdown");
        navButton.innerHTML = views.name;
        navDropDownGroup.appendChild(navButton);
        let navDropDown = document.createElement('div');
        navDropDown.className = "dropdown-menu";
        Object.keys(views.submenu).forEach(function(vkey) {
          let v = views.submenu[vkey];
          if (v === "divider") {
            let separatorElement = document.createElement('div');
            separatorElement.className = "dropdown-divider";
            navDropDown.appendChild(separatorElement);
          }
          else if (VIEWS[v] !== undefined) {
            // Don't added removed views
            let index = VIEWS[v];
            if (index.name === undefined || index.name === "") {
              console.warn("Error! VIEW " + v + " has no name.");
              return;
            }
            viewHash[index.hash] = v;
            if (index.valid) { addReportColumn(index); }  // Temporary workaround for incremental since it parses JSON in addReportColumn

            let dropDownItem = document.createElement('a');
            dropDownItem.id = v;
            dropDownItem.innerHTML = index.name;
            dropDownItem.setAttribute("data-toggle", "tooltip");
            dropDownItem.setAttribute("data-html", "true");
            dropDownItem.setAttribute("data-placement", "right");
            if (index.valid) {
              dropDownItem.className = 'dropdown-item';
              // TODO: VIEWS.DOT hash
              dropDownItem.href = index.hash; 
              if (index.hasOwnProperty("title")) {
                // dropDownItem.title = index.title;
              }
            } else { 
              dropDownItem.className = 'dropdown-item disabled';
              dropDownItem.href = "#";
              // if (index.hasOwnProperty("secondTitle")) { dropDownItem.title = index.secondTitle; }
            }
            let toolTipHtml = lookUpTerminology(v, !index.valid);
            if (toolTipHtml !== "") { dropDownItem.title = toolTipHtml; }
            navDropDown.appendChild(dropDownItem);
          }
        });
        navDropDownGroup.appendChild(navDropDown);
        $("#main-menu").append(navDropDownGroup);
      }
    });

    // display the current view
    currentPane = "#report-pane-view" + view.value;
    $(currentPane).toggle();
  }

  // Remove invalid views from the VIEWS object for the given product
  // Disable views are invalid if the JSON is missing or invalid, and add prefix to view name
  // In the event that we cannot determine the product, then we only show the minimum set of data defined by "PRODUCTS.ALL"
  function disableInvalidViews() {
    Object.keys(VIEWS).forEach(function(v) {
      var sel_view = VIEWS[v];
      if (sel_view.product != PRODUCTS.ALL && (product === undefined || sel_view.product != product)) {
        // view doesn't belong to this product
        delete VIEWS[v];
        return;
      }
      if (!sel_view.valid) {
        // view is disabled because JSON is invalid
        return;
      }
      // add clickDown and source fields for parsing the JSON data later
      sel_view.clickDown = null;
      sel_view.source = null;
      // Prepend 'Kernel' or 'Component' for different views depending on product
      sel_view.name = sel_view.name.replace(PREFIX_PAD, function_prefix);
    });
  }

    // add data to the report pane
    function addReportColumn(reportEnum) {
        var report = REPORT_PANE_HTML;
        if (reportEnum != VIEWS.GV && reportEnum != VIEWS.LMEM && reportEnum != VIEWS.SCHEDULE) {
          // Temporary workaround until tables are refactored
          $("#report-pane")[0].insertAdjacentHTML("beforeend", "<div id=\"report-pane-view" + reportEnum.value + "\" class=\"report-pane-view-style\"></div>");
          $("#report-pane-view" + reportEnum.value).toggle();  
        }
        let reportPane, reportBody, reportView;
        switch (reportEnum) {
          case VIEWS.SUMMARY:
            report += reportEnum.name + "</div><div class='card-body' id='report-body' onscroll='adjustToWindowEvent()'>";
            report += "<div id='area-table-content'></div>";
            break;
          case VIEWS.OPT:
            report += LOOP_ANALYSIS_NAME;
            report += "</div><div class='card-body' id='report-body' onscroll='adjustToWindowEvent()'>";
            report += "<table class='table table-hover' id='area-table-content'></table>";
            break;
          case VIEWS.FMAX_II:
            report += "f<sub>MAX</sub> II Report";
            report += "</div><div class='card-body' id='report-body' onscroll='adjustToWindowEvent()'>";
            report += "<table class='table table-hover' id='area-table-content'></table>";
            break;
          case VIEWS.VERIF:
            report += reportEnum.name + "</div><div class='card-body' id='report-body' onscroll='adjustToWindowEvent()'>";
            report += "<table class='table table-hover' id='area-table-content'></table>";
            break;
          case VIEWS.AREA_SRC:
          case VIEWS.AREA_SYS:
            report += "<table style='width:100%'><tr><td class='panel-heading-text'>";
            report += "<span style='font-size:1rem;'>" + reportEnum.name + "</span>";
            report += "<br>(area utilization values are estimated)<br>Notation <i>file:X</i> > <i>file:Y</i> indicates a function call on line X was inlined using code on line Y.";
            if (VIEWS.AREA_SYS.valid && VIEWS.hasOwnProperty("INCREMENTAL") && VIEWS.INCREMENTAL.valid) {
              report += "<br><strong>Note: Area report accuracy may be reduced when performing an incremental compile.</strong>";
            }
            if (VIEWS.AREA_SYS.valid && !areaJSON.debug_enabled) {
                report += "<br><strong>Recompile without <tt>-g0</tt> for detailed area breakdown by source line.</strong>";
            }
            report += "</td><td>";
            report += "<span style='font-size:1rem;float:right'>";
            report += "<button id='collapseAll' type='button' class='text-left exp-col-btn'><span class='body glyphicon'>&#xe160;</span>&nbsp;Collapse All&nbsp;</button>";
            report += "<button id='expandAll' type='button' class='text-left exp-col-btn'><span class='body glyphicon'>&#xe158;</span>&nbsp;Expand All&nbsp;</button>";
            report += "</span>";
            report += "</td></tr></table>";
    
            report += "</div><div class='card-body' id='report-body' onscroll='adjustToWindowEvent()'>";
            report += "<table class='table table-hover' id='area-table-content'></table>";
            break;
          case VIEWS.INCREMENTAL:
            if (incrementalJSON.hasOwnProperty('name')) {  // This needs to be refactor. Should not be parsing JSON here
                report += incrementalJSON.name;
            } else {
                report += reportEnum.name;
            }
            if (incrementalJSON.hasOwnProperty('details')) {
                report += "<hr/>";
                incrementalJSON.details.forEach(function(d) {
                report += "<li>" + d.text + "</li>";
                });
            }
            report +="</div>";
            report += "<div class='card-body' id='report-body' onscroll='adjustToWindowEvent()'>";
            report += "<table class='table table-hover' id='area-table-content'></table>";
            break;
          case VIEWS.LMEM:
            reportView = document.createElement('div');
            reportView.id = "report-pane-view" + reportEnum.value;
            reportView.className = "row report-pane-view-style";
            addTreePanel(reportEnum.name.replace("Viewer", "List"), reportEnum.id, true, reportView);
            reportBody = addViewerPanel(reportEnum, "layers-lmem", "LMEMG");
            reportView.appendChild(reportBody);
            reportPane = document.getElementById('report-pane');
            reportPane.appendChild(reportView);
            $("#report-pane-view" + reportEnum.value).toggle();
            return;
          case VIEWS.GV:
            reportView = document.createElement('div');
            reportView.id = "report-pane-view" + reportEnum.value;
            reportView.className = "row report-pane-view-style";
            addTreePanel(reportEnum.name.replace("Viewer", "List"), reportEnum.id, false, reportView);
            reportBody = addViewerPanel(reportEnum, "layers", "GVG");
            reportView.appendChild(reportBody);
            reportPane = document.getElementById('report-pane');
            reportPane.appendChild(reportView);
            $("#report-pane-view" + reportEnum.value).toggle();
            return;
          case VIEWS.SCHEDULE:
            reportView = document.createElement('div');
            reportView.id = "report-pane-view" + reportEnum.value;
            reportView.className = "row report-pane-view-style";
            addTreePanel(reportEnum.name.replace("Viewer", "List"), reportEnum.id, false, reportView);
            reportBody = addViewerPanel(reportEnum, "layers", "SVC");
            reportView.appendChild(reportBody);
            reportPane = document.getElementById('report-pane');
            reportPane.appendChild(reportView);
            $("#report-pane-view" + reportEnum.value).toggle();
            return;
          case VIEWS.DOT:
            report += "<table style='width:100%'><tr><td class='panel-heading-text'>";
            report += reportEnum.name;
            report += " &mdash; Use Ctrl + Left Click to open VHDL file for model nodes.";
            report += "</td><td>";
    
            // Controls
            report += "<span style='float:right'>";
            report += "<input  id='dot_edges_toggle'  type='checkbox' checked='checked'>Edges</input>";
            report += "<button id='dot_up_hierarchy'  type='button' >&nbsp;UP&nbsp;</button>";
            report += "<button id='dot_top_hierarchy' type='button' >&nbsp;TOP&nbsp;</button>";
            report += "</span>";
            report += "</td></tr></table>";
    
            report += "<span id='dot_hierarchy_text'>|module</span>";
    
            report += "</div><div class='card-body' id='report-body' onscroll='adjustToWindowEvent()'>";
            report += "<div id='area-table-content' style='height:100%;'></div>";
            break;
          case VIEWS.SCH_TABLE:
            report += "</div><div class='card-body' id='report-body'>";
            report += "<div id='SCHEDULE' class='card-body fade in active' ></div></div>";
            break;
        }
    
        report += "</div></div></div>";
    
        $("#report-pane-view" + reportEnum.value).html(report);
        $("#report-pane-view" + reportEnum.value + " #area-table-content").html(reportEnum.source);
    }
}

///// Global functions

// Add the passed-in HDL filename to the source code pane, and display it,
// creating a new ACE editor object if necessary.
function addHdlFileToEditor(file) {
  var rawFile = new XMLHttpRequest();
  rawFile.open("GET", file, false);
  rawFile.onreadystatechange = function () {
    if(rawFile.readyState === 4) {
      if(rawFile.status === 200 || rawFile.status === 0) {
        var allText = rawFile.responseText;

        // Create the editor if it doesn't exist
        if (dot_editor === null) {
          var count = $("#editor-pane-nav li").length;
          $("#editor-pane-nav").append("<li><a data-target='#file" + count + "'>HDL</a><p></p></li>");
          fileIndexMap.HDL = count;

          var editor_div = "<div class='tab-pane' id='file" + count + "' style='height:500px;'>";
          editor_div += "<div class='well' id='dot_editor_well'></div></div>";
          $("#editor-pane .tab-content").append(editor_div);

          dot_editor = ace.edit($("#dot_editor_well")[0]);
          dot_editor.setTheme( "../ace/theme/xcode" );
          dot_editor.setFontSize( 12 );
          dot_editor.getSession().setUseWrapMode( true );
          dot_editor.getSession().setNewLineMode( "unix" );

          fileJSON.push({"editor":dot_editor, "absName":"HDL", "name":"HDL", "path":"HDL", "content":allText});
        }

        // Set the file content
        if (file.endsWith(".vhd")) {
          dot_editor.getSession().setMode( "../ace/mode/vhdl" );
        } else {
          dot_editor.getSession().setMode( "../ace/mode/verilog" );
        }
        dot_editor.setValue( allText.replace( /(\r\n)/gm, "\n" ) );
        dot_editor.setReadOnly( true );

        syncEditorPaneToLine(1, "HDL");
      } else {
        alert("Something went wrong trying to get HDL file", file);
      }
    }
  };
  rawFile.send(null);
}

function zoomed() {
    var graph = d3.select("#svg_container svg g");
    graph.attr("transform", "translate(" + d3.event.translate + ")scale(" + d3.event.scale + ")");
}

function
adjustToWindowEvent()
{
    setReportPaneHeight();
    stickTableHeader();
    if (!sideCollapsed) adjustEditorButtons();
    var svg = $("#svg_container svg");
    if(svg.length > 0) {
      svg[0].style.width = $("#svg_container").width();
      svg[0].style.height = $("#svg_container").height();
    }
    if (view === VIEWS.SCHEDULE) redrawScheduleChart();  // Only redraw schedule during resizing schedule viewer
}

function resizeEditor()
{
    if (sideCollapsed) return;

    var editor;
    for (var i = 0; i < fileJSON.length; i++) {
        if (fileJSON[i].name == curFile || fileJSON[i].path == curFile) {
            editor = fileJSON[i].editor;
            break;
        }
    }
    if (editor) editor.resize();
}

function refreshAreaVisibility() {
    $(currentPane + " #area-table-content tr").each(function() {
        if ($(this).attr('data-level') == "0" && $(this).is(":hidden")) {
            $(this).toggle();
        }
    });
}

function updateURLHash() {
    if (history.pushState) {
        history.pushState(null, null, view.hash);
    } else {
        location.hash(view.hash);
    }
}

function goToView(viewId, update) {
    var newView = VIEWS[viewId];
    if (!newView) { updateURLHash(); return; }
    if (view != newView) {
        $(currentPane).toggle();
        view = newView;
        currentPane = "#report-pane-view" + view.value;
        $(currentPane).toggle();
        if (view.clickDown !== null && view.clickDown.getAttribute('index')) {
            changeDivContent(view.clickDown.getAttribute('index'));
        } else {
            changeDivContent(0);
        }
        if (view == VIEWS.DOT && isValidDot) {
            // Collapse editor and details to maximize screen space for the graph
            if (!sideCollapsed) collapseAceEditor();
            if (!detailCollapsed) collapseDetails();
        }
    }
    if (update) {
        updateURLHash();
    }
    // clear details pane before switching between views
    clearDivContent();
    if (sv_chart !== undefined) { sv_chart.clearPopover(); }
    top_node_id = -1;
    switch (view) {
      case VIEWS.LMEM:
        if (!lmem_graph && VIEWS.LMEM.valid) {
            var hasLMem = addHierarchyTree(VIEWS.LMEM);
            if (hasLMem) $('#LMEMG').html("<br>&nbspClick on a memory variable to render it!");
            else $('#LMEMG').html("&nbspThere is no " + ((product == PRODUCTS.OPENCL) ? "kernel" : "component") + " memory variable in the design file!");
        }
        else if (VIEWS.LMEM.valid) lmem_graph.refreshGraph();
        else $('#LMEMG').html("&nbsp " + view.name + " data is invalid!");
        break;
      case VIEWS.GV:
      case VIEWS.SCHEDULE:
        // Menu would have been disabled if data is not valid, so no tree would mean empty data
        let hasTree = addHierarchyTree(view);
        if (hasTree) $('#' + getViewerConst().gid).html("<br>&nbspClick on a node to see the data path graph.");
        else $('#' + getViewerConst().gid).html("&nbspSystem is empty.");
        break;
      case VIEWS.DOT:
        var hash_split = window.location.hash.split("/");
        putSvg(hash_split[hash_split.length - 1]);
        break;
      case VIEWS.SCH_TABLE:
        if(isValidScheduler){
            var data = schedule_infoJSON; 
            renderScheduleTable("#SCHEDULE", data);
        }
        break;
    }
    refreshAreaVisibility();
    adjustToWindowEvent();
}

function unsetClick() {
  if(view.clickDown !== null) {
    view.clickDown.classList.remove("nohover");
    view.clickDown.classList.remove("selected-item-highlight");
    view.clickDown = null;
    changeDivContent(0);
  }
}

// Go to the requested view when the URL hash is changed
$(window).on('hashchange', function() {
      var hash_view = window.location.hash.split("/")[0];
      goToView(viewHash[hash_view]);
    });

/*****************************************/
/* Main Template initialization */
/*****************************************/
$(document).ready(function () {
    identifyBrowser();

    var isBrowser = browserInfo.isOpera || browserInfo.isSafari || browserInfo.isFirefox || browserInfo.isChrome || browserInfo.isIE;
    if (!portable_mode && isBrowser) {  // and using a browser
        if (!sideCollapsed) collapseAceEditor();
        if (!detailCollapsed) collapseDetails();
        displayBrowserNotSupportedWarning();
        return;
    }

    if (browserInfo.isOpera || browserInfo.isSafari) {
        // TODO: annotate this message to the top of the report for final beta release
        alert("Browser not supported");
    }
    buildFPGAReportMainMenu();

    $('label.tree-toggle').click(function () {
        $(this).parent().children('ul.tree').toggle(200);
    });

    if (window.location.hash === "") {
        updateURLHash();
    } else {
      var hash_view = window.location.hash.split("/")[0];
      goToView(viewHash[hash_view]);
    }

    $(window).resize(function () {
        adjustToWindowEvent();
        resizeEditor();
    });

    function getChildren($row) {
        var children = [], level = $row.attr('data-level');
        var isExpanding;
        var maxExpandedLevel = Number(level) + 1;

        // Check if expanding or collapsing
        if ($row.next().is(":hidden")) {
            isExpanding = true;
        } else {
            isExpanding = false;
        }

        while($row.next().attr('data-level') > level) {
            // Always expand or collapse immediate child
            if($row.next().attr('data-level')-1 == level) {
                children.push($row.next());
                $row.next().attr('data-ar-vis',$row.next().attr('data-ar-vis')==1?0:1);
            } else {
                // expand if previously was expanded and parent has been expanded - maxExpandedLevel is used to tell if a child's immediate parent has been expanded
                if ($row.next().attr('data-ar-vis')==1 && isExpanding && $row.next().attr('data-level')<=(maxExpandedLevel+1)) {
                    children.push($row.next());
                    maxExpandedLevel = Math.max(maxExpandedLevel, $row.next().attr('data-level'));
                    // collapse if visible and element is some descendant of row which has been clicked
                } else if (!isExpanding && $row.next().is(":visible")) {
                    children.push($row.next());
                }
            }
            $row = $row.next();
        }
        return children;
    }


    // Expand or collapse when parent table row clicked
    $('#report-pane').on('click', '.parent', function() {
        var children = getChildren($(this));
        $.each(children, function () {
            $(this).toggle();
        });
        toggleChevon( $(this).find('.ar-toggle')[0], 'none' );
        stickTableHeader();
    });

    $('#report-pane').on('click', 'tr', function(d) {
        // do not change clicked state if we click an anchor (ie expand/collapse chevron)
        if (d.target.tagName.toLowerCase() === "a") return;
        // traverse up the DOMtree until we get to the table row
        for (d = d.target; d && d !== document; d = d.parentNode) {
            if (d.tagName.toLowerCase() === "tr") break;
        }
        // check to see if row is 'clickable'
        if (!$(this).attr('clickable')) return;
        if (view.clickDown == d) {
            // deselect row
            unsetClick();
        } else {
            // else select new row
            if (view.clickDown) {
                // deselect previous row
                unsetClick();
            }
            if ($(this).attr('index')) {
                // update "details" pane
                changeDivContent($(this).attr('index'));
            }
            view.clickDown = d;
            d.classList.add("nohover");
            d.classList.add("selected-item-highlight");
        }
    });

    // Display details on mouseover
    $('#report-pane').on('mouseover', 'tr', function() {
        if(view.clickDown === null && $(this).attr('index') && detailCollapsed === false) {
            changeDivContent($(this).attr('index'));
        }
    });

    $('.dropdown_nav').on('click', function () {
        // Clicking a .dropdown_nav item changes the page hash.
        // If the onHashChange event is supported in the browser, we will change views
        // using the corresponding event handler. Otherwise, do it explicitly here.
        if (!("onhashchange" in window)) {
            var viewId = $(this).attr("viewId");
            goToView(viewId);
        }
    });

    $('#collapse_source').on('click', collapseAceEditor);
    $('body').on('click', '#close-source', function () {
        collapseAceEditor();
        flashMenu();
    });

    $('#collapse_details').on('click', collapseDetails);
    $('body').on('click', '#close-details', function () {
        collapseDetails();
        flashMenu();
    });

    // Editor navigation dropdown event
    $('#editor-nav-button').change( function() {
      let newFileIndex = $(this).val();
      $(previousFilehash).removeClass("active");
      $(newFileIndex).addClass("active");
      previousFilehash = newFileIndex;
    });

    // Tool tip for menu
    $(function () {
      $('[data-toggle="tooltip"]').tooltip();
    })

    $('#report-pane').on('click', '#showFullyUnrolled', function() {
        $('.ful').each(function () {
            $(this).toggle();
        });
        stickTableHeader();
    });

    // Expand all the rows in area table
    $('#report-pane').on('click', '#expandAll', function () {
        // Get all the rows in the table which can expand/collapse
        var parents = $(currentPane + ' .parent');

        $.each(parents, function () {
            // Toggle all the children of that parent row
            var children = getChildren($(this));
            $.each(children, function () {
                // Set the data-ar-vis to be one so that it will expand afterwards
                $(this).attr('data-ar-vis', 1);
                // Only toggle if row is hidden and need to expand, or visible and need to collapse
                if ($(this).is(":hidden"))
                    $(this).toggle();
            });

            // Make all the arrow icons pointing down
            var iconsToToggle = $(this).find('.ar-toggle');
            $.each(iconsToToggle, function () { toggleChevon($(this)[0], 'down'); });
        });

        stickTableHeader();
    });

    // Collapse all the rows in area table
    $('#report-pane').on('click', '#collapseAll', function () {
        // Get all the rows in the table which can expand/collapse
        var parents = $(currentPane + ' .parent').toArray().reverse();

        $.each(parents, function () {
            // Toggle all the children of that parent row
            var children = getChildren($(this));
            $.each(children, function () {
                // Set the data-ar-vis to be zero so that the row states resets
                $(this).attr('data-ar-vis', 0);
                // Only toggle if row is hidden and need to expand, or visible and need to collapse
                if (!$(this).is(":hidden"))
                    $(this).toggle();
            });

            // Make all the arrow icons pointing down
            var iconsToToggle = $(this).find('.ar-toggle');
            $.each(iconsToToggle, function () {  toggleChevon($(this)[0], 'right'); });
        });

        stickTableHeader();
    });

    // DOT viewer controls
    $('#report-pane').on('click', '#dot_up_hierarchy', function () {
        var hash = window.location.hash.split('/');
        hash.pop();
        if (hash.length > 1) goToDot(hash.join('/'));
    });

    $('#report-pane').on('click', '#dot_top_hierarchy', function () {
        goToDot("#dot/" + dot_top);
    });

    $('#report-pane').on('click', '#dot_edges_toggle', function (d) {
        if ($(this).is(':checked')) {
          $("#svg_container").find("g.edge").toggle();
        } else {
          $("#svg_container").find("g.edge").toggle();
        }
    });
});

// Function given by IDE
// populates browserInfo and determine which browser is being used to open report.html
function identifyBrowser(){
    //find browser version:
    browserInfo.isOpera = !!window.opera || navigator.userAgent.indexOf(' OPR/') >= 0; // Opera 8.0+ (UA detection to detect Blink/v8-powered Opera)
    browserInfo.isFirefox = typeof InstallTrigger !== 'undefined';   // Firefox 1.0+
    browserInfo.isSafari = Object.prototype.toString.call(window.HTMLElement).indexOf('Constructor') > 0; // At least Safari 3+: "[object HTMLElementConstructor]"
    browserInfo.isChrome = !!window.chrome && !browserInfo.isOpera; // Chrome 1+
    browserInfo.isIE = /*@cc_on!@*/false || !!document.documentMode; // At least IE6
}

// Function given by IDE
// Issue a warning in the report div saying the report can only be open by IDE
function displayBrowserNotSupportedWarning(){
    // create a warning box:
    var warningWrapper = $("#report-pane");
    // warningWrapper.className = 'warningWrapper';

    var warningContainer = document.createElement('div');
    warningContainer.className = 'warningContainer';
    $(warningWrapper).append(warningContainer);

    var title = document.createElement('h2');
    title.innerHTML = 'WARNING!';
    $(warningContainer).append(title);

    var message = document.createElement('div');
    message.style.textAlign = 'left';
    message.style.marginLeft = '20px';
    message.style.marginRight = '20px';

    var span1 = document.createElement('span');
    span1.innerHTML = "Your browser is not supported to view the report<br/>" +
                       "Please use Intel oneAPI IDE.<br/><br/>";

    message.appendChild(span1);
    $(warningContainer).append(message);
    $(warningWrapper).hide().fadeIn(1000);
    
}

function
flashMenu()
{
    var $menuElement = $('#collapse_sidebar');
    var interval = 500;
    $menuElement.fadeIn(interval, function () {
        $menuElement.css("color", "#80bfff");
        $menuElement.css("border", "1px solid #80bfff");
        $menuElement.fadeOut(interval, function () {
            $menuElement.fadeIn(interval, function () {
                $menuElement.fadeOut(interval, function () {
                    $menuElement.fadeIn(interval, function () {
                        $menuElement.css("color", "black");
                        $menuElement.css("border", "1px solid transparent");
                    });
                });
            });
        });
    });
}

function
collapseDetails()
{
    $('#detail-pane').toggle();
    detailCollapsed = !detailCollapsed;
    if (detailCollapsed) {
        // when details is collapsed, clear it
        changeDivContent(0);
    } else if (view.clickDown) {
        // when details is un-collapsed, update contents, if valid
        changeDivContent(view.clickDown.getAttribute('index'));
    }
    adjustToWindowEvent();
    resizeEditor();
}

// Use to toggle Editor - use by collaspe source button
function collapseAceEditor() {
    $('#editor-pane').toggle();
    let reportPane = document.getElementById('report-pane');
    if (sideCollapsed) {
        reportPane.className = reportPane.className.replace("col-sm-12", "col-sm-7");
        sideCollapsed = false;
    } else {
        reportPane.className = reportPane.className.replace("col-sm-7", "col-sm-12");
        sideCollapsed = true;
    }
    adjustToWindowEvent();
    resizeEditor();
}

// Forces header of area report to remain at the top of the area table during scrolling
// (the header is the row with the column titles - ALUTs, FFs, etc.)
function
stickTableHeader()
{
    if (view !== VIEWS.AREA_SYS && view !== VIEWS.AREA_SRC && view !== VIEWS.OPT && view !== VIEWS.VERIF && view !== VIEWS.INCREMENTAL && view != VIEWS.FMAX_II) return;

    var reportBody = $(currentPane + " #report-body")[0];
    if (!reportBody) return;
    var areaTable = $(currentPane + " #area-table-content")[0];
    if (!areaTable) return;
    var panel = reportBody.getBoundingClientRect();
    var table = areaTable.getBoundingClientRect();
    var rowWidth = 0.0;
    var tableWidth = table.width;
    var systemRow;

    var tableHeader = $(currentPane + ' #table-header').filter(function () {
        if ($(this).is(":visible")) return true;
        return false;
    });

    systemRow = $(currentPane + ' #first-row')
        .filter(function () {
            if ($(this).is(":visible")) return true;
            return false;
        });

    tableHeader.css("position", "absolute")
        .css("top", (panel.top - table.top))
        .css("left", 0);

    tableHeader.find('th').each(function (i) {
        var itemWidth = (systemRow.find('td').eq(i))[0].getBoundingClientRect().width;
        if (i === 0) {
            // This column contains the expand/collapse all button. Check if need to resize button
            if (itemWidth < $('#collapseAll').outerWidth() || itemWidth < 116) {
                $('#collapseAll').outerWidth(itemWidth);
                $('#expandAll').outerWidth(itemWidth);
            } else {
                $('#collapseAll').outerWidth(116);
                $('#expandAll').outerWidth(116);
            }
        }
        rowWidth += itemWidth;

        $(this).css('min-width', itemWidth);
    });

    // Set the Spacer row height equal to current tableHeader height
    systemRow.css("height", tableHeader.outerHeight());

    // if we just hid the selected row, unselect it and clear details pane
    if (view.clickDown && view.clickDown.offsetParent === null) {
        unsetClick();
    }
}

function
setReportPaneHeight()
{
    var viewPortHeight = $(window).height() - 1;
    var navBarHeight = $(".navbar-collapse").height();
    var detailHeight = (detailCollapsed) ? 17 : $("#detail-pane").height() + 27;
    // update the report and editor height
    $('#report-pane, #editor-pane').css('height', viewPortHeight - navBarHeight - detailHeight);

    let panelHeight = $(currentPane).height();
    // Adjust the tree height
    let treeDivList = $(currentPane).find('#tree-list');
    if (treeDivList.length > 0) {
      setTreePanelHeight(treeDivList.eq(0), panelHeight);
    }
    // Adjust the report body, currently are graph or memory viewer
    let reportBodyDivList = $(currentPane).find('#report-pane-col2');
    if (reportBodyDivList.length > 0) {
      setViewerPanelHeight(reportBodyDivList.eq(0), panelHeight);
    }

    var editorHeadingHeight = $('#editor-nav-btn-group').outerHeight();
    // Use class here since all editor w/ file will have this class
    $('.tab-pane').css('height', panelHeight - editorHeadingHeight);
}

/**
 * 
 * @param {Object} details returns by getDetails()
 * @param {String} title shown in details card title
 */
function changeDetailsPane(details, title) {
  if (!details) return;
  let formattedDetails = getHTMLDetailsFromJSON(details, title);
  if (!formattedDetails) return;
  document.getElementById("details").innerHTML = formattedDetails;
}

// TO BE OBSELETED
function
changeDivContent(idx, details)
{
    if (view == VIEWS.LMEM || view == VIEWS.GV) {
        if (!details) return;
        document.getElementById("details").innerHTML = details;
    } else {
        document.getElementById("details").innerHTML = detailValues[idx];
    }
}

function
clearDivContent(){
    document.getElementById("details").innerHTML = "";
}

function
syncEditorPaneToLine( line, filename )
{
    if (portable_mode) {
        gotoFileACEEditorLine(filename, line);  // ACE Editor
    }
    else {
        // IDE use
        if (typeof goToFile !== "undefined") { 
            goToFile(filename, line);  //For ecplise
        } else if ( typeof window !== "undefined" && window.external !== "undefined") {
            window.external.goToFile(filename, line);  // For Visual studio		
        } else {
            alert("Not found goToFile function")
        }
    }
}

function editorNoHighlightActiveLine(event) {
    if (portable_mode) {
        // Get current editor, and disable line highlighting and cursor.
        removeACEEditorHighlight();
    }
}

function syncEditorPaneToLineNoPropagagte(event, line, filename) {
    event.stopPropagation();
    syncEditorPaneToLine(line, filename);
}

function getFilename(path) {
    if (!isValidFileList) return path;

    var name = path;
    for (var i = 0; i < fileJSON.length; i++) {
        if (path.indexOf(fileJSON[i].path) != -1) {
            return fileJSON[i].path;
        } else if (path.indexOf(fileJSON[i].name) != -1) {
            name = fileJSON[i].name;
            break;
        }
    }
    return name;
}

// TO BE OBSELETED. This can be replaced by console.warn and flow assertions
function
warn(condition, message) {
    if (!condition) {
        console.log("WARNING: " + (message || ("Assertion Failed.")));
    }
}
