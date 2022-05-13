"use strict";

// return details object or null -- doesn't index into details
function getDetails(node) {
  if (!node || !node.details) return null;
  if (node.details.length > 1) {
    // Still support an array but switching to remove it in the future to simplify details schema
    console.warn(node.details.length > 1, "Details have more than 1 item!");
  }
  return node.details;
}

function getHTMLName(nameText) {
  // C++ user friendly supports special characters
  let name = nameText;
  if (nameText.includes("<") || nameText.includes(">")) {
    name = name.replace("<", "&lt;");
    name = name.replace(">", "&gt;");
  }
  return name;
}

// Generate html details from JSON
function getHTMLDetailsFromJSON(JSON_details, title) {
  if (!JSON_details || !JSON_details.length) return "";

  var DETAIL = {
      TEXT:    "text",
      BRIEF:   "brief",
      TABLE:   "table",
      UNKNOWN: "unknown"  // for temporary use when tracking bullet points,
                          // none of the JSON details should have this type
  };

  // Details uses bootstrap card: card-title and card-body
  var html_details = "";
  if (title !== undefined) {
    // Safe guard to show pipe wording for SYCL
    let htmlTitle = getHTMLName(title);
    if (product === PRODUCTS.SYCL && htmlTitle.includes("Channel")) {
      let renamed_title = htmlTitle.replace("Channel", "Pipe");
      html_details = "<h6 class=\"card-title\">" + renamed_title + ":</h6>";
    } else {
      html_details = "<h6 class=\"card-title\">" + htmlTitle + ":</h6>"
    }
  }
  var list_stack = [];
  list_stack.push(DETAIL.UNKNOWN);
  return generateHTMLDetails(html_details, JSON_details, list_stack);

  // Generate html details from array (list) or object (table)
  function generateHTMLDetails(html_details, JSON_details, list_stack) {

    JSON_details.forEach(function (detail) {
      var use_html_list = implementHTMLList(JSON_details, list_stack);

      // add <ul> and </ul> tags for details which are implemented in list style
      if (detail.type == DETAIL.TEXT && (list_stack[list_stack.length-1] == DETAIL.UNKNOWN ||
        list_stack[list_stack.length-1] == DETAIL.TABLE) && use_html_list) {
        html_details += "<ul style='margin-left: 10px; padding-left: 10px;'>";
      } else if (detail.type === DETAIL.TABLE && list_stack[list_stack.length-1] === DETAIL.TABLE &&
                 use_html_list) {
        html_details += "</ul>";
      }

      // keep track of type of details being printed
      list_stack[list_stack.length-1] = detail.type;

      // generate <li> or <table> html detail
      html_details = generateSingleHTMLDetail(html_details, detail, list_stack, use_html_list);
    });

    // close last list if necessary
    if (list_stack[list_stack.length-1] == DETAIL.TEXT && implementHTMLList(JSON_details, list_stack)) {
      html_details += "</ul>";
    }

    list_stack.pop();
    return html_details;
  }

  // Generate html table data or list item for detail
  function generateSingleHTMLDetail(html_details, JSON_details, list_stack, use_li) {
    // if detail is type table:
    // print key in first column, value in second column
    if (JSON_details.type == "table") {
      // these variables are needed to sort the table details in a more reasonable order;
      // Required size and Implemented size come one after the other, bank information stays together, Additional info comes at the end
      var req_and_impl_size = [];
      var banks_related = [];
      var additional_info_html = "";
      var other_details = [];
      var reference_link = "";
      var html_temp;

      html_details += "<table id='DetailsTable' class='table-striped'>";
      Object.keys(JSON_details).forEach( function (key) {
        html_temp = "";
        if (key === "type") return;
        // Temporary workaround to hide latency for system
        if (key === "Latency" && top_node_id === 0) { return; }
        html_temp += "<tr>";
        html_temp += "<td>" + key + "</td>";
        let value = JSON_details[key];
        if (value instanceof Array) {
          list_stack.push(DETAIL.UNKNOWN);
          html_temp += "<td>";
          html_temp = generateHTMLDetails(html_temp, value, list_stack);
          html_temp += "</td>";
        } else {
          html_temp += "<td>" + value + "</td>";
        }

        // TODO: move data grouping into a function and pass in as a callback in generateSingleHTMLDetail
        // group the detail into the appropriate group depending on the key
        if (key.indexOf("Additional information") != -1){
          additional_info_html = html_temp;
        } else if (key.indexOf("Requested size") != -1) {
          req_and_impl_size.unshift(html_temp);
        } else if (key.indexOf("Implemented size") != -1) {
          req_and_impl_size.push(html_temp);
        } else if (key.toLowerCase().indexOf("bank") != -1 || key.indexOf("Total replication") != -1) {
          banks_related.push(html_temp);
        } else if (key.toLowerCase().indexOf("reference") != -1) {
          reference_link = html_temp;
        } else {
          other_details.push(html_temp);
        }
      });
      // add the details to the actual html_details string in the appropriate order
      req_and_impl_size.forEach( function(html_string) {
        html_details += html_string;
      });
      banks_related.forEach( function(html_string) {
        html_details += html_string;
      });
      other_details.forEach( function(html_string) {
        html_details += html_string;
      });
      html_details += additional_info_html;
      html_details += reference_link;

      html_details += "</table>";

    // otherwise add text detail
    // add as <li> unless only single item
    } else {
      if (use_li) html_details += "<li>";
        html_details += insertLinks(JSON_details.text, JSON_details.links);
      if (JSON_details.details) {
        list_stack.push(DETAIL.UNKNOWN);
        html_details = generateHTMLDetails(html_details, JSON_details.details, list_stack);
      }
      if (use_li) html_details += "</li>";
    }
    return html_details;
  }

  // Given the text and links, replace the instances of '%L' with the proper link information
  // Guide: add <a></a> link to OpenCL Best Practices Guide
  // View:  add <a></a> link to switch views (to Area analysis, Loops report, etc.)
  // File:  add <a></a> link to file and source line in editor
  function insertLinks(text, links) {
    if (links === undefined) return text;
    links.forEach(function (link) {
      var link_name = "";

      // insert link to Best Practices Guide
      if (link.guide) {
        link_name = "<a href='" + link.link + "'>" + link.guide + "</a>";

      // insert link to alternate view
      } else if (link.view) {
        var jsonView = VIEWS.SUMMARY;
        Object.keys(VIEWS).forEach(function (v) {
          if (VIEWS[v].name === link.view) jsonView = VIEWS[v];
        });
        link_name = "<a href='" + jsonView.hash + "' onClick='goToView(" + jsonView.value + ", true)'>" + jsonView.name + "</a>";

      // insert link to file and line
      } else if (link.filename == "Unknown location") {
        link_name = "<a href='#' onClick='editorNoHighlightActiveLine()'>Unknown location</a>";
      } else {
        link_name = "<a href='#' onClick='syncEditorPaneToLine(" + link.line + ", \"" +
                    getFilename(link.filename) + "\")'>" + link.filename.substring(link.filename.lastIndexOf('/') + 1) + ": " + link.line + "</a>";
      }
      text = text.replace(/\%L/, link_name);
    });
    return text;
  }

  // Return true if details should be implemented in an html list
  function implementHTMLList(details, list_stack) {
    // print the details in a point form list if:
    //   1. there are multiple details                   or
    //   2. the detail has subdetails                    or
    //   3. the details is a subdetail of a text detail
    //   Note: it is assumed that the detail is of type 'text'
    return details.length > 1 ||
          (details.length == 1 && details.hasOwnProperty('details') && details.details.length) ||
          (list_stack.length > 1 && list_stack[list_stack.length - 2] == DETAIL.TEXT);
  }
}
