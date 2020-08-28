#include "utils.hpp"

#include <stdio.h>
#include <tl/fmt.hpp>

char* loadStr(const char* fileName)
{
    FILE* f = fopen(fileName, "r");
    if(!f) {
        tl::eprintln("error loading: ", fileName);
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    const size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* str = new char[len+1];
    fread(str, len, 1, f);
    str[len] = 0;
    fclose(f);
    return str;
}