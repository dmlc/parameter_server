#ifndef SINGLETON_H_INCLUDED
#define SINGLETON_H_INCLUDED

#include <new>

#define DECLARE_SINGLETON(classname, ...) \
  public: \
    static classname * instance() { \
        if (sm_pInstance == NULL) { \
            sm_pInstance = new (std::nothrow) classname(__VA_ARGS__); \
        } \
        \
        return sm_pInstance; \
    } \
    \
    static bool is_created() { \
        return sm_pInstance != NULL; \
    } \
    \
  protected: \
    classname(); \
    classname(const classname &); \
    \
  protected: \
    static classname * sm_pInstance; \
    \
  private:

#define IMPLEMENT_SINGLETON(classname) \
    classname * classname::sm_pInstance = NULL; \
    classname::classname() {}

#define IMPLEMENT_SINGLETON_WITHOUT_CTOR(classname) \
    classname * classname::sm_pInstance = NULL;

#endif /* ! SINGLETON_H_INCLUDED */

// vim: ts=4 sts=4 sw=4 noet

