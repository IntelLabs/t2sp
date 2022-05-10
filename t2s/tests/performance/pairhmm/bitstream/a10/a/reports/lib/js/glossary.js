/* Glossary.js contains a look up function for the brief explanation of
   terminologies use in the tool. */

function lookUpTerminology (word, hasError) {
  // Define all terminologies in a dictionary
  // title: The bold name appear on the top of for tooltip. This can be a longer name instead of any abbreivations used in the menu
  // text : The html text show when the data exist
  // secondText: Use when the data is not available or error handling
  let glossaryDict = {
    SUMMARY: {
      "title": "Report Summary",
      "text":  "A quick overview of the results from compiling your design, including compile information and warnings, and estimated FPGA resource usage.",
      "secondText": "Report summary is not avaliable."
    },
    OPT: {
      "title": "Loops Analysis",
      "text":  "Information such as pipelined and initiation interval.",
      "secondText": "Loop analysis data is not avaliable."
    },
    FMAX_II: {
      "title": "f<sub>MAX</sub> & Initiation Interval Report",
      "text":  "Performance metrics on all blocks such as target II, scheduled f<sub>MAX</sub>, block II , latency, and maximum interleaving iterations.",
      "secondText": "Fmax II report data is not avaliable."
    },
    VERIF: {
      "title": "Verification Statistics",
      "text":  "Information such as the number and type of invocations, latency, initiation interval, and throughput.",
      "secondText": "Unavailable. Run the verification testbench to enable."
    },
    AREA_SYS: {
      "title": "Area Analysis of System",
      "text": "Resource utilization estimates organized by implemented hardware hierarchies in the FPGA.",
      "secondText": "Area Analysis of System report data is not avaliable."
    },
    AREA_SRC: {
      "title": "Area Analysis of Source (deprecated)",
      "text":  "Area Analysis by Source is deprecated. Use Area Analysis of System instead.",
      "secondText": "Area Analysis by Source report data is not avaliable."
    },
    INCREMENTAL: {
      "title": "Incremental Compile",
      "text":  "Provides percentage of design not preserved metric on your OpenCL design. New resource usage estimate in new design for standard compile.",
      "secondText": "Incremental compile data is unavailable."
    },
    GV: {
      "title": "Graph Viewer (beta)",
      "text":  "Provides views of different levels of your design: system, " + PREFIX_PAD + ", block, and cluster.",
      "secondText": "Graph Viewer data is not avaliable."
    },
    LMEM : {
      "title": PREFIX_PAD + " Memory Viewer",
      "text": "Shows memory system connections including those between loads and stores specific to logical ports on the memory banks. You can also view replicate nodes for each memory bank.",
      "secondText": PREFIX_PAD + " Memory Viewer data is not avaliable."
    },
    SCHEDULE: {
      "title": "Schedule Viewer (alpha)",
      "text":  "Shows the static scheduled cycle and latency of a clustered group of instructions in your design.",
      "secondText": "Schedule Viewer data is not avaliable."
    }
  };

  if (glossaryDict.hasOwnProperty(word)) {
    if (glossaryDict[word].hasOwnProperty("title") && glossaryDict[word].hasOwnProperty("text") &&
        glossaryDict[word].hasOwnProperty("secondText")) {
      let titleHtml = "<b>" + glossaryDict[word].title.replace(PREFIX_PAD, function_prefix) + "</b><br>";
      if (hasError) {
        return titleHtml + glossaryDict[word]["secondText"].replace(PREFIX_PAD, function_prefix);
      }
      return titleHtml + glossaryDict[word]["text"].replace(PREFIX_PAD, function_prefix);
    }
    console.log("Warning! Incomplete information for on " + word);
    return "";
  }
  console.log("Warning! No information for on " + word);
  return "";
}