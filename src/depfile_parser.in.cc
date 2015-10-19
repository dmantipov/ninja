// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "depfile_parser.h"

// A note on backslashes in Makefiles, from reading the docs:
// Backslash-newline is the line continuation character.
// Backslash-# escapes a # (otherwise meaningful as a comment start).
// Backslash-% escapes a % (otherwise meaningful as a special).
// Finally, quoting the GNU manual, "Backslashes that are not in danger
// of quoting ‘%’ characters go unmolested."
// How do you end a line with a backslash?  The netbsd Make docs suggest
// reading the result of a shell command echoing a backslash!
//
// Rather than implement all of above, we do a simpler thing here:
// Backslashes escape a set of characters (see "escapes" defined below),
// otherwise they are passed through verbatim.
// If anyone actually has depfiles that rely on the more complicated
// behavior we can adjust this.
bool DepfileParser::Parse(string* content, string* err) {
  // in: current parser input point.
  // end: end of input.
  // parsing_targets: whether we are parsing targets or dependencies.
  char* in = &(*content)[0];
  char* end = in + content->size();
  bool have_target = false;
  bool have_secondary_target_on_this_rule = false;
  bool parsing_targets = true;
  while (in < end) {
    bool have_newline = false;
    // out: current output point (typically same as in, but can fall behind
    // as we de-escape backslashes).
    char* out = in;
    // filename: start of the current parsed filename.
    char* filename = out;
    for (;;) {
      // start: beginning of the current parsed span.
      const char* start = in;
      char* yymarker = NULL;
      /*!re2c
      re2c:define:YYCTYPE = "unsigned char";
      re2c:define:YYCURSOR = in;
      re2c:define:YYLIMIT = end;
      re2c:define:YYMARKER = yymarker;

      re2c:yyfill:enable = 0;

      re2c:indent:top = 2;
      re2c:indent:string = "  ";

      nul = "\000";
      escape = [ \\#*[|\]];
      newline = '\r'?'\n';

      '\\' escape {
        // De-escape backslashed character.
        *out++ = yych;
        continue;
      }
      '$$' {
        // De-escape dollar character.
        *out++ = '$';
        continue;
      }
      '\\' [^\000\r\n] {
        // Let backslash before other characters through verbatim.
        *out++ = '\\';
        *out++ = yych;
        continue;
      }
      [a-zA-Z0-9+,/_:.~()}{%@=!\x80-\xFF-]+ {
        // Got a span of plain text.
        int len = (int)(in - start);
        // Need to shift it over if we're overwriting backslashes.
        if (out < start)
          memmove(out, start, len);
        out += len;
        continue;
      }
      nul {
        break;
      }
      '\\' newline {
        // A line continuation ends the current file name.
        break;
      }
      newline {
        // A newline ends the current file name and the current rule.
        have_newline = true;
        break;
      }
      [^] {
        // For any other character (e.g. whitespace), swallow it here,
        // allowing the outer logic to loop around again.
        break;
      }
      */
    }

    int len = (int)(out - filename);
    const bool is_dependency = !parsing_targets;
    if (len > 0 && filename[len - 1] == ':') {
      len--;  // Strip off trailing colon, if any.
      parsing_targets = false;
      have_target = true;
    }

    if (len > 0) {
      if (is_dependency) {
        if (have_secondary_target_on_this_rule) {
          *err = "depfile has multiple output paths";
          return false;
        }
        ins_.push_back(StringPiece(filename, len));
      } else if (!out_.str_) {
        out_ = StringPiece(filename, len);
      } else if (out_ != StringPiece(filename, len)) {
        have_secondary_target_on_this_rule = true;
      }
    }

    if (have_newline) {
      // A newline ends a rule so the next filename will be a new target.
      parsing_targets = true;
      have_secondary_target_on_this_rule = false;
    }
  }
  if (!have_target) {
    *err = "expected ':' in depfile";
    return false;
  }
  return true;
}
