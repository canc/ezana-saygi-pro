#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace adhan {

class Json {
 public:
  enum Type { NIL, BOOL, NUMBER, STRING, ARRAY, OBJECT };

  Json() : type_(NIL), b_(false), n_(0) {}
  static Json null() { return Json(); }
  static Json boolean(bool v) {
    Json j;
    j.type_ = BOOL;
    j.b_ = v;
    return j;
  }
  static Json number(double v) {
    Json j;
    j.type_ = NUMBER;
    j.n_ = v;
    return j;
  }
  static Json number(int v) { return number(static_cast<double>(v)); }
  static Json number(int64_t v) { return number(static_cast<double>(v)); }
  static Json string(const std::string& v) {
    Json j;
    j.type_ = STRING;
    j.s_ = v;
    return j;
  }
  static Json object() {
    Json j;
    j.type_ = OBJECT;
    return j;
  }
  static Json array() {
    Json j;
    j.type_ = ARRAY;
    return j;
  }

  Type type() const { return type_; }
  bool is_null() const { return type_ == NIL; }
  bool is_object() const { return type_ == OBJECT; }
  bool is_string() const { return type_ == STRING; }
  bool is_number() const { return type_ == NUMBER; }
  bool is_bool() const { return type_ == BOOL; }
  bool is_array() const { return type_ == ARRAY; }

  bool as_bool(bool def = false) const { return type_ == BOOL ? b_ : def; }
  double as_number(double def = 0) const { return type_ == NUMBER ? n_ : def; }
  int as_int(int def = 0) const { return type_ == NUMBER ? static_cast<int>(n_) : def; }
  int64_t as_int64(int64_t def = 0) const { return type_ == NUMBER ? static_cast<int64_t>(n_) : def; }
  const std::string& as_string() const { return s_; }
  std::string as_string(const std::string& def) const { return type_ == STRING ? s_ : def; }

  Json& operator[](const std::string& key) {
    type_ = OBJECT;
    return o_[key];
  }
  const Json& get(const std::string& key) const {
    static Json kNull;
    std::map<std::string, Json>::const_iterator it = o_.find(key);
    if (it == o_.end()) return kNull;
    return it->second;
  }
  bool has(const std::string& key) const { return o_.find(key) != o_.end(); }
  const std::map<std::string, Json>& object_items() const { return o_; }

  void push(const Json& v) {
    type_ = ARRAY;
    a_.push_back(v);
  }
  size_t size() const { return type_ == ARRAY ? a_.size() : 0; }
  const Json& at(size_t i) const {
    static Json kNull;
    if (type_ != ARRAY || i >= a_.size()) return kNull;
    return a_[i];
  }

  static bool parse(const std::string& text, Json* out, std::string* err);
  std::string stringify(bool pretty = false) const;

 private:
  Type type_;
  bool b_;
  double n_;
  std::string s_;
  std::map<std::string, Json> o_;
  std::vector<Json> a_;

  void stringify_into(std::string* out, bool pretty, int indent) const;
};

}  // namespace adhan
