/*******************************************************
 * Copyright (c) 2015-2019, ArrayFire
 * All rights reserved.
 *
 * This file is distributed under 3-clause BSD license.
 * The complete license agreement can be obtained at:
 * http://arrayfire.com/licenses/BSD-3-Clause
 ********************************************************/

#include <common.hpp>
#include <window.hpp>

#include <iostream>
#include <fstream>
#include <cmath>

using namespace fg;
using namespace std;

typedef struct {
    GLuint vertex;
    GLuint fragment;
} shaders_t;

GLenum FGTypeToGLenum(fg::FGType val)
{
    switch(val) {
        case FG_BYTE:           return GL_BYTE;
        case FG_UNSIGNED_BYTE:  return GL_UNSIGNED_BYTE;
        case FG_INT:            return GL_INT;
        case FG_UNSIGNED_INT:   return GL_UNSIGNED_INT;
        default:                return GL_FLOAT;
    }
}

fg::FGType GLenumToFGType(GLenum val)
{
    switch(val) {
        case GL_BYTE:           return FG_BYTE;
        case GL_UNSIGNED_BYTE:  return FG_UNSIGNED_BYTE;
        case GL_INT:            return FG_INT;
        case GL_UNSIGNED_INT:   return FG_UNSIGNED_INT;
        default:                return FG_FLOAT;
    }
}

GLenum FGModeToGLColor(ColorMode mode)
{
    GLenum color = GL_RGBA;
    switch(mode) {
        case FG_RED : color = GL_RED;  break;
        case FG_RGB : color = GL_RGB;  break;
        default     : color = GL_RGBA; break;
    }
    return color;
}

ColorMode GLModeToFGColor(GLenum mode)
{
    ColorMode color = FG_RGBA;
    switch(mode) {
        case GL_RED : color = FG_RED;  break;
        case GL_RGB : color = FG_RGB;  break;
        default     : color = FG_RGBA; break;
    }
    return color;
}

char* loadFile(const char * fname, GLint &fSize)
{
    std::ifstream file(fname,std::ios::in|std::ios::binary|std::ios::ate);
    if (file.is_open())
    {
        unsigned int size = (unsigned int)file.tellg();
        fSize = size;
        char *memblock = new char [size];
        file.seekg (0, std::ios::beg);
        file.read (memblock, size);
        file.close();
        std::cerr << "file " << fname << " loaded" << std::endl;
        return memblock;
    }

    char buffer[64];
    sprintf(buffer, "Unable to open file %s", fname);

    throw fg::Error("loadFile", __LINE__, buffer, FG_ERR_GL_ERROR);
}

void printShaderInfoLog(GLint shader)
{
    int infoLogLen = 0;
    int charsWritten = 0;
    GLchar *infoLog;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);

    if (infoLogLen > 1)
    {
        infoLog = new GLchar[infoLogLen];
        glGetShaderInfoLog(shader,infoLogLen, &charsWritten, infoLog);
        std::cerr << "InfoLog:" << std::endl << infoLog << std::endl;
        delete [] infoLog;
        throw fg::Error("printShaderInfoLog", __LINE__, "OpenGL Shader compilation failed", FG_ERR_GL_ERROR);
    }
}

void printLinkInfoLog(GLint prog)
{
    int infoLogLen = 0;
    int charsWritten = 0;
    GLchar *infoLog;

    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLogLen);

    if (infoLogLen > 1)
    {
        infoLog = new GLchar[infoLogLen];
        // error check for fail to allocate memory omitted
        glGetProgramInfoLog(prog,infoLogLen, &charsWritten, infoLog);
        std::cerr << "InfoLog:" << std::endl << infoLog << std::endl;
        delete [] infoLog;
        throw fg::Error("printLinkInfoLog", __LINE__, "OpenGL Shader linking failed", FG_ERR_GL_ERROR);
    }
}

void attachAndLinkProgram(GLuint program, shaders_t shaders)
{
    glAttachShader(program, shaders.vertex);
    glAttachShader(program, shaders.fragment);

    glLinkProgram(program);
    GLint linked;
    glGetProgramiv(program,GL_LINK_STATUS, &linked);
    if (!linked)
    {
        std::cerr << "Program did not link." << std::endl;
        throw fg::Error("attachAndLinkProgram", __LINE__, "OpenGL program linking failed", FG_ERR_GL_ERROR);
    }
    printLinkInfoLog(program);
}

shaders_t loadShaders(const char * vert_code, const char * frag_code)
{
    GLuint f, v;

    v = glCreateShader(GL_VERTEX_SHADER);
    f = glCreateShader(GL_FRAGMENT_SHADER);

    // load shaders & get length of each
    glShaderSource(v, 1, &vert_code, NULL);
    glShaderSource(f, 1, &frag_code, NULL);

    GLint compiled;

    glCompileShader(v);
    glGetShaderiv(v, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        std::cerr << "Vertex shader not compiled." << std::endl;
        printShaderInfoLog(v);
    }

    glCompileShader(f);
    glGetShaderiv(f, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        std::cerr << "Fragment shader not compiled." << std::endl;
        printShaderInfoLog(f);
    }

    shaders_t out; out.vertex = v; out.fragment = f;

    return out;
}

GLuint initShaders(const char* vshader_code, const char* fshader_code)
{
    shaders_t shaders = loadShaders(vshader_code, fshader_code);
    GLuint shader_program = glCreateProgram();
    attachAndLinkProgram(shader_program, shaders);
    return shader_program;
}

int next_p2(int value)
{
    return int(std::pow(2, (std::ceil(std::log2(value)))));
}

float clampTo01(float a)
{
    return (a < 0.0f ? 0.0f : (a>1.0f ? 1.0f : a));
}

#ifdef OS_WIN
#include <strsafe.h>

void getFontFilePaths(std::vector<std::string>& pFiles, std::string pDir, std::string pExt)
{
   WIN32_FIND_DATA ffd;
   LARGE_INTEGER filesize;
   TCHAR szDir[MAX_PATH];
   size_t length_of_arg;
   DWORD dwError=0;
   HANDLE hFind = INVALID_HANDLE_VALUE;

   // Check that the input path plus 3 is not longer than MAX_PATH.
   // Three characters are for the "\*" plus NULL appended below.
   StringCchLength(pDir.c_str(), MAX_PATH, &length_of_arg);

   if (length_of_arg > (MAX_PATH - 3)) {
       throw fg::Error("getImageFilePaths", __LINE__,
           "WIN API call: Directory path is too long",
           FG_ERR_FILE_NOT_FOUND);
   }

   //printf("\nTarget directory is %s\n\n", pDir.c_str());
   // Prepare string for use with FindFile functions.  First, copy the
   // string to a buffer, then append '\*' to the directory name.
   StringCchCopy(szDir, MAX_PATH, pDir.c_str());
   std::string wildcard = "\\*" + pExt;
   StringCchCat(szDir, MAX_PATH, wildcard.c_str());

   // Find the first file in the directory.
   hFind = FindFirstFile(szDir, &ffd);
   if (INVALID_HANDLE_VALUE == hFind) {
       throw fg::Error("getImageFilePaths", __LINE__,
           "WIN API call: file fetch in DIR failed",
           FG_ERR_FILE_NOT_FOUND);
   }

   // List all the files in the directory with some info about them.
   do {
      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
         // It is a directory, skip the entry
         //_tprintf(TEXT("  %s   <DIR>\n"), ffd.cFileName);
      } else {
         filesize.LowPart = ffd.nFileSizeLow;
         filesize.HighPart = ffd.nFileSizeHigh;
         //_tprintf(TEXT("  %s   %ld bytes\n"), ffd.cFileName, filesize.QuadPart);
         pFiles.push_back(std::string(ffd.cFileName));
      }
   } while (FindNextFile(hFind, &ffd) != 0);

   dwError = GetLastError();
   if (dwError != ERROR_NO_MORE_FILES) {
       throw fg::Error("getImageFilePaths", __LINE__,
           "WIN API call: files fetch returned no files",
           FG_ERR_FILE_NOT_FOUND);
   }

   FindClose(hFind);
}
#endif
