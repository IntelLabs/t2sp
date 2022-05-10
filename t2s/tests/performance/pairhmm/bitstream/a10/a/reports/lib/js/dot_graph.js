"use strict";

// disable JSHint warning: Use the function form of "use strict".
// This warning is meant to prevent problems when concatenating scripts that
// aren't strict, but we shouldn't have any of those anyway.
/* jshint -W097 */

// Global variables for the dot graph viewer.
var dot_editor = null;
var dot_clickdown = null;

// Get the current hierarchy and display it on the view titlebar.
function updateDotHierarchy() {
  var hash = window.location.hash;
  var hash_split = hash.split("/");
  var hierarchy = "";
  if (view == VIEWS.DOT) {
    for (var i = 1; i < hash_split.length; i++) {
      hierarchy += "|" + hash_split[i].replace(/\d+_/, '');
    }
  }
  $("#dot_hierarchy_text").html(hierarchy);
}

// Update the page hash to force the 'onhashchange' event handler to call
// goToView, which should then invoke putSvg
function goToDot(dot_hash) {
  $("#dot_viewer_dropdown_nav").attr("href", dot_hash);
  window.location.hash = dot_hash;
}

// Load and display the specified SVG.
function putSvg(next) {
  if (next == VIEWS.DOT.hash) {
    goToDot(next + "/" + dot_top);
    return;
  }
  if (!next) next = dot_top;
  var root = d3.select("#svg_container");
  d3.xml("lib/dot_svg/" + next + ".svg", function(error, documentFragment) {
       if (error) {console.log(error); return;}
       var svgNode = documentFragment.getElementsByTagName("svg")[0];

       var w = $(root.node()).width();
       var h = $(root.node()).height();

       svgNode.setAttribute("viewBox", "0 0 " + w + ' ' + h);
       svgNode.setAttribute("style", "width: " + w + 'px;height:' + h + "px;");

       var d = d3.select(svgNode);

       if (root.node().childNodes.length > 0) {
           root.node().replaceChild(svgNode, root.node().childNodes[0]);
       } else {
           root.node().appendChild(svgNode);
       }
       var matrix = d.select("g").node().transform.baseVal.consolidate().matrix;
       var X = matrix.e;
       var Y = matrix.f;
       var width = parseInt(svgNode.getAttribute("width"), 10);
       var height = parseInt(svgNode.getAttribute("height"), 10);
       var x_scale = w / width;
       var y_scale = h / height;
       var initial_scale = Math.min(x_scale, y_scale);
       initial_scale = Math.min(initial_scale, 1);

       var translate_x = ((w - width*initial_scale) / 2);
       var translate_y = ((h - height*initial_scale) / 2);

       // Setup event listeners for model nodes.
       root.selectAll("polygon").each(function() {
            if (this.hasAttribute("fill")) {
                // All model nodes should show the corresponding HDL, or the
                // corresponding DOT graph when clicked.  dsdk::A_MODEL_NODEs
                // are magenta, dsdk::A_SCHEDULED_MODEL_NODEs are white, and
                // are not a direct descendant of the "graph" (the background
                // is also white, but is a direct descendant of the "graph".
                if (this.getAttribute("fill") == "magenta" ||
                    (this.getAttribute("fill") == "white" && !this.parentNode.classList.contains("graph"))) {
                    var g = this.parentNode;
                    var title = g.getElementsByTagName("title")[0].innerHTML;
                    g.addEventListener("click", function(e){
                          // Ctrl+Click opens source, plain click opens graph.
                          var evt = window.event || e;
                          if (evt.ctrlKey) {
                            // TODO Eventually the filename will probably need
                            // to include a kernel/component subdirectory.
                            var filename = title.replace(/^\d+_/, "")+".vhd";
                            addHdlFileToEditor("lib/hdl/" + filename);
                            if (sideCollapsed) {
                              collapseAceEditor();
                              refreshAreaVisibility();
                              adjustToWindowEvent();
                            }
                          } else {
                            var new_hash = window.location.hash + "/" + title;
                            VIEWS.DOT.hash = new_hash;
                            goToDot(new_hash);
                          }
                      });
                }
            }
       });

       // Clickdown for edge highlighting.
       root.selectAll("g.edge path")
         .on('click', function () {
             // TODO use parent
             if (dot_clickdown == this) {
               dot_clickdown = null;
             } else {
               dot_clickdown = this;
             }
             });

       // Edge/arrowhead highlighting.
       var highlightColor = "#1d99c1";
       root.selectAll("g.edge path")
         .on('mouseover', function () {
             $(this).attr({"stroke-width":5, "stroke":highlightColor});
             $(this).siblings("polygon").attr({"fill":highlightColor, "stroke":highlightColor, "stroke-width":3});
             })
         .on('mouseout', function () {
             if (dot_clickdown == this) return;
             $(this).attr({"stroke-width":1, "stroke":"black"});
             $(this).siblings("polygon").attr({"fill":"black", "stroke":"black", "stroke-width":1});
             });
       root.selectAll("g.edge polygon")
         .on('mouseover', function () {
             $(this).siblings("path").attr({"stroke-width":5, "stroke":highlightColor});
             $(this).attr({"fill":highlightColor, "stroke":highlightColor, "stroke-width":3});
             })
         .on('mouseout', function () {
             if (dot_clickdown == this) return;
             $(this).siblings("path").attr({"stroke-width":1, "stroke":"black"});
             $(this).attr({"fill":"black", "stroke":"black", "stroke-width":1});
             });


       // TODO use translateExtent() to prevent translating out of frame?
                    //.translateExtent([10 - X, 10 - Y, 2 * X - 10, 2 * Y - 10])
                    //.translateExtent([[0, 0], [2 * Y - 10]])

       var scaled_x = initial_scale * X;
       var scaled_y = initial_scale * Y;
       scaled_x += translate_x;
       scaled_y += translate_y;
       var zoom = d3.behavior.zoom()
                    .scaleExtent([0, 1.5])
                    .scale(initial_scale)
                    .translate([scaled_x, scaled_y])
                    .on("zoom", zoomed);
       d.select("g").attr("transform","translate(" + scaled_x + "," + scaled_y +")scale(" + initial_scale + "," + initial_scale + ")");
       d.call(zoom);
       if (!$('#dot_edges_toggle').is(':checked')) {
         $("#svg_container").find("g.edge").toggle();
       }
   });
  updateDotHierarchy();
}
