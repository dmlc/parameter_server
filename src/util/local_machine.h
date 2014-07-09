#include <stdio.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include <string>

namespace PS {

// information of local machine, works on linux (probably some also work for mac)
class LocalMachine {
 public:
  // virtual memory used by my process in MB
  static double VirMem() {
    return getLine("VmSize:") / 1e3;
  }

  // physical memory used by my process in MB
  static double PhyMem() {
    return getLine("VmRSS:") / 1e3;
  }

  // return the IP address for given interface eth0, eth1, ...
  static std::string IP(const std::string& interface) {
    struct ifaddrs * ifAddrStruct = NULL;
    struct ifaddrs * ifa = NULL;
    void * tmpAddrPtr = NULL;

    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == NULL) continue;
      if (ifa ->ifa_addr->sa_family==AF_INET) {
        // is a valid IP4 Address
        tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        char addressBuffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
        if (strncmp(ifa->ifa_name,
                    interface.c_str(),
                    interface.size()) == 0) {
                    // strlen(ifa->ifa_name)) == 0) {
          return std::string(addressBuffer);
        }
      }
      // else if (ifa->ifa_addr->sa_family==AF_INET6) { // check it is IP6
      //     // is a valid IP6 Address
      //     tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
      //     char addressBuffer[INET6_ADDRSTRLEN];
      //     inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
      //     printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
      // }
    }
    if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);
    return std::string();
  }

 private:
  static double getLine(const char *name) {
    FILE* file = fopen("/proc/self/status", "r");
    char line[128];
    int result = -1;
    while (fgets(line, 128, file) != NULL){
      if (strncmp(line, name, strlen(name)) == 0){
        result = parseLine(line);
        break;
      }
    }
    fclose(file);
    return result;
  }

  static int parseLine(char* line){
    int i = strlen(line);
    while (*line < '0' || *line > '9') line++;
    line[i-3] = '\0';
    i = atoi(line);
    return i;
  }
};

} // namespace PS
