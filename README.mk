- =proto=: protobuf classes
- =system=: core system components
- =box=: global shared data structures, such as (sparse) vectors and matrices
- =app=: applications
- =data=: convert data
- =util=: some utility classes

** debug

- enable core dump:
=ulimit -c unlimited=
- set core file pattern:

linux:
echo "core_%e_%p" | sudo tee /proc/sys/kernel/core_pattern

max os x:
https://developer.apple.com/library/mac/technotes/tn2124/_index.html

sudo chmod 1775 /cores
echo "limit core unlimited" | sudo tee -a /etc/launchd.conf
