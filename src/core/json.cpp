#include "json.h"

#include <cctype>
#include <cstdio>
#include <sstream>
#include <cstdint>

namespace adhan {
namespace {

struct Parser {
  const char* s;
  size_t n;
  size_t i;
  std::string err;

  explicit Parser(const std::string& t) : s(t.c_str()), n(t.size()), i(0) {}

  void skip() {
    while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
  }

  bool fail(const char* m) {
    err = m;
    return false;
  }

  bool parse_value(Json* out) {
    skip();
    if (i >= n) return fail("unexpected end");
    char c = s[i];
    if (c == '{') return parse_object(out);
    if (c == '[') return parse_array(out);
    if (c == '"') {
      std::string str;
      if (!parse_string(&str)) return false;
      *out = Json::string(str);
      return true;
    }
    if (c == 't' || c == 'f') return parse_bool(out);
    if (c == 'n') return parse_null(out);
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
    return fail("unexpected token");
  }

  bool parse_null(Json* out) {
    if (i + 4 <= n && std::string(s + i, 4) == "null") {
      i += 4;
      *out = Json::null();
      return true;
    }
    return fail("invalid null");
  }

  bool parse_bool(Json* out) {
    if (i + 4 <= n && std::string(s + i, 4) == "true") {
      i += 4;
      *out = Json::boolean(true);
      return true;
    }
    if (i + 5 <= n && std::string(s + i, 5) == "false") {
      i += 5;
      *out = Json::boolean(false);
      return true;
    }
    return fail("invalid bool");
  }

  bool parse_number(Json* out) {
    size_t start = i;
    if (s[i] == '-') ++i;
    if (i >= n || !std::isdigit(static_cast<unsigned char>(s[i]))) return fail("invalid number");
    while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    if (i < n && s[i] == '.') {
      ++i;
      while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
      ++i;
      if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
      while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }
    double v = 0;
    std::sscanf(std::string(s + start, i - start).c_str(), "%lf", &v);
    *out = Json::number(v);
    return true;
  }

  bool parse_string(std::string* out) {
    if (i >= n || s[i] != '"') return fail("expected string");
    ++i;
    out->clear();
    while (i < n) {
      char c = s[i++];
      if (c == '"') return true;
      if (c == '\\') {
        if (i >= n) return fail("bad escape");
        char e = s[i++];
        switch (e) {
          case '"':
          case '\\':
          case '/':
            out->push_back(e);
            break;
          case 'b':
            out->push_back('\b');
            break;
          case 'f':
            out->push_back('\f');
            break;
          case 'n':
            out->push_back('\n');
            break;
          case 'r':
            out->push_back('\r');
            break;
          case 't':
            out->push_back('\t');
            break;
          case 'u': {
            if (i + 4 > n) return fail("bad unicode escape");
            unsigned code = 0;
            for (int k = 0; k < 4; ++k) {
              char h = s[i++];
              code <<= 4;
              if (h >= '0' && h <= '9')
                code += h - '0';
              else if (h >= 'a' && h <= 'f')
                code += h - 'a' + 10;
              else if (h >= 'A' && h <= 'F')
                code += h - 'A' + 10;
              else
                return fail("bad hex");
            }
            if (code < 0x80) {
              out->push_back(static_cast<char>(code));
            } else if (code < 0x800) {
              out->push_back(static_cast<char>(0xC0 | (code >> 6)));
              out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
              out->push_back(static_cast<char>(0xE0 | (code >> 12)));
              out->push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
              out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            break;
          }
          default:
            return fail("bad escape");
        }
      } else {
        out->push_back(c);
      }
    }
    return fail("unterminated string");
  }

  bool parse_object(Json* out) {
    if (s[i] != '{') return fail("expected {");
    ++i;
    *out = Json::object();
    skip();
    if (i < n && s[i] == '}') {
      ++i;
      return true;
    }
    for (;;) {
      skip();
      std::string key;
      if (!parse_string(&key)) return false;
      skip();
      if (i >= n || s[i] != ':') return fail("expected :");
      ++i;
      Json val;
      if (!parse_value(&val)) return false;
      (*out)[key] = val;
      skip();
      if (i < n && s[i] == ',') {
        ++i;
        continue;
      }
      if (i < n && s[i] == '}') {
        ++i;
        return true;
      }
      return fail("expected } or ,");
    }
  }

  bool parse_array(Json* out) {
    if (s[i] != '[') return fail("expected [");
    ++i;
    *out = Json::array();
    skip();
    if (i < n && s[i] == ']') {
      ++i;
      return true;
    }
    for (;;) {
      Json val;
      if (!parse_value(&val)) return false;
      out->push(val);
      skip();
      if (i < n && s[i] == ',') {
        ++i;
        continue;
      }
      if (i < n && s[i] == ']') {
        ++i;
        return true;
      }
      return fail("expected ] or ,");
    }
  }
};

void append_indent(std::string* out, int indent) {
  out->append(indent, ' ');
}

}  // namespace

bool Json::parse(const std::string& text, Json* out, std::string* err) {
  Parser p(text);
  Json v;
  if (!p.parse_value(&v)) {
    if (err) *err = p.err;
    return false;
  }
  p.skip();
  if (p.i != p.n) {
    // trailing whitespace already skipped; leftover is error
    if (p.i < p.n) {
      if (err) *err = "trailing data";
      return false;
    }
  }
  *out = v;
  return true;
}

void Json::stringify_into(std::string* out, bool pretty, int indent) const {
  switch (type_) {
    case NIL:
      out->append("null");
      break;
    case BOOL:
      out->append(b_ ? "true" : "false");
      break;
    case NUMBER: {
      char buf[64];
      if (n_ == static_cast<double>(static_cast<int64_t>(n_))) {
        std::snprintf(buf, sizeof(buf), "%.0f", n_);
      } else {
        std::snprintf(buf, sizeof(buf), "%.6g", n_);
      }
      out->append(buf);
      break;
    }
    case STRING: {
      out->push_back('"');
      for (size_t i = 0; i < s_.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s_[i]);
        switch (c) {
          case '"':
            out->append("\\\"");
            break;
          case '\\':
            out->append("\\\\");
            break;
          case '\n':
            out->append("\\n");
            break;
          case '\r':
            out->append("\\r");
            break;
          case '\t':
            out->append("\\t");
            break;
          default:
            if (c < 0x20) {
              char b[8];
              std::snprintf(b, sizeof(b), "\\u%04x", c);
              out->append(b);
            } else {
              out->push_back(static_cast<char>(c));
            }
        }
      }
      out->push_back('"');
      break;
    }
    case ARRAY: {
      out->push_back('[');
      for (size_t i = 0; i < a_.size(); ++i) {
        if (pretty) {
          out->push_back('\n');
          append_indent(out, indent + 2);
        }
        a_[i].stringify_into(out, pretty, indent + 2);
        if (i + 1 < a_.size()) out->push_back(',');
      }
      if (pretty && !a_.empty()) {
        out->push_back('\n');
        append_indent(out, indent);
      }
      out->push_back(']');
      break;
    }
    case OBJECT: {
      out->push_back('{');
      size_t k = 0;
      for (std::map<std::string, Json>::const_iterator it = o_.begin(); it != o_.end(); ++it, ++k) {
        if (pretty) {
          out->push_back('\n');
          append_indent(out, indent + 2);
        }
        Json::string(it->first).stringify_into(out, false, 0);
        out->push_back(':');
        if (pretty) out->push_back(' ');
        it->second.stringify_into(out, pretty, indent + 2);
        if (k + 1 < o_.size()) out->push_back(',');
      }
      if (pretty && !o_.empty()) {
        out->push_back('\n');
        append_indent(out, indent);
      }
      out->push_back('}');
      break;
    }
  }
}

std::string Json::stringify(bool pretty) const {
  std::string out;
  stringify_into(&out, pretty, 0);
  return out;
}

}  // namespace adhan
