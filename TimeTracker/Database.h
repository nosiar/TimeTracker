#pragma once

#include "sqlite3.h"

class Database
{
public:
    Database();
    int open(const char* file_name);
    void close();
    int create_table();
    int insert(char* name, char* datetime, int duration);

private:
    sqlite3* db;
    bool is_open;
};
