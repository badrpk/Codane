#pragma once
#include <map>
#include <string>
#include <variant>
#include <vector>
#include <stdexcept>
namespace codane {
class Json {
 public:
  using object=std::map<std::string,Json>; using array=std::vector<Json>;
  using value=std::variant<std::nullptr_t,bool,double,std::string,array,object>;
  Json():v(nullptr){} Json(std::nullptr_t):v(nullptr){} Json(bool x):v(x){} Json(double x):v(x){} Json(int x):v(double(x)){} Json(std::string x):v(std::move(x)){} Json(const char*x):v(std::string(x)){} Json(array x):v(std::move(x)){} Json(object x):v(std::move(x)){}
  bool is_object()const{return std::holds_alternative<object>(v);} bool is_array()const{return std::holds_alternative<array>(v);}
  const object& obj()const{return std::get<object>(v);} object& obj(){return std::get<object>(v);} const array& arr()const{return std::get<array>(v);} array& arr(){return std::get<array>(v);}
  std::string str()const{return std::get<std::string>(v);} double number()const{return std::get<double>(v);} bool boolean()const{return std::get<bool>(v);}
  bool has(const std::string&k)const{return is_object()&&obj().count(k);} const Json& at(const std::string&k)const{return obj().at(k);} Json& operator[](const std::string&k){return obj()[k];}
  std::string dump()const; static Json parse(const std::string&);
 private: value v;
};
}
