#ifndef JSK_GL_H
#define JSK_GL_H

#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>

char* read_file(const char *path)
{
    FILE* file = fopen(path, "rb");
    
    if (!file)
    {
        printf("Failed to open file: %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(size + 1);
    
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, size, file);
    buffer[size] = '\0';

    fclose(file);
    
    return buffer;
}

GLuint compile_shader(GLenum type, const GLchar* const src_string)
{
    GLuint shader_id = glCreateShader(type);
    
    glShaderSource(shader_id, 1, &src_string, NULL);
    glCompileShader(shader_id);

    GLint success;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        GLchar info[512];
        glGetShaderInfoLog(shader_id, 512, NULL, info);
	
        printf("Shader error:\n%s\n", info);
    }

    return shader_id;
}

#endif
