#pragma once
namespace PS {

/**
 * @brief  Setups environment
 */
class Env {
 public:
  Env() { }
  ~Env() { }

  void Init(char* argv0);
 private:
  void InitGlog(char* argv0);
  void InitDMLC();
  void AssembleMyNode();

};

}  // namespace PS
