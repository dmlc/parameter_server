#pragma once

namespace PS {

// split a std::string using a character delimiter. if skip_empty == true,
// split("one:two::three", ':'); will return 4 items

static std::vector<std::string>
split(const string &s, char delim, bool skip_empty = false) {
  std::vector<string> elems;
  std::stringstream ss(s);
  string item;
  while (std::getline(ss, item, delim))
    if (!(skip_empty && item.empty()))
      elems.push_back(item);
  return elems;
}

} // namespace PS
