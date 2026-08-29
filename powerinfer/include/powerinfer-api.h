#pragma once

#    if defined(_WIN32) && !defined(__MINGW32__) && defined(POWERINFER_SHARED)
#        ifdef POWERINFER_BUILD
#            define POWERINFER_API __declspec(dllexport)
#        else
#            define POWERINFER_API __declspec(dllimport)
#        endif
#    elif defined(_WIN32)
// Static (or MinGW) Windows build: no import/export attribute at all. ELF
// visibility is meaningless on PE and clang warns on it, which is fatal under
// powerinfer's -Werror.
#        define POWERINFER_API
#    else
#        define POWERINFER_API __attribute__ ((visibility ("default")))
#    endif
