"use strict";

// disable JSHint warning: Use the function form of "use strict".
// This warning is meant to prevent problems when concatenating scripts that
// aren't strict, but we shouldn't have any of those anyway.
/* jshint -W097 */

/**
 * @function tryParseJSON Validates if the string is in JSON format
 * 
 * @param {String} json is the data in quotes in JS format.
 * @returns {Object} data if valid. Otherwise null if the JSON is empty or invalid.
 */
function tryParseJSON(json) {
  var valid = !$.isEmptyObject(json);
  if (valid) {
    if (typeof json !== "string") {
      // If we've gotten to this point, this is already a valid Javascript object.
      return json;
    }
    try {
      return JSON.parse(json);
    } catch(e) {
      console.log(e);
      return null;
    }
  } else {
    return null;
  }
}

/**
 * 
 * @param {Object} node in JSON data structure.
 * @returns {Boolean} true if it has debug location, false otherwise.
 */
function hasDebug(node) {
  return node.hasOwnProperty('debug');
}

/**
 * 
 * @param {Object} node in JSON data structure.
 * @returns {Object} {filename:String, line:integer}.
 */
function getFirstDebug(node) {
  if (node.hasOwnProperty('debug')) {
    return node.debug[0][0];
  }
  return undefined;
}

/**
 * 
 * @param {String} path 
 */
function getShortFilename(path) {
  if (!isValidFileList) return path;
  for (var i = 0; i < fileJSON.length; i++)
      if (path.indexOf(fileJSON[i].name) != -1) return fileJSON[i].name;
  return path;
}

/**
 * 
 * @param {Object} debugObj is a key-value pair {filename:String, line:integer}
 * @returns {String} (filename:line)
 */
function formatDebugLocation(debugObj) {
  return "(" + getShortFilename(debugObj.filename) + ":" + debugObj.line + ")";
}