//===----------------------------- editor.js -------------------------------===//
//
// Add files to dropdown, highlight and remove highlight line for ACE Editor.
//
//===----------------------------------------------------------------------===//

"use strict";

// Add files to drop down above the editor pane
// Only call this if fileJSON is valid and editor is enabled
function addFileTabs( fileJSON ) {
  var fileNavDropDown = document.getElementById("editor-nav-button");  // Dropdown element ID
  // Converting fileJSON list into dropdown form
  // Native support from bootstrap for dropdown form and different file editor toggle
  Object.keys(fileJSON).forEach( function( filekey, fileIndex ) {
    let fileObj = fileJSON[filekey];
    let dropDownItem = document.createElement('option');
    dropDownItem.className = "dropdown-item";
    dropDownItem.value = "#file" + fileIndex;  // use value attribute selection
    dropDownItem.innerHTML = fileObj.name;
    dropDownItem.title = fileObj.absName;  // full path when mousehover
    dropDownItem.setAttribute("data-toggle", "collapse");
    dropDownItem.setAttribute("role", "button");
    dropDownItem.setAttribute("aria-expanded", "false");
    dropDownItem.setAttribute("aria-controls", "collapseExample");
    // dropDownItem.setAttribute("data-target", "#file" + fileIndex);  // use for toggling tab-content item
    if(fileObj.has_active_debug_locs) {
      dropDownItem.className += " has_active_debug_locs";
    }
    fileNavDropDown.appendChild(dropDownItem);

    fileIndexMap[fileObj.path] = fileIndex;  // add to index map for faster toggling of tab content
  });

  let editorContent = document.getElementById("editor-pane-content");

  Object.keys(fileJSON).forEach( function( filekey, fileIndex ) {
    let fileObj = fileJSON[filekey];
    let fileContent = document.createElement("div");
    fileContent.id = "file" + fileIndex;
    fileContent.className = "collapse tab-pane border";
    if (fileIndex === 0) { fileContent.className += " active"; }
    let editorDiv = document.createElement("div");
    editorDiv.className = "well";

    fileContent.appendChild(editorDiv);
    SetupEditor(fileObj, editorDiv);

    editorContent.appendChild(fileContent);

    function SetupEditor(fileInfo, editorDiv) {
      let editor = ace.edit( editorDiv );
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
  });
}

// Make the editor navigation dropdown + exit same width as the editor
function adjustEditorButtons() {
    var editorWidth = $("#editor-pane").width();
    var editorExitButton = $("#close-source").outerWidth(true);
    $("#editor-nav-button").css("width", editorWidth - editorExitButton - 1);
}

function gotoFileACEEditorLine ( filename, line ) {
    let editor;
    let index = 0;

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

    if (!editor || line < 1){
      console.warn("Warning! Invalid editor or line number:" + line );
      return;
    }

    if (index !== previousFilehash) {
      $(previousFilehash).removeClass("active");
      $('#file' + index).addClass("active");
      previousFilehash = '#file' + index;
    }
    $('#editor-nav-button')[0].value = '#file' + index;  // selection is done by value describe addFileTabs()
    editor.focus();
    editor.resize(true);
    editor.scrollToLine( line, true, true, function() {} );

    // Enable line highlighting and cursor
    editor.setHighlightActiveLine(true);
    editor.setHighlightGutterLine(true);
    editor.renderer.$cursorLayer.element.style.opacity = 1;

    editor.gotoLine( line );
}

function removeACEEditorHighlight() {
    // Get current editor, and disable line highlighting and cursor.
    var editor = ace.edit($("#editor-pane-content div.active div.well")[0]);
    editor.setHighlightActiveLine(false);
    editor.setHighlightGutterLine(false);
    editor.renderer.$cursorLayer.element.style.opacity = 0;
}