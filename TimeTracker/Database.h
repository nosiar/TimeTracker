#pragma once

#include "sqlite3.h"
#include <vector>
#include <map>

class Reader
{
    friend class Database;
public:
    bool read() const;
    std::string get(const char* column) const;
    void clear();
    int size() const { return size_; }
private:
    int size_;
    mutable int cur_;
    std::map<std::string, std::vector<std::string>> reader;
};

class Database
{
public:
    Database();
    void open();
    void close();
    void create_table();
    void insert(const char* name, const char* subname, const char* datetime, int duration);
    void update(const char* name, const char* subname, const char* datetime, int add_duration);
    const Reader& select(const char* name, const char* subname, const char* time);
private:
    void exec(const char* sql);
    static int Database::callback(void *, int argc, char**argv, char**azColName);
    sqlite3* sqlite_db;
    bool is_open;
    int last_error;
    char* last_error_msg;
    Reader reader;
};