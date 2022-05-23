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
var FLOWS = {
    NONE:   0,
    OPENCL: 1,
    HLS:    2,
    BOTH:   3
    };

var show_hidden = (((typeof enable_dot != "undefined") && enable_dot == 1) || (typeof schedule_infoJSON != "undefined"))  ? true : false;  // TODO: change this with a compiler flag later

var MENU = {
    SUMMARY:       { value: 1, name: "Summary", view: "SUMMARY" },
    THROUGHPUT:    { value: 2, name: "Throughput Analysis", submenu: ["OPT", "divider", "FMAX_II", "VERIF"] },
    RESOURCE_UTIL: { value: 3, name: "Area Analysis", submenu: ["AREA_SYS", "AREA_SRC", "INCREMENTAL"] },
    VIEWERS:       { value: 4, name: "System Viewers", submenu: ["GV", "divider", "LMEM"] },
    ALPHA_TOOLS:   { value: 9, name: "Alpha Tools", submenu: ["DOT", "SCHEDULE"], hidden: true}   // only unhide with compiler flag
};
var VIEWS = {
    NONE:        { value: 0, name: "",                         flow: FLOWS.NONE},
    SUMMARY:     { value: 1, name: "Summary",                  flow: FLOWS.BOTH},

    //THROUGHPUT:     { value: 20, name: "Performance analysis",   flow: FLOWS.BOTH},
    //PROFILER:    { value: 21, name: "Dynamic profiler",       flow: FLOWS.BOTH},  // TODO: replace with Profiler
    VERIF:       { value: 21, name: "Verification Statistics", flow: FLOWS.HLS},
    OPT:         { value: 22, name: "Loops Analysis",          flow: FLOWS.BOTH},  // TODO: rename this
    FMAX_II:     { value: 23, name: "Fmax II Report",          flow: FLOWS.BOTH},
    //QII_STA:     { value: 24, name: "Quartus Critical Paths",       flow: FLOWS.BOTH },
    //II_EST:      { value: 25, name: "Dynamic II/Latency Estimator", flow: FLOWS.NONE}, // TODO: change to correct flow once activated

    AREA_SYS:    { value: 30, name: "Area Analysis of System",  flow: FLOWS.BOTH},
    AREA_SRC:    { value: 31, name: "Area Analysis of Source (deprecated)",  flow: FLOWS.BOTH},
    //QII_RES:     { value: 31, name: "Quartus Resource Utilization", flow: FLOWS.BOTH},
    INCREMENTAL: { value: 32, name: "Incremental Compile",      flow: FLOWS.OPENCL},

    GV:          { value: 41, name: "Graph Viewer (beta)",         flow: FLOWS.BOTH,   id: "pipeline" },
    LMEM:        { value: 42, name: PREFIX_PAD + " Memory Viewer", flow: FLOWS.BOTH,   id: "lmem"  },
    //EMEM:        { value: 43, name: "External memory viewer", flows: FLOWS.OPENCL},

    // Alpha or internal tools
    DOT:         { value: 90, name: "Dot Graph Viewer",         flow: FLOWS.BOTH, hash: "#dot"},
    SCHEDULE:    { value: 92, name: "Schedule Viewer",          flow: FLOWS.BOTH}
    //OPTIMIZE_ADVISOR: { value: 5, name: "Optimization advisor", view: "" },  // to be enabled by a flag in the future
};
var viewHash = []; // initialized in main::initializeViews()

// vector< fileInfo objects >
var detailValues = [""];
var detailIndex = 0;
var curFile;
var detailOptValues = [];

var sideCollapsed = false;
var detailCollapsed = false;
var view = VIEWS.SUMMARY;
var flow = FLOWS.BOTH;
var currentPane = null;

var LOOP_ANALYSIS_NAME = VIEWS.OPT.name + "<span style='float:right'><input id='showFullyUnrolled' type='checkbox' checked='checked' value='Fully unrolled loops'>&nbspShow fully unrolled loops&nbsp</span>";
var REPORT_PANE_HTML = "<div class='classWithPad' id='opt-area-panel'><div class='panel panel-default' id='report-panel-body'><div class='panel-heading'>";
var NO_SOURCE = "No Source Line";

var isValidFileList      = true;
var isValidSummary       = true;
var isValidWarnings      = true;
var isValidInfo          = true;
var isValidQuartus       = true;

var isValidLoopAnalysis  = true;
var isValidFMaxIIViewer  = true;
var isValidVerifAnalysis = true;

var isValidAreaReport    = true;
var isValidIncremental   = true;

var isValidHierarchyTree = true;
var isValidSystemViewer  = true;
var isValidSoTViewer     = true;
var isValidMemoryViewer  = true;
var isValidPipelineViewer = true;
var isValidBlockViewer   = true;

var isValidDot           = true;
var isValidAreaSRCReport = true;
var isValidScheduler     = true;

var incrementalJSON = null;

function
main()
{
    // check if all information is valid
    isValidSummary       = (typeof summaryJSON != "undefined") && (summaryJSON = tryParseJSON(summaryJSON)) !== null;
    isValidWarnings      = (typeof warningsJSON!= "undefined") && (warningsJSON= tryParseJSON(warningsJSON))!== null;
    isValidInfo          = (typeof infoJSON    != "undefined") && (infoJSON    = tryParseJSON(infoJSON))    !== null;
    isValidQuartus       = (typeof quartusJSON != "undefined") && (quartusJSON = tryParseJSON(quartusJSON)) !== null;

    isValidFileList      = (typeof fileJSON    != "undefined") && (fileJSON    = tryParseJSON(fileJSON))    !== null;

    // Throughput Analysis
    // ThroughputSummary = throughputJSON
    isValidLoopAnalysis  = (typeof loopsJSON   != "undefined") && (loopsJSON   = tryParseJSON(loopsJSON))   !== null;
    isValidFMaxIIViewer  = (typeof fmax_iiJSON != "undefined") && (fmax_iiJSON = tryParseJSON(fmax_iiJSON)) !== null;
    isValidVerifAnalysis = (typeof verifJSON   != "undefined") && (verifJSON   = tryParseJSON(verifJSON))   !== null;

    if( !isValidFMaxIIViewer) VIEWS.FMAX_II.flow = FLOWS.NONE;
    // QuartusCriticalPath = QuartusSTAJSON
    // DynamicIILatencyEst = DynIIJSON

    // Area Analysis
    isValidAreaReport    = (typeof areaJSON    != "undefined") && (areaJSON    = tryParseJSON(areaJSON))    !== null;

    // Deal with JSON files for incremental flow.  There may be two different
    // incremental JSON files, depending on whether this is an incremental setup
    // compile (incremental_initialJSON), or an incremental change
    // (incremental_changeJSON).  We deal with both of them the same throughout
    // the rest of the reports, so just merge them into 'incrementalJSON'.
    var isValidIncrementalInitial = (typeof incremental_initialJSON != "undefined") && (incremental_initialJSON=tryParseJSON(incremental_initialJSON)) !== null;
    var isValidIncrementalChange  = (typeof incremental_changeJSON  != "undefined") && (incremental_changeJSON =tryParseJSON(incremental_changeJSON))  !== null;
    isValidIncremental = isValidIncrementalInitial || isValidIncrementalChange;
    incrementalJSON = isValidIncrementalInitial ? incremental_initialJSON : isValidIncrementalChange ? incremental_changeJSON : null;

    // Viewers
    isValidHierarchyTree = (typeof treeJSON    != "undefined") && (treeJSON    = tryParseJSON(treeJSON))    !== null;
    isValidSystemViewer  = (typeof mavJSON     != "undefined") && (mavJSON     = tryParseJSON(mavJSON))     !== null;
    isValidSoTViewer     = (typeof systemJSON  != "undefined") && (systemJSON  = tryParseJSON(systemJSON))  !== null;
    // FunctionViewer    = FuncJSON
    isValidMemoryViewer  = (typeof new_lmvJSON     != "undefined") && (new_lmvJSON     = tryParseJSON(new_lmvJSON))     !== null;
    isValidBlockViewer    = (typeof blockJSON    != "undefined") && (blockJSON    = tryParseJSON(blockJSON))    !== null;
    isValidPipelineViewer = (typeof pipelineJSON != "undefined") && (pipelineJSON = tryParseJSON(pipelineJSON)) !== null;

    // Alpha level tools
    isValidAreaSRCReport = (typeof area_srcJSON!= "undefined") && (area_srcJSON= tryParseJSON(area_srcJSON))!== null;
    isValidDot           = (typeof enable_dot  != "undefined") && enable_dot == 1;
    isValidScheduler  =    (typeof schedule_infoJSON != "undefined") && (schedule_infoJSON = tryParseJSON(schedule_infoJSON)) !== null;

    if (!isValidDot) VIEWS.DOT.flow = FLOWS.NONE;
    if (!isValidScheduler) VIEWS.SCHEDULE.flow = FLOWS.NONE;
    if (!isValidIncremental) VIEWS.INCREMENTAL.flow = FLOWS.NONE;

    // Add the view number
    Object.keys(VIEWS).forEach(function(v) {
        var index = VIEWS[v];
        if (index.hash === undefined) index.hash = "#view" + index.value;
    });

    // Set page title
    // TODO: need to update when info.json gets new schema
    var pageTitle = "Report";
    if (isValidInfo && infoJSON.hasOwnProperty('rows')) {
        for (var r = 0; r < infoJSON.rows.length; ++r) {
            if (infoJSON.rows[r].hasOwnProperty('name')) {
                if (infoJSON.rows[r].name === "Project Name") {
                    pageTitle += ": " + infoJSON.rows[r].data;
                } else if (infoJSON.rows[r].name.indexOf("i++") >= 0) {
                    flow = FLOWS.HLS;
                } else if (infoJSON.rows[r].name.indexOf("AOC") >= 0) {
                    flow = FLOWS.OPENCL;
                }
            }
        }
    }
    $('#titleText').html(pageTitle);
    // initialize the prefix used for submenu
    function_prefix = (flow == FLOWS.OPENCL) ? "Kernel" : "Function";
    // remove unused/invalid views
    removeInvalidViews();

    // Get area and optimization report
    if (VIEWS.AREA_SYS) {
      VIEWS.AREA_SYS.source = isValidAreaReport ? parseAreaData(areaJSON) : "&nbsp;Area report data is invalid!";
    }
    if (VIEWS.AREA_SRC) {
      VIEWS.AREA_SRC.source = isValidAreaSRCReport ? parseAreaData(area_srcJSON) : "&nbsp;Area report data is invalid!";
    }

    if (VIEWS.SUMMARY) {
        if (isValidSummary && isValidInfo && isValidWarnings) {
            if (isValidQuartus) {
                VIEWS.SUMMARY.source = parseSummaryData(infoJSON, warningsJSON, summaryJSON, quartusJSON);
            } else {
                VIEWS.SUMMARY.source = parseSummaryData(infoJSON, warningsJSON, summaryJSON, undefined);
            }
        } else {
            VIEWS.SUMMARY.source = "&nbspSummary data is invalid!";
        }
    }

    if (VIEWS.OPT) {
        if (isValidLoopAnalysis && !$.isEmptyObject(loopsJSON)) {
            VIEWS.OPT.source = parseLoopData(loopsJSON);
        } else {
            VIEWS.OPT.source = "&nbspLoop analysis data is invalid!";
        }
    }

    if(VIEWS.FMAX_II) {
        if (isValidFMaxIIViewer) {
            var data = fmax_iiJSON;
            var table = renderFmaxIITable(data);
            VIEWS.FMAX_II.source = table;
        } else {
            VIEWS.FMAX_II.source = "&nbspFmax II report data is invalid!";
        }
    }

    if (VIEWS.VERIF) {
        if (isValidVerifAnalysis) {
            VIEWS.VERIF.source = parseVerifData(verifJSON);
        } else {
            VIEWS.VERIF.source = "&nbspVerification analysis data is unavailable!\n";
            VIEWS.VERIF.source += "Run the verification testbench to generate this information.";
        }
    }

    if (isValidDot) {
      VIEWS.DOT.source = "<div id='svg_container' style='width:100%;height:100%;margin:0 auto;overflow:hidden;'></div>";
    }

    if (VIEWS.INCREMENTAL) {
        if (isValidIncremental) {
            VIEWS.INCREMENTAL.source = parseIncrementalData(incrementalJSON);
        } else {
            VIEWS.INCREMENTAL.source = "&nbspIncremental compile data is unavailable!\n";
        }
    }

    initializeViews();

    if (isValidFileList) {
      // Add file tabs to the editor pane
      addFileTabs( fileJSON );
      adjustToWindowEvent();
    } else {
      $('#editor-pane').toggle();
      $('#report-pane').css('width', '100%');
      sideCollapsed = true;
      adjustToWindowEvent();
    }

    ///// Functions

    /// main::initializeViews()
    function initializeViews() {
        // create a div for each view, and set it to "hidden"; also create
        // a menu entry for each view
        Object.keys(MENU).forEach(function(m) {
            var views = MENU[m];
            var li;
            // hide hidden menu - TODO: consolide isValidScheduler the flag to show_hidden flag
            if (views.hidden !== undefined && !show_hidden && !isValidScheduler) return;

            if (views.view !== undefined) {
                // no submenu, the menu is the view
                var v = MENU[m].view;
                var index = VIEWS[v];
                $("#report-pane")[0].insertAdjacentHTML("beforeend", "<div id=\"report-pane-view" + index.value + "\" class=\"report-pane-view-style\"></div>");
                $("#report-pane-view" + index.value).toggle();
                li = "<li class=\"";
                li += "nav-item\" viewId=" + v + "><a href=\"";
                li += index.hash + "\" ";
                li += ">" + index.name + "</a></li>";
                $("#main-menu")[0].insertAdjacentHTML("beforeend", li);
                viewHash[index.hash] = v;
                addReportColumn(index);
            } else {
                // has submenu
                // add the menu
                var adv_list = "<li class=\"dropdown\">\n";
                adv_list += "<a class=\"dropdown-toggle\" data-toggle=\"dropdown\" role=\"button\" aria-haspopup=\"true\" aria-expanded=\"false\">";
                adv_list += views.name;
                adv_list += "<span class=\"caret\"></span></a>\n";
                adv_list += "<ul class=\"dropdown-menu\" id=\"view-menu" + views.value + "\">\n";
                adv_list += "</ul>\n</li>\n";
                $("#main-menu")[0].insertAdjacentHTML("beforeend", adv_list);
                views.submenu.forEach(function(v) {
                    if (v === "divider") {
                        li = "<li role=\"separator\" class=\"divider\"></li>";
                        $("#view-menu" + views.value)[0].insertAdjacentHTML("beforeend", li);
                    }
                    else if (VIEWS[v] !== undefined) {
                        // Don't added removed views
                        var index = VIEWS[v];
                        if (index.name !== "") {
                            $("#report-pane")[0].insertAdjacentHTML("beforeend", "<div id=\"report-pane-view" + index.value + "\" class=\"report-pane-view-style\"></div>");
                            $("#report-pane-view" + index.value).toggle();
                            li = "<li class=\"dropdown_nav\" viewId=" + v + "><a href=\"";
                            if (index == VIEWS.DOT) {
                                // This allows us to update the DOT viewer hash dynamically
                                li += index.hash + "/" + dot_top + "\" id=\"dot_viewer_dropdown_nav\" ";
                            } else {
                                li += index.hash + "\" ";
                            }
                            li += "style='color:black'>" + index.name + "</a></li>";
                            $("#view-menu" + views.value)[0].insertAdjacentHTML("beforeend", li);
                        }
                        viewHash[index.hash] = v;
                        addReportColumn(index);
                    }
                });
            }
        });
        // display the current view
        currentPane = "#report-pane-view" + view.value;
        $(currentPane).toggle();
    }

    /// main::addFileTabs
    function
    addFileTabs( fileJSON )
    {
    var navTabs = d3.select( "#editor-pane" ).selectAll( "#editor-pane-nav" );
    var listElements = navTabs.selectAll( "li" )
        .data( fileJSON )
        .enter()
        .append( "li" )
        .attr( "class", function( d, i ) {
            var classname = "";
            if (i === 0) {
                classname = "active";
                $('.selected').html(d.name);
                $('.mouseoverbuttontext').html(d.absName);
                curFile = d.path;
            }
            if(d.has_active_debug_locs) {
              classname += " has_active_debug_locs";
            }
            fileIndexMap[d.path] = i;
            return classname;
        });

    var anchors = listElements
        .append( "a" )
        .attr( "class", "mouseover")
        .attr( "data-target", function( d, i ) { return "#file" + i; } )
        .text( function( d ) { return d.name; });

    //show file path information using hover text
    anchors = listElements
        .append( "p" )
        .attr( "class", "mouseovertext")
        .text( function( d ) {
          return d.absName;
        });

    $( "#editor-pane-nav" ).on( "click", "a", function( e ) {
        $(this).tab("show");
        $("#editor-pane-nav li").removeClass("active");
        $(this).attr("class", "active");

        $('.selected').html($(this).text());
        $('.mouseoverbuttontext').html($(this).next()[0].innerHTML);
    });

    var divs = d3.select( "#editor-pane" ).selectAll( ".tab-content" )
        .selectAll( "div" )
        .data( fileJSON )
        .enter()
        .append( "div" )
        .attr( "class", function( d, i ) {
            var classname = "tab-pane";
            if ( i === 0 ) classname = classname + " in active";
            return classname;
        })
        .attr( "id", function( d, i ) { return "file" + i; } )
        .attr( "style", "height:500px;" );

    var editorDivs = divs
        .append( "div" )
        .attr( "class", "well" );

    editorDivs.each( SetupEditor );

    /// Functions
    function
    SetupEditor( fileInfo )
    {
        var editor = ace.edit( this ); // "this" is the DOM element
        fileInfo.editor = editor;

        editor.setTheme( "../ace/theme/xcode" );
        editor.setFontSize( 12 );
        editor.getSession().setMode( "../ace/mode/c_cpp" );
        editor.getSession().setUseWrapMode( true );
        editor.getSession().setNewLineMode( "unix" );

        // Replace \r\n with \n in the file content (for windows)
        editor.setValue( fileInfo.content.replace( /(\r\n)/gm, "\n" ) );
        editor.setReadOnly( true );
        editor.scrollToLine( 1, true, true, function() {} );
        editor.gotoLine( 1 );
    }
  }
}

///// Global functions

// remove invalid views from the VIEWS object and add prefix to view name
// views are invalid if the JSON is missing or invalid, or if the view is invalid for the given flow
function removeInvalidViews() {
    Object.keys(VIEWS).forEach(function(v) {
        var sel_view = VIEWS[v];
        if (sel_view.flow != FLOWS.BOTH && sel_view.flow != flow) {
            delete VIEWS[v];
        } else {
            // add clickDown and source fields
            sel_view.clickDown = null;
            sel_view.source = null;
            // Prepend 'Kernel' or 'Component' for different views depending on flow
            sel_view.name = sel_view.name.replace(PREFIX_PAD, function_prefix);
        }
    });
}

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
    switch (view) {
      case VIEWS.LMEM:
        if (!lmem_graph && isValidMemoryViewer) {
            var hasLMem = addHierarchyTree(VIEWS.LMEM);
            if (hasLMem) $('#LMEMG').html("<br>&nbspClick on a memory variable to render it!");
            else $('#LMEMG').html("&nbspThere is no " + ((flow == FLOWS.OPENCL) ? "kernel" : "component") + " memory variable in the design file!");
        }
        else if (isValidMemoryViewer) lmem_graph.refreshGraph();
        else $('#LMEMG').html("&nbsp " + view.name + " data is invalid!");
        break;
      case VIEWS.GV:
        if (isValidPipelineViewer) addHierarchyTree(VIEWS.GV);
        else $('#' + GRAPH_CONST.GV.gid).html("&nbsp" + view.name + " data is invalid!");
        break;
      case VIEWS.DOT:
        var hash_split = window.location.hash.split("/");
        putSvg(hash_split[hash_split.length - 1]);
        break;
      case VIEWS.SCHEDULE:
        if(isValidScheduler){
            var data = schedule_infoJSON; 
            renderScheduleTable("#SCHEDULE", data);
        }
        break;
    }
    refreshAreaVisibility();
    adjustToWindowEvent();
}

function addReportColumn(reportEnum) {
    var report = REPORT_PANE_HTML;

    switch (reportEnum) {
      case VIEWS.SUMMARY:
        report += reportEnum.name + "</div><div class='panel-body' id='report-body' onscroll='adjustToWindowEvent()'>";
        report += "<div id='area-table-content'></div>";
        break;
      case VIEWS.OPT:
        report += LOOP_ANALYSIS_NAME;
        report += "</div><div class='panel-body' id='report-body' onscroll='adjustToWindowEvent()'>";
        report += "<table class='table table-hover' id='area-table-content'></table>";
        break;
      case VIEWS.FMAX_II:
        report += "Fmax II Report";
        report += "</div><div class='panel-body' id='report-body' onscroll='adjustToWindowEvent()'>";
        report += "<table class='table table-hover' id='area-table-content'></table>";
        break;
      case VIEWS.VERIF:
        report += reportEnum.name + "</div><div class='panel-body' id='report-body' onscroll='adjustToWindowEvent()'>";
        report += "<table class='table table-hover' id='area-table-content'></table>";
        break;
      case VIEWS.AREA_SRC:
      case VIEWS.AREA_SYS:
        report += "<table style='width:100%'><tr><td class='panel-heading-text'>";
        report += reportEnum.name + "<br>(area utilization values are estimated)<br>Notation <i>file:X</i> > <i>file:Y</i> indicates a function call on line X was inlined using code on line Y.";
        if (isValidAreaReport && isValidIncremental) {
          report += "<br><strong>Note: Area report accuracy may be reduced when performing an incremental compile.</strong>";
        }
        if (isValidAreaReport && !areaJSON.debug_enabled) {
            report += "<br><strong>Recompile without <tt>-g0</tt> for detailed area breakdown by source line.</strong>";
        }
        report += "</td><td>";
        report += "<span style='float:right'>";
        report += "<button id='collapseAll' type='button' class='text-left exp-col-btn'><span class='glyphicon glyphicon-chevron-up'></span>&nbsp;Collapse All&nbsp;</button>";
        report += "<button id='expandAll' type='button' class='text-left exp-col-btn'><span class='glyphicon glyphicon-chevron-down'></span>&nbsp;Expand All&nbsp;</button>";
        report += "</span>";
        report += "</td></tr></table>";

        report += "</div><div class='panel-body' id='report-body' onscroll='adjustToWindowEvent()'>";
        report += "<table class='table table-hover' id='area-table-content'></table>";
        break;
      case VIEWS.INCREMENTAL:
        if (incrementalJSON.hasOwnProperty('name')) {
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
        report += "<div class='panel-body' id='report-body' onscroll='adjustToWindowEvent()'>";
        report += "<table class='table table-hover' id='area-table-content'></table>";
        break;
      case VIEWS.LMEM:
        report = addTreePanel(reportEnum);
        report += "<div class=\"col col-sm-9\" id=\"report-pane-col2\">";
        report += addViewerPanel(reportEnum, "layers-lmem", "LMEMG");
        break;
      case VIEWS.GV:
        report = addTreePanel(reportEnum);
        report += "<div class=\"col col-sm-9\" id=\"report-pane-col2\">";
        report += addViewerPanel(reportEnum, "layers", "GVG");
        break;
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

        report += "</div><div class='panel-body' id='report-body' onscroll='adjustToWindowEvent()'>";
        report += "<div id='area-table-content' style='height:100%;'></div>";
        break;
      case VIEWS.SCHEDULE:
        report += "</div><div class='panel-body' id='report-body'>";
        report += "<div id='SCHEDULE' class='panel-body fade in active' ></div></div>";
        break;
    }

    report += "</div></div></div>";

    $("#report-pane-view" + reportEnum.value).html(report);
    $("#report-pane-view" + reportEnum.value + " #area-table-content").html(reportEnum.source);
}

function addTreePanel(view) {
    var headerName = view.name.replace("Viewer", "List");
    var idName = getTreeIDName(view.id);
    var filterId = idName + "-filter";
    var buttonId = idName + "-button";

    // Add a filter div for each tree (currently used in LMEM viewer to filter tree entries by type)
    // Possible TO-DO: Populate filter div for other views to filter tree entries
    var C_VIEWER_TREE_PANEL_PREFIX = "<div id=\"tree-list\" class=\"col col-sm-3\" id=\"report-pane-col1\"><div class=\"panel panel-default\" id=\"report-panel-tree\"><div class=\"panel-heading\">";
    var C_VIEWER_TREE_FILTER_PREFIX  = "<div class=\"btn-group pull-right\" id=\"";
    var C_VIEWER_TREE_FILTER_BODY = "\"><button type=\"button\" class=\"btn btn-default btn-sm dropdown-toggle\" data-toggle=\"dropdown\"><span class=\"glyphicon glyphicon-filter\"></span> <span class=\"caret\"></span></button><div id=\""; 
    var C_VIEWER_TREE_FILTER_SUFFIX = "\" class=\"dropdown-menu dropdown-menu-right\"></div></div>";

    var C_VIEWER_TREE_BODY_PREFIX  = "</div><div id=\"tree-body\" class='panel-body'><div id=\"";
    var C_VIEWER_TREE_BODY_SUFFIX  = "\"></div></div></div></div>";

    var panelHTML = C_VIEWER_TREE_PANEL_PREFIX;

    // Add filter dropdown menu only for LMEM viewer for now
    if(view.name === VIEWS.LMEM.name) {
        panelHTML += C_VIEWER_TREE_FILTER_PREFIX + buttonId + C_VIEWER_TREE_FILTER_BODY + filterId + C_VIEWER_TREE_FILTER_SUFFIX;
    }

    panelHTML += headerName ;
    panelHTML += C_VIEWER_TREE_BODY_PREFIX + idName + C_VIEWER_TREE_BODY_SUFFIX;
    return panelHTML;
}

function addViewerPanel(view, layerId, panelId) {
    var headerName = view.name;
    var idName = view.id;
    var C_VIEWER_PANEL_PREFIX = "<div class=\"classWithPad\" id=\"";
    var C_VIEWER_HEADER_PREFIX = "\"><div class=\"panel panel-default\" id=\"report-panel-body\"><div class=\"panel-heading\">";
    var C_VIEWER_HEADER_SUFFIX = "<span class=\"currentEntity\"></span><span style='float:right'><div id=\"";
    var C_VIEWER_BODY_PREFIX  = "\"></div></span></div><div id='";
    var C_VIEWER_PANEL_SUFFIX = "' class='panel-body fade in active'></div>";
    var panelHTML = C_VIEWER_PANEL_PREFIX + idName + C_VIEWER_HEADER_PREFIX;
    panelHTML += headerName + C_VIEWER_HEADER_SUFFIX + layerId + C_VIEWER_BODY_PREFIX;
    panelHTML += panelId + C_VIEWER_PANEL_SUFFIX;
    return panelHTML;
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

// navigation bar tree toggle
$(document).ready(function () {
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
        $(this).find('.ar-toggle').toggleClass('glyphicon-chevron-down glyphicon-chevron-right');
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
            $.each(iconsToToggle, function () {
                if ($(this).hasClass('glyphicon-chevron-right'))
                    $(this).toggleClass('glyphicon-chevron-down glyphicon-chevron-right');
            });
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
            $.each(iconsToToggle, function () {
                if ($(this).hasClass('glyphicon-chevron-down'))
                    $(this).toggleClass('glyphicon-chevron-down glyphicon-chevron-right');
            });
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

function
collapseAceEditor()
{
    if (!isValidFileList) return;

    $('#editor-pane').toggle();
    if (sideCollapsed && isValidFileList) {
        $('#report-pane').css('width', '60%');
        sideCollapsed = false;
    } else {
        $('#report-pane').css('width', '100%');
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
adjustEditorButtons()
{
    var editorWidth = $("#editor-pane").width();
    var editorExitButton = $("#close-source").outerWidth(true);
    $("#editor-nav-button").css("width", editorWidth - editorExitButton - 1);
}

function
setReportPaneHeight()
{
    var viewPortHeight = $(window).height() - 1;
    var navBarHeight = $(".navbar-collapse").height();
    var detailHeight = (detailCollapsed) ? 16 : $("#detail-pane").height();
    $('#report-pane, #editor-pane').css('height', viewPortHeight - navBarHeight - detailHeight);

    var panelHeight = $("#report-pane").height();
    var panelHeadingHeight = $(currentPane + ' .panel-heading').outerHeight();
    $(currentPane + ' #report-body').css('height', panelHeight - panelHeadingHeight);
    $(currentPane).css('height', $('#report-pane').innerHeight());

    if (view == VIEWS.LMEM) {
        $('#LMEMG').css('height', panelHeight - panelHeadingHeight);
        $('#lmemg').css('height', panelHeight - panelHeadingHeight);
        $('#lmemg').css('width', $('#report-pane').innerWidth());
        $('ul.fancytree-container').css('height', panelHeight - panelHeadingHeight);
    }

    if (view == VIEWS.GV) {
        $('#GVG').css('height', panelHeight - panelHeadingHeight);
        $('#gvg').css('height', panelHeight - panelHeadingHeight);
        $('#gvg').css('width', $('#report-pane').innerWidth());
        $('ul.fancytree-container').css('height', panelHeight - panelHeadingHeight);
    }

    var editorHeadingHeight = $('.input-group-btn').outerHeight();
    $('.tab-pane').css('height', panelHeight - editorHeadingHeight);

}

function
changeDivContent(idx, details)
{
    if (view == VIEWS.LMEM || view == VIEWS.GV) {
        if (!details) return;
        document.getElementById("details").innerHTML = "<ul class='details-list'>" + details + "</ul>";
    } else {
        document.getElementById("details").innerHTML = "<ul class='details-list'>" + detailValues[idx] + "</ul>";
    }
}

function
clearDivContent(){
    document.getElementById("details").innerHTML = "<ul class='details-list'></ul>";
}

function
syncEditorPaneToLine( line, filename )
{
    var node;
    var editor;
    var index = 0;

    if (line == -1 || !isValidFileList) return;
    curFile = filename;

    index = fileIndexMap[filename];
    if (index === undefined) {
      // Try searching without the preceding "./", if it exists
      filename = filename.replace(/^\.\//, '');
      index = fileIndexMap[filename];
      if (index === undefined) {
        for (var file in fileIndexMap) {
          var ndx = file.lastIndexOf(filename);
          if (file.endsWith(filename) && ndx > 0 && (file[ndx-1] == '/' || file[ndx-1] == '\\')) {
            index = fileIndexMap[file];
            break;
          }
        }
      }
    }

    editor = (fileJSON[index]) ? fileJSON[index].editor : undefined;

    warn( editor, "Editor invalid!" );
    warn( line > 0, "Editor line number invalid!" );

    if (!editor || line < 1) return;

    var target = "li:eq(" + index + ")";
    $("#editor-pane-nav li").removeClass("active");
    $( "#editor-pane-nav " + target + " a" ).tab( "show" );
    $('.selected').html($("#editor-pane-nav " + target + " a").text());
    $('.mouseoverbuttontext').html($("#editor-pane-nav " + target + " p").text());
    editor.focus();
    editor.resize(true);
    editor.scrollToLine( line, true, true, function() {} );

    // Enable line highlighting and cursor
    editor.setHighlightActiveLine(true);
    editor.setHighlightGutterLine(true);
    editor.renderer.$cursorLayer.element.style.opacity = 1;

    editor.gotoLine( line );
}

function editorNoHighlightActiveLine(event) {
  // Get current editor, and disable line highlighting and cursor.
  var editor = ace.edit($("#editor-pane-content div.active div.well")[0]);
  editor.setHighlightActiveLine(false);
  editor.setHighlightGutterLine(false);
  editor.renderer.$cursorLayer.element.style.opacity = 0;
}

function syncEditorPaneToLineNoPropagagte(event, line, filename) {
    event.stopPropagation();
    syncEditorPaneToLine(line, filename);
}

function getShortFilename(path) {
    if (!isValidFileList) return path;
    for (var i = 0; i < fileJSON.length; i++)
        if (path.indexOf(fileJSON[i].name) != -1) return fileJSON[i].name;
    return path;
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

function
warn(condition, message) {
    if (!condition) {
        console.log("WARNING: " + (message || ("Assertion Failed.")));
    }
}
