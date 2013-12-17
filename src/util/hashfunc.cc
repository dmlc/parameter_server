#include "hashfunc.h"
#include "BaseConverter.h"
#include "md5.h"
#include "MurmurHash3.h"

namespace PS {
  Key HashFunc :: MMToKeyRange(Key min_key, Key max_key, const std::string& str) {
    uint64_t* res = new uint64_t[2];
    MurmurHash3_x64_128(str.c_str(), str.length(), 0, res);
    string MMstr_dec = strfy(res[0]) + strfy(res[1]);
    pair<string, Key> res_pair = div_mod(MMstr_dec, (max_key - min_key));
    Key r_key = min_key + res_pair.second;
    
    return r_key;
  }
  Key HashFunc :: MD5ToKeyRange(Key min_key, Key max_key, const std::string& str) {
    //MD5 md5_obj(str);
    string md5str_hex = md5(str);//md5_obj.hexdigest();
    strToUpper(md5str_hex);
    const BaseConverter& hex2dec = BaseConverter::HexToDecimalConverter();
    string md5str_dec = hex2dec.Convert(md5str_hex);
    pair<string, Key> res_pair = div_mod(md5str_dec, (max_key - min_key));
    Key r_key = min_key + res_pair.second;
    
    return r_key;
}

  Key HashFunc :: RandToKeyRange(Key min_key, Key max_key) {
    double r = rand() / (double) RAND_MAX;
    Key r_key = min_key + r * (max_key - min_key);
    
    return r_key;
  }

  Key HashFunc :: AverageToKeyRange(const Key min_key, const Key max_key, const size_t num, const size_t i) {
    size_t interval = (max_key - min_key) / num;

    Key r_key = min_key + i * interval;

    return r_key;
  }
}
