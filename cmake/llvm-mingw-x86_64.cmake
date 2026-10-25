# SPDX-License-Identifier: GPL-3.0-or-later
# HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
#
# Toolchain fuer den Cross-Build Linux -> Windows x64 mit llvm-mingw.
#
# llvm-mingw und nicht die GCC-MinGW-Toolchain: das fertige Qt von qt.io gibt
# es fuer MinGW nur mit GCC 13.1 und msvcrt. Eine andere GCC bringt eine
# eigene libstdc++-6.dll mit, von der nur eine geladen werden kann, und die
# Laufzeiten passen dann nicht zusammen. Der llvm-mingw-Build von Qt nutzt UCRT
# und libc++, und libc++ ist abwaertskompatibel.
#
# Nicht direkt verwenden, sondern ueber tools/windows-cross.sh: das Skript
# laedt Qt in die passende Version und haengt diese Datei an Qts eigene
# qt.toolchain.cmake (QT_CHAINLOAD_TOOLCHAIN_FILE).

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_hrt_triple x86_64-w64-mingw32)
find_program(_hrt_cxx ${_hrt_triple}-clang++
    HINTS ENV HRT_LLVM_MINGW /opt/llvm-mingw /usr/lib/llvm-mingw
    PATH_SUFFIXES bin
    REQUIRED)
get_filename_component(_hrt_bin "${_hrt_cxx}" DIRECTORY)

set(CMAKE_C_COMPILER   "${_hrt_bin}/${_hrt_triple}-clang")
set(CMAKE_CXX_COMPILER "${_hrt_bin}/${_hrt_triple}-clang++")
set(CMAKE_RC_COMPILER  "${_hrt_bin}/${_hrt_triple}-windres")

# Programme (moc, rcc, uic) kommen vom Host, alles zum Linken vom Ziel.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
