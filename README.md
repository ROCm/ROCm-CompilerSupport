# RocmCompilerSupport has moved!

This project is now located in the
[AMD Fork of the LLVM Project](https://github.com/ROCm/llvm-project), under the
"amd/comgr" directory. This repository is now read-only.

All issues and pull requests related to Comgr should be filed at
https://github.com/ROCm/llvm-project with the `comgr` tag.

Users wishing to build Comgr against upstream LLVM without needing to clone the
entire ROCm llvm-project fork can still do so as follows:

    cd <upstream-llvm-project>
    git remote add rocm-llvm https://github.com/ROCm/llvm-project.git
    git fetch rocm-llvm <branch> (default branch is amd-staging)
    git checkout rocm-llvm/<branch> -- amd (default branch is amd-staging)

The amd-specific projects, including comgr, hipcc, and device-libs, will now be
present in the `<upstream llvm-project>/amd` directory.
