# DrCCTProf

## Instructions for building DrCCTProf

### Linux

To build DrCCTProf on Linux, use the following commands as a guide. This builds 64-bit DrCCTProf in release mode:

```
# Install dependencies for Ubuntu 15+.  Adjust this command as appropriate for
# other distributions (in particular, use "cmake3" for Ubuntu Trusty).
$ sudo apt-get install cmake g++ g++-multilib doxygen transfig imagemagick ghostscript git zlib1g-dev
# Then, run the build script. This will configure, make, and check DrCCTProf.
$ sh release_version_build.sh
```

## Instructions for running DrCCTProf Client

DrCCTProf provides 6 sample clients.
```
# 1. A NULL tool that does nothing.
./script/run_null.sh {app_full_path} {app_args}

# 2. A instruction counting tool that counts each instruction.
./script/run_cct_ins_counting.sh {app_full_path} {app_args}

# 3. A tool that collects call path on each instruction.
./script/run_cct_all_ins.sh {app_full_path} {app_args}

# 4.A tool that collects call path on each MEMORY instruction.
./script/run_cct_mem.sh {app_full_path} {app_args}

# 5. A tool that does Data centric on each MEMORY instruction.
./script/run_cct_mem_with_datatcentric.sh {app_full_path} {app_args}

# 6. Combination of 4 and 5.
./script/run_cct_memory_with_data_centric_with_search.sh {app_full_path} {app_args}
```
